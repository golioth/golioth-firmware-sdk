/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/settings.h>
#include "golioth_util.h"
#include "coap_client.h"
#include <golioth/golioth_debug.h>
#include <golioth/zcbor_utils.h>
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

#if defined(CONFIG_GOLIOTH_SETTINGS)

LOG_TAG_DEFINE(golioth_settings);

#define SETTINGS_PATH_PREFIX ".c/"
#define SETTINGS_STATUS_PATH "status"

#define GOLIOTH_SETTINGS_MAX_RESPONSE_LEN 256
#define GOLIOTH_SETTINGS_MAX_NAME_LEN 63 /* not including NULL */

/// Private struct for storing a single setting
struct golioth_setting
{
    bool is_valid;
    const char *key;  // aka name
    enum golioth_settings_value_type type;
    union
    {
        golioth_int_setting_cb int_cb;
        golioth_bool_setting_cb bool_cb;
        golioth_float_setting_cb float_cb;
        golioth_string_setting_cb string_cb;
    };
    int32_t int_min_val;  // applies only to integers
    int32_t int_max_val;  // applies only to integers
    void *cb_arg;
};

/// Private struct to contain settings state data
struct golioth_settings
{
    struct golioth_client *client;
    size_t num_settings;
    struct golioth_setting settings[CONFIG_GOLIOTH_MAX_NUM_SETTINGS];
};

struct settings_response
{
    zcbor_state_t zse[1 /* num_backups */ + 2];
    uint8_t buf[GOLIOTH_SETTINGS_MAX_RESPONSE_LEN];
    size_t num_errors;
    struct golioth_settings *settings;
};

static void response_init(struct settings_response *response, struct golioth_settings *settings)
{
    memset(response, 0, sizeof(*response));

    response->settings = settings;

    zcbor_new_encode_state(response->zse,
                           ARRAY_SIZE(response->zse),
                           response->buf,
                           sizeof(response->buf),
                           1);

    /* Initialize the map */
    zcbor_map_start_encode(response->zse, ZCBOR_MAX_ELEM_COUNT);
}

