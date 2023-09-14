/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "golioth_settings.h"
#include "golioth_util.h"
#include "golioth_time.h"
#include "golioth_coap_client.h"
#include "golioth_statistics.h"
#include "golioth_debug.h"
#include "zcbor_utils.h"
#include <errno.h>
#include <math.h>  // modf
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include "zcbor_any_skip_fixed.h"

// Example settings request from cloud:
//
// {
//   "version": 1652109801583 // Unix timestamp with the most recent change
//   "settings": {
//     "MOTOR_SPEED": 100,
//     "UPDATE_INTERVAL": 100,
//     "TEMPERATURE"_FORMAT": "celsius"
//   }
// }
//
// Example settings response from device:
//
// {
//   "version": 1652109801583 // timestamp, copied from settings request
//   "errors": [ // if no errors, then omit
//      { "setting_key": "string", "error_code": integer, "details": "string" },
//      ...
//   ],
// }

#if (CONFIG_GOLIOTH_SETTINGS_ENABLE == 1)

LOG_TAG_DEFINE(golioth_settings);

#define SETTINGS_PATH_PREFIX ".c/"
#define SETTINGS_STATUS_PATH "status"

#define GOLIOTH_SETTINGS_MAX_RESPONSE_LEN 256
#define GOLIOTH_SETTINGS_MAX_NAME_LEN 63 /* not including NULL */

struct settings_response {
    zcbor_state_t zse[1 /* num_backups */ + 2];
    uint8_t buf[GOLIOTH_SETTINGS_MAX_RESPONSE_LEN];
    size_t num_errors;
    golioth_client_t client;
};

static void response_init(struct settings_response* response, golioth_client_t client) {
    memset(response, 0, sizeof(*response));

    response->client = client;

    zcbor_new_encode_state(
            response->zse, ARRAY_SIZE(response->zse), response->buf, sizeof(response->buf), 1);

    /* Initialize the map */
    zcbor_map_start_encode(response->zse, ZCBOR_MAX_ELEM_COUNT);
}

static void add_error_to_response(
        struct settings_response* response,
        const char* key,
        golioth_settings_status_t code) {
    if (response->num_errors == 0) {
        zcbor_tstr_put_lit(response->zse, "errors");
        zcbor_list_start_encode(response->zse, ZCBOR_MAX_ELEM_COUNT);
    }

    zcbor_map_start_encode(response->zse, 2);

    zcbor_tstr_put_lit(response->zse, "setting_key");
    zcbor_tstr_put_term(response->zse, key);

    zcbor_tstr_put_lit(response->zse, "error_code");
    zcbor_int64_put(response->zse, code);

    zcbor_map_end_encode(response->zse, 2);

    response->num_errors++;
}

static golioth_setting_t* find_registered_setting(golioth_settings_t* gsettings, const char* key) {
    for (size_t i = 0; i < gsettings->num_settings; i++) {
        golioth_setting_t* s = &gsettings->settings[i];
        if (!s->is_valid) {
            continue;
        }
        if (strcmp(s->key, key) == 0) {
            return s;
        }
    }
    return NULL;
}

