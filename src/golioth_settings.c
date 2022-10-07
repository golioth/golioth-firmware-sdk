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

#define TAG "golioth_settings"

#define SETTINGS_PATH_PREFIX ".c/"
#define SETTINGS_STATUS_PATH "status"

static void add_error_to_array(cJSON* array, const char* key, golioth_settings_status_t code) {
    cJSON* error = cJSON_CreateObject();
    cJSON_AddStringToObject(error, "setting_key", key);
    cJSON_AddNumberToObject(error, "error_code", (double)code);
    cJSON_AddItemToArray(array, error);
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

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, min(64, payload_size), GLTH_LOG_DEBUG);

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
    assert(gsettings->callback);

    // Iterate over settings object and call callback for each setting
    cJSON* setting = settings->child;
    while (setting) {
        const char* key = setting->string;
        golioth_settings_value_t value = {};

        if (cJSON_IsString(setting)) {
            value.type = GOLIOTH_SETTINGS_VALUE_TYPE_STRING;
            value.string.ptr = setting->valuestring;
            value.string.len = strlen(setting->valuestring);
        } else if (cJSON_IsBool(setting)) {
            value.type = GOLIOTH_SETTINGS_VALUE_TYPE_BOOL;
            value.b = setting->valueint;
        } else if (cJSON_IsNumber(setting)) {
            // Use modf to determine if this number is an int or a float
            // TODO - is there a more efficient way to do this?
            bool is_float = false;
            double int_part = 0;
            double fractional_part = modf(setting->valuedouble, &int_part);
            const double epsilon = 0.000001f;
            if (fractional_part > epsilon) {
                is_float = true;
            }

            if (is_float) {
                value.type = GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT;
                value.f = setting->valuedouble;
            } else {
                value.type = GOLIOTH_SETTINGS_VALUE_TYPE_INT;
                value.i32 = setting->valueint;
            }
        } else {
            value.type = GOLIOTH_SETTINGS_VALUE_TYPE_UNKNOWN;
            GLTH_LOGW(TAG, "Setting with key %s has unknown type", key);
            add_error_to_array(errors, key, GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID);
            goto next_setting;
        }

        golioth_settings_status_t setting_status = gsettings->callback(key, &value);
        if (setting_status != GOLIOTH_SETTINGS_SUCCESS) {
            add_error_to_array(errors, key, setting_status);
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
    free(json_string);
    cJSON_Delete(report);

cleanup:
    if (json) {
        cJSON_Delete(json);
        GSTATS_INC_FREE("on_settings_json");
    }
}

golioth_status_t golioth_settings_register_callback(
        golioth_client_t client,
        golioth_settings_cb callback) {
    if (!callback) {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    golioth_settings_t* settings = golioth_coap_client_get_settings(client);
    settings->callback = callback;

    return golioth_coap_client_observe_async(
            client, SETTINGS_PATH_PREFIX, "", COAP_MEDIATYPE_APPLICATION_JSON, on_settings, NULL);
}

#else  // CONFIG_GOLIOTH_SETTINGS_ENABLE

golioth_status_t golioth_settings_register_callback(
        golioth_client_t client,
        golioth_settings_cb callback) {
    return GOLIOTH_ERR_NOT_IMPLEMENTED;
}

#endif  // CONFIG_GOLIOTH_SETTINGS_ENABLE