static void add_error_to_response(struct settings_response *response,
                                  const char *key,
                                  enum golioth_settings_status code)
{
    if (response->num_errors == 0)
    {
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

static struct golioth_setting *find_registered_setting(struct golioth_settings *gsettings,
                                                       const char *key)
{
    for (size_t i = 0; i < gsettings->num_settings; i++)
    {
        struct golioth_setting *s = &gsettings->settings[i];
        if (!s->is_valid)
        {
            continue;
        }
        if (strcmp(s->key, key) == 0)
        {
            return s;
        }
    }
    return NULL;
}

static int finalize_and_send_response(struct golioth_client *client,
                                      struct settings_response *response,
                                      int64_t version)
{
    bool ok;

    /*
     * If there were errors, then the "errors" array is still open,
     * so we need to close it.
     */
    if (response->num_errors > 0)
    {
        ok = zcbor_list_end_encode(response->zse, SIZE_MAX);
        if (!ok)
        {
            return -ENOMEM;
        }
    }

    /* Set version */
    ok = zcbor_tstr_put_lit(response->zse, "version") && zcbor_int64_put(response->zse, version);
    if (!ok)
    {
        return -ENOMEM;
    }

    /* Close the root map */
    ok = zcbor_map_end_encode(response->zse, 1);
    if (!ok)
    {
        return -ENOMEM;
    }

    GLTH_LOG_BUFFER_HEXDUMP(TAG,
                            response->buf,
                            response->zse->payload - response->buf,
                            GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    golioth_coap_client_set(client,
                            SETTINGS_PATH_PREFIX,
                            "status",
                            GOLIOTH_CONTENT_TYPE_CBOR,
                            response->buf,
                            response->zse->payload - response->buf,
                            NULL,
                            NULL,
                            false,
                            GOLIOTH_SYS_WAIT_FOREVER);

    return 0;
}

static int settings_decode(zcbor_state_t *zsd, void *value)
{
    struct settings_response *settings_response = value;
    struct golioth_settings *gsettings = settings_response->settings;
    struct zcbor_string label;
    enum golioth_settings_status setting_status = GOLIOTH_SETTINGS_SUCCESS;
    bool ok;

    if (zcbor_nil_expect(zsd, NULL))
    {
        /* No settings are set */
        return -ENOENT;
    }

    ok = zcbor_map_start_decode(zsd);
    if (!ok)
    {
        GLTH_LOGW(TAG, "Did not start CBOR list correctly");
        return -EBADMSG;
    }

    while (!zcbor_list_or_map_end(zsd))
    {
        /* Handle item */
        ok = zcbor_tstr_decode(zsd, &label);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to get label");
            return -EBADMSG;
        }

        char key[GOLIOTH_SETTINGS_MAX_NAME_LEN + 1] = {};

        /* Copy setting label/name and ensure it's NULL-terminated */
        memcpy(key, label.value, MIN(GOLIOTH_SETTINGS_MAX_NAME_LEN, label.len));

        bool data_type_valid = true;

        zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*zsd->payload);

        GLTH_LOGD(TAG, "key = %s, major_type = %d", key, major_type);

        const struct golioth_setting *registered_setting = find_registered_setting(gsettings, key);
        if (!registered_setting)
        {
            add_error_to_response(settings_response, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);

            ok = zcbor_any_skip(zsd, NULL);
            if (!ok)
            {
                GLTH_LOGE(TAG, "Failed to skip unsupported type");
                return -EBADMSG;
            }

            continue;
        }

        switch (major_type)
        {
            case ZCBOR_MAJOR_TYPE_TSTR:
            {
                struct zcbor_string str;

                if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_STRING)
                {
                    data_type_valid = false;
                    break;
                }

                ok = zcbor_tstr_decode(zsd, &str);
                if (!ok)
                {
                    data_type_valid = false;
                    break;
                }

                setting_status = registered_setting->string_cb((char *) str.value,
                                                               str.len,
                                                               registered_setting->cb_arg);
                break;
            }
            case ZCBOR_MAJOR_TYPE_PINT:
            case ZCBOR_MAJOR_TYPE_NINT:
            {
                int64_t value;

                if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT)
                {
                    data_type_valid = false;
                    break;
                }

                ok = zcbor_int64_decode(zsd, &value);
                if (!ok)
                {
                    data_type_valid = false;
                    break;
                }

                setting_status =
                    registered_setting->int_cb((int32_t) value, registered_setting->cb_arg);
                break;
            }
            case ZCBOR_MAJOR_TYPE_SIMPLE:
            {
                bool value_bool;
                double value_double;

                if (zcbor_float_decode(zsd, &value_double))
                {
                    if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT)
                    {
                        data_type_valid = false;
                        break;
                    }

                    setting_status = registered_setting->float_cb((float) value_double,
                                                                  registered_setting->cb_arg);
                }
                else if (zcbor_bool_decode(zsd, &value_bool))
                {
                    if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_BOOL)
                    {
                        data_type_valid = false;
                        break;
                    }

                    setting_status =
                        registered_setting->bool_cb(value_bool, registered_setting->cb_arg);
                }
                else
                {
                    break;
                }
                break;
            }
            default:
                data_type_valid = false;
                break;
        }

        if (data_type_valid)
        {
            if (setting_status != GOLIOTH_SETTINGS_SUCCESS)
            {
                add_error_to_response(settings_response, key, setting_status);
            }
        }
        else
        {
            add_error_to_response(settings_response, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);

            ok = zcbor_any_skip(zsd, NULL);
            if (!ok)
            {
                GLTH_LOGE(TAG, "Failed to skip unsupported type");
                return -EBADMSG;
            }
        }
    }

    ok = zcbor_map_end_decode(zsd);
    if (!ok)
    {
        GLTH_LOGW(TAG, "Did not end CBOR list correctly");
        return -EBADMSG;
    }

    return 0;
}

static void on_settings(struct golioth_client *client,
                        const struct golioth_response *response,
                        const char *path,
                        const uint8_t *payload,
                        size_t payload_size,
                        void *arg)
{
    ZCBOR_STATE_D(zsd, 2, payload, payload_size, 1);
    int64_t version;
    struct golioth_settings *settings = arg;
    struct settings_response settings_response;
    struct zcbor_map_entry map_entries[] = {
        ZCBOR_TSTR_LIT_MAP_ENTRY("settings", settings_decode, &settings_response),
        ZCBOR_TSTR_LIT_MAP_ENTRY("version", zcbor_map_int64_decode, &version),
    };
    int err;

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, payload_size, GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    if (payload_size == 3 && payload[1] == 'O' && payload[2] == 'K')
    {
        /* Ignore "OK" response received after observing */
        return;
    }

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, min(64, payload_size), GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    response_init(&settings_response, settings);

    err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
    if (err)
    {
        if (err != -ENOENT)
        {
            GLTH_LOGE(TAG, "Failed to parse tstr map");
        }
        return;
    }

    finalize_and_send_response(client, &settings_response, version);
}