static int finalize_and_send_response(
        golioth_client_t client,
        struct settings_response* response,
        int64_t version) {
    bool ok;

    /*
     * If there were errors, then the "errors" array is still open,
     * so we need to close it.
     */
    if (response->num_errors > 0) {
        ok = zcbor_list_end_encode(response->zse, SIZE_MAX);
        if (!ok) {
            return -ENOMEM;
        }
    }

    /* Set version */
    ok = zcbor_tstr_put_lit(response->zse, "version") && zcbor_int64_put(response->zse, version);
    if (!ok) {
        return -ENOMEM;
    }

    /* Close the root map */
    ok = zcbor_map_end_encode(response->zse, 1);
    if (!ok) {
        return -ENOMEM;
    }

    GLTH_LOG_BUFFER_HEXDUMP(
            TAG,
            response->buf,
            response->zse->payload - response->buf,
            GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    golioth_coap_client_set(
            client,
            SETTINGS_PATH_PREFIX,
            "status",
            COAP_MEDIATYPE_APPLICATION_CBOR,
            response->buf,
            response->zse->payload - response->buf,
            NULL,
            NULL,
            false,
            GOLIOTH_WAIT_FOREVER);

    return 0;
}

static int settings_decode(zcbor_state_t* zsd, void* value) {
    struct settings_response* settings_response = value;
    golioth_client_t client = settings_response->client;
    struct zcbor_string label;
    golioth_settings_status_t setting_status = GOLIOTH_SETTINGS_SUCCESS;
    golioth_settings_t* gsettings = golioth_coap_client_get_settings(client);
    bool ok;

    if (zcbor_nil_expect(zsd, NULL)) {
        /* No settings are set */
        return -ENOENT;
    }

    ok = zcbor_map_start_decode(zsd);
    if (!ok) {
        GLTH_LOGW(TAG, "Did not start CBOR list correctly");
        return -EBADMSG;
    }

    while (!zcbor_list_or_map_end(zsd)) {
        /* Handle item */
        ok = zcbor_tstr_decode(zsd, &label);
        if (!ok) {
            GLTH_LOGE(TAG, "Failed to get label");
            return -EBADMSG;
        }

        char key[GOLIOTH_SETTINGS_MAX_NAME_LEN + 1] = {};

        /* Copy setting label/name and ensure it's NULL-terminated */
        memcpy(key, label.value, MIN(GOLIOTH_SETTINGS_MAX_NAME_LEN, label.len));

        bool data_type_valid = true;

        zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*zsd->payload);

        GLTH_LOGD(TAG, "key = %s, major_type = %d", key, major_type);

        const golioth_setting_t* registered_setting = find_registered_setting(gsettings, key);
        if (!registered_setting) {
            add_error_to_response(settings_response, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);

            ok = zcbor_any_skip(zsd, NULL);
            if (!ok) {
                GLTH_LOGE(TAG, "Failed to skip unsupported type");
                return -EBADMSG;
            }

            continue;
        }

        switch (major_type) {
            case ZCBOR_MAJOR_TYPE_TSTR: {
                struct zcbor_string str;

                if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_STRING) {
                    data_type_valid = false;
                    break;
                }

                ok = zcbor_tstr_decode(zsd, &str);
                if (!ok) {
                    data_type_valid = false;
                    break;
                }

                setting_status = registered_setting->string_cb(
                        (char*)str.value, str.len, registered_setting->cb_arg);
                break;
            }
            case ZCBOR_MAJOR_TYPE_PINT:
            case ZCBOR_MAJOR_TYPE_NINT: {
                int64_t value;

                if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT) {
                    data_type_valid = false;
                    break;
                }

                ok = zcbor_int64_decode(zsd, &value);
                if (!ok) {
                    data_type_valid = false;
                    break;
                }

                setting_status =
                        registered_setting->int_cb((int32_t)value, registered_setting->cb_arg);
                break;
            }
            case ZCBOR_MAJOR_TYPE_SIMPLE: {
                bool value_bool;
                double value_double;

                if (zcbor_float_decode(zsd, &value_double)) {
                    if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT) {
                        data_type_valid = false;
                        break;
                    }

                    setting_status = registered_setting->float_cb(
                            (float)value_double, registered_setting->cb_arg);
                } else if (zcbor_bool_decode(zsd, &value_bool)) {
                    if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_BOOL) {
                        data_type_valid = false;
                        break;
                    }

                    setting_status =
                            registered_setting->bool_cb(value_bool, registered_setting->cb_arg);
                } else {
                    break;
                }
                break;
            }
            default:
                data_type_valid = false;
                break;
        }

        if (data_type_valid) {
            if (setting_status != GOLIOTH_SETTINGS_SUCCESS) {
                add_error_to_response(settings_response, key, setting_status);
            }
        } else {
            add_error_to_response(settings_response, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);

            ok = zcbor_any_skip(zsd, NULL);
            if (!ok) {
                GLTH_LOGE(TAG, "Failed to skip unsupported type");
                return -EBADMSG;
            }
        }
    }

    ok = zcbor_map_end_decode(zsd);
    if (!ok) {
        GLTH_LOGW(TAG, "Did not end CBOR list correctly");
        return -EBADMSG;
    }

    return 0;
}

static void on_settings(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    ZCBOR_STATE_D(zsd, 2, payload, payload_size, 1);
    int64_t version;
    struct settings_response settings_response;
    struct zcbor_map_entry map_entries[] = {
            ZCBOR_TSTR_LIT_MAP_ENTRY("settings", settings_decode, &settings_response),
            ZCBOR_TSTR_LIT_MAP_ENTRY("version", zcbor_map_int64_decode, &version),
    };
    int err;

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, payload_size, GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    if (payload_size == 3 && payload[1] == 'O' && payload[2] == 'K') {
        /* Ignore "OK" response received after observing */
        return;
    }

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, min(64, payload_size), GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    response_init(&settings_response, client);

    err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
    if (err) {
        if (err != -ENOENT) {
            GLTH_LOGE(TAG, "Failed to parse tstr map");
        }
        return;
    }

    finalize_and_send_response(client, &settings_response, version);
}

