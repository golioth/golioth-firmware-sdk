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
#include <cJSON.h>
#include <math.h>  // modf

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

static void add_error_to_array(cJSON* array, const char* key, golioth_settings_status_t code) {
    cJSON* error = cJSON_CreateObject();
    cJSON_AddStringToObject(error, "setting_key", key);
    cJSON_AddNumberToObject(error, "error_code", (double)code);
    cJSON_AddItemToArray(array, error);
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

static void on_settings(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    if (payload_size == 0 || payload[0] != '{') {
        // Ignore anything that is clearly not JSON
        return;
    }

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, min(64, payload_size), GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    cJSON* json = cJSON_ParseWithLength((const char*)payload, payload_size);
    if (!json) {
        GLTH_LOGE(TAG, "Failed to parse settings");
        goto cleanup;
    }
    GSTATS_INC_ALLOC("on_settings_json");

    const cJSON* version = cJSON_GetObjectItemCaseSensitive(json, "version");
    if (!version || !cJSON_IsNumber(version)) {
        GLTH_LOGE(TAG, "Key version not found");
        goto cleanup;
    }

    const cJSON* settings = cJSON_GetObjectItemCaseSensitive(json, "settings");
    if (!settings) {
        GLTH_LOGE(TAG, "Key settings not found");
        goto cleanup;
    }

    // Create status report that we'll send back to Golioth after processing settings
    cJSON* report = cJSON_CreateObject();
    cJSON_AddNumberToObject(report, "version", version->valueint);  // copied from request
    cJSON* errors = cJSON_AddArrayToObject(report, "errors");

    golioth_settings_t* gsettings = golioth_coap_client_get_settings(client);

    // Iterate over settings object and call callback for each setting
    cJSON* setting = settings->child;
    while (setting) {
        const char* key = setting->string;

        const golioth_setting_t* registered_setting = find_registered_setting(gsettings, key);
        if (!registered_setting) {
            add_error_to_array(errors, key, GOLIOTH_SETTINGS_KEY_NOT_RECOGNIZED);
            goto next_setting;
        }

        if (cJSON_IsString(setting)) {
            if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_STRING) {
                add_error_to_array(errors, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);
                goto next_setting;
            }

            assert(registered_setting->string_cb);
            golioth_settings_status_t setting_status = registered_setting->string_cb(
                    setting->valuestring, strlen(setting->valuestring), registered_setting->cb_arg);

            if (setting_status != GOLIOTH_SETTINGS_SUCCESS) {
                add_error_to_array(errors, key, setting_status);
            }
        } else if (cJSON_IsBool(setting)) {
            if (registered_setting->type != GOLIOTH_SETTINGS_VALUE_TYPE_BOOL) {
                add_error_to_array(errors, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);
                goto next_setting;
            }

            assert(registered_setting->bool_cb);
            golioth_settings_status_t setting_status = registered_setting->bool_cb(
                    (bool)setting->valueint, registered_setting->cb_arg);

            if (setting_status != GOLIOTH_SETTINGS_SUCCESS) {
                add_error_to_array(errors, key, setting_status);
            }
        } else if (cJSON_IsNumber(setting)) {
            if (registered_setting->type == GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT) {
                assert(registered_setting->float_cb);

                golioth_settings_status_t setting_status = registered_setting->float_cb(
                        (float)setting->valuedouble, registered_setting->cb_arg);
                if (setting_status != GOLIOTH_SETTINGS_SUCCESS) {
                    add_error_to_array(errors, key, setting_status);
                }
            } else if (registered_setting->type == GOLIOTH_SETTINGS_VALUE_TYPE_INT) {
                // Use modf to determine if this number is an int or a float
                // TODO - is there a more efficient way to do this?
                double int_part = 0;
                double fractional_part = modf(setting->valuedouble, &int_part);
                const double epsilon = 0.000001f;
                if (fractional_part > epsilon) {
                    // Registered type is integer, but received float
                    add_error_to_array(errors, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);
                    goto next_setting;
                }

                // Verify min/max range
                int32_t new_value = (int32_t)setting->valueint;
                if ((new_value < registered_setting->int_min_val)
                    || (new_value > registered_setting->int_max_val)) {
                    add_error_to_array(errors, key, GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE);
                    goto next_setting;
                }

                assert(registered_setting->int_cb);
                golioth_settings_status_t setting_status = registered_setting->int_cb(
                        (int32_t)setting->valueint, registered_setting->cb_arg);

                if (setting_status != GOLIOTH_SETTINGS_SUCCESS) {
                    add_error_to_array(errors, key, setting_status);
                }
            } else {
                // Received Number, but registered type is neither integer nor float
                add_error_to_array(errors, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);
                goto next_setting;
            }
        } else {
            GLTH_LOGW(TAG, "Setting with key %s has unknown type", key);
            add_error_to_array(errors, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);
            goto next_setting;
        }

    next_setting:
        setting = setting->next;
    }

    // In case of no errors, the errors array must be omitted from the report.
    if (cJSON_GetArraySize(errors) == 0) {
        cJSON_DeleteItemFromObject(report, "errors");
    }

    // Serialize and send the status report
    char* json_string = cJSON_PrintUnformatted(report);
    GLTH_LOGD(TAG, "Sending status: %s", json_string);
    golioth_coap_client_set(
            client,
            SETTINGS_PATH_PREFIX,
            "status",
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)json_string,
            strlen(json_string),
            NULL,
            NULL,
            false,
            GOLIOTH_WAIT_FOREVER);
    golioth_sys_free(json_string);
    cJSON_Delete(report);

cleanup:
    if (json) {
        cJSON_Delete(json);
        GSTATS_INC_FREE("on_settings_json");
    }
}

static void settings_lazy_init(golioth_client_t client) {
    golioth_settings_t* gsettings = golioth_coap_client_get_settings(client);

    if (gsettings->initialized) {
        return;
    }

    golioth_status_t status = golioth_coap_client_observe_async(
            client, SETTINGS_PATH_PREFIX, "", COAP_MEDIATYPE_APPLICATION_JSON, on_settings, NULL);

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