static struct golioth_setting *alloc_setting(struct golioth_settings *settings)
{
    if (settings->num_settings == CONFIG_GOLIOTH_MAX_NUM_SETTINGS)
    {
        GLTH_LOGE(TAG,
                  "Exceededed CONFIG_GOLIOTH_MAX_NUM_SETTINGS (%d)",
                  CONFIG_GOLIOTH_MAX_NUM_SETTINGS);
        return NULL;
    }

    return &settings->settings[settings->num_settings++];
}

static enum golioth_status request_settings(struct golioth_settings *settings)
{
    return golioth_coap_client_get(settings->client,
                                   SETTINGS_PATH_PREFIX,
                                   "",
                                   GOLIOTH_CONTENT_TYPE_CBOR,
                                   on_settings,
                                   settings,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

struct golioth_settings *golioth_settings_init(struct golioth_client *client)
{
    struct golioth_settings *gsettings = golioth_sys_malloc(sizeof(struct golioth_settings));

    if (gsettings == NULL)
    {
        goto finish;
    }

    gsettings->client = client;
    gsettings->num_settings = 0;

    enum golioth_status status = golioth_coap_client_observe_async(client,
                                                                   SETTINGS_PATH_PREFIX,
                                                                   "",
                                                                   GOLIOTH_CONTENT_TYPE_CBOR,
                                                                   on_settings,
                                                                   gsettings);

    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Failed to observe settings");
        golioth_sys_free(gsettings);
        gsettings = NULL;
    }

finish:
    return gsettings;
}

enum golioth_status golioth_settings_register_int(struct golioth_settings *settings,
                                                  const char *setting_name,
                                                  golioth_int_setting_cb callback,
                                                  void *callback_arg)
{
    return golioth_settings_register_int_with_range(settings,
                                                    setting_name,
                                                    INT32_MIN,
                                                    INT32_MAX,
                                                    callback,
                                                    callback_arg);
}

enum golioth_status golioth_settings_register_int_with_range(struct golioth_settings *settings,
                                                             const char *setting_name,
                                                             int32_t min_val,
                                                             int32_t max_val,
                                                             golioth_int_setting_cb callback,
                                                             void *callback_arg)
{
    if (!callback)
    {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    struct golioth_setting *new_setting = alloc_setting(settings);
    if (!new_setting)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    new_setting->is_valid = true;
    new_setting->key = setting_name;
    new_setting->type = GOLIOTH_SETTINGS_VALUE_TYPE_INT;
    new_setting->int_cb = callback;
    new_setting->int_min_val = min_val;
    new_setting->int_max_val = max_val;
    new_setting->cb_arg = callback_arg;

    return request_settings(settings);
}

enum golioth_status golioth_settings_register_bool(struct golioth_settings *settings,
                                                   const char *setting_name,
                                                   golioth_bool_setting_cb callback,
                                                   void *callback_arg)
{
    if (!callback)
    {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    struct golioth_setting *new_setting = alloc_setting(settings);
    if (!new_setting)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    new_setting->is_valid = true;
    new_setting->key = setting_name;
    new_setting->type = GOLIOTH_SETTINGS_VALUE_TYPE_BOOL;
    new_setting->bool_cb = callback;
    new_setting->cb_arg = callback_arg;

    return request_settings(settings);
}

enum golioth_status golioth_settings_register_float(struct golioth_settings *settings,
                                                    const char *setting_name,
                                                    golioth_float_setting_cb callback,
                                                    void *callback_arg)
{
    if (!callback)
    {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    struct golioth_setting *new_setting = alloc_setting(settings);
    if (!new_setting)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    new_setting->is_valid = true;
    new_setting->key = setting_name;
    new_setting->type = GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT;
    new_setting->float_cb = callback;
    new_setting->cb_arg = callback_arg;

    return request_settings(settings);
}

enum golioth_status golioth_settings_register_string(struct golioth_settings *settings,
                                                     const char *setting_name,
                                                     golioth_string_setting_cb callback,
                                                     void *callback_arg)
{
    if (!callback)
    {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    struct golioth_setting *new_setting = alloc_setting(settings);
    if (!new_setting)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    new_setting->is_valid = true;
    new_setting->key = setting_name;
    new_setting->type = GOLIOTH_SETTINGS_VALUE_TYPE_STRING;
    new_setting->string_cb = callback;
    new_setting->cb_arg = callback_arg;

    return request_settings(settings);
}
#endif  // CONFIG_GOLIOTH_SETTINGS