static void settings_lazy_init(golioth_client_t client) {
    golioth_settings_t* gsettings = golioth_coap_client_get_settings(client);

    if (gsettings->initialized) {
        return;
    }

    golioth_status_t status = golioth_coap_client_observe_async(
            client, SETTINGS_PATH_PREFIX, "", COAP_MEDIATYPE_APPLICATION_CBOR, on_settings, NULL);

    if (status != GOLIOTH_OK) {
        GLTH_LOGE(TAG, "Failed to observe settings");
        return;
    }

    gsettings->initialized = true;
}

golioth_setting_t* alloc_setting(golioth_client_t client) {
    settings_lazy_init(client);

    golioth_settings_t* gsettings = golioth_coap_client_get_settings(client);

    if (gsettings->num_settings == CONFIG_GOLIOTH_MAX_NUM_SETTINGS) {
        GLTH_LOGE(
                TAG,
                "Exceededed CONFIG_GOLIOTH_MAX_NUM_SETTINGS (%d)",
                CONFIG_GOLIOTH_MAX_NUM_SETTINGS);
        return NULL;
    }

    return &gsettings->settings[gsettings->num_settings++];
}

golioth_status_t golioth_settings_register_int(
        golioth_client_t client,
        const char* setting_name,
        golioth_int_setting_cb callback,
        void* callback_arg) {
    return golioth_settings_register_int_with_range(
            client, setting_name, INT32_MIN, INT32_MAX, callback, callback_arg);
}

golioth_status_t golioth_settings_register_int_with_range(
        golioth_client_t client,
        const char* setting_name,
        int32_t min_val,
        int32_t max_val,
        golioth_int_setting_cb callback,
        void* callback_arg) {
    if (!callback) {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    golioth_setting_t* new_setting = alloc_setting(client);
    if (!new_setting) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    new_setting->is_valid = true;
    new_setting->key = setting_name;
    new_setting->type = GOLIOTH_SETTINGS_VALUE_TYPE_INT;
    new_setting->int_cb = callback;
    new_setting->int_min_val = min_val;
    new_setting->int_max_val = max_val;
    new_setting->cb_arg = callback_arg;

    return GOLIOTH_OK;
}

golioth_status_t golioth_settings_register_bool(
        golioth_client_t client,
        const char* setting_name,
        golioth_bool_setting_cb callback,
        void* callback_arg) {
    if (!callback) {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    golioth_setting_t* new_setting = alloc_setting(client);
    if (!new_setting) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    new_setting->is_valid = true;
    new_setting->key = setting_name;
    new_setting->type = GOLIOTH_SETTINGS_VALUE_TYPE_BOOL;
    new_setting->bool_cb = callback;
    new_setting->cb_arg = callback_arg;

    return GOLIOTH_OK;
}

golioth_status_t golioth_settings_register_float(
        golioth_client_t client,
        const char* setting_name,
        golioth_float_setting_cb callback,
        void* callback_arg) {
    if (!callback) {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    golioth_setting_t* new_setting = alloc_setting(client);
    if (!new_setting) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    new_setting->is_valid = true;
    new_setting->key = setting_name;
    new_setting->type = GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT;
    new_setting->float_cb = callback;
    new_setting->cb_arg = callback_arg;

    return GOLIOTH_OK;
}

#else  // CONFIG_GOLIOTH_SETTINGS_ENABLE

golioth_status_t golioth_settings_register_int(
        golioth_client_t client,
        const char* setting_name,
        golioth_int_setting_cb callback,
        void* callback_arg) {
    return GOLIOTH_ERR_NOT_IMPLEMENTED;
}

golioth_status_t golioth_settings_register_int_with_range(
        golioth_client_t client,
        const char* setting_name,
        int32_t min_val,
        int32_t max_val,
        golioth_int_setting_cb callback,
        void* callback_arg) {
    return GOLIOTH_ERR_NOT_IMPLEMENTED;
}

golioth_status_t golioth_settings_register_bool(
        golioth_client_t client,
        const char* setting_name,
        golioth_bool_setting_cb callback,
        void* callback_arg) {
    return GOLIOTH_ERR_NOT_IMPLEMENTED;
}

golioth_status_t golioth_settings_register_float(
        golioth_client_t client,
        const char* setting_name,
        golioth_float_setting_cb callback,
        void* callback_arg) {
    return GOLIOTH_ERR_NOT_IMPLEMENTED;
}

#endif  // CONFIG_GOLIOTH_SETTINGS_ENABLE
