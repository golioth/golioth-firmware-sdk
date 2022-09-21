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
//   "error_code": 0 // Define error codes
//   "version": 1652109801583 // Report 0 for errors or ignored for errors
// }

#if (CONFIG_GOLIOTH_SETTINGS_ENABLE == 1)

#define TAG "golioth_settings"

#define SETTINGS_PATH_PREFIX ".c/"
#define SETTINGS_STATUS_PATH "status"

static struct {
    bool initialized;
    golioth_settings_cb callback;
} _golioth_settings;

static void send_status_report(
        golioth_client_t client,
        int32_t version,
        golioth_settings_status_t status) {
    cJSON* status_report = cJSON_CreateObject();
    cJSON_AddNumberToObject(status_report, "version", version);
    cJSON_AddNumberToObject(status_report, "error_code", status);
    char* json_string = cJSON_PrintUnformatted(status_report);
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
    cJSON_Delete(status_report);
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

    // Status for all settings, to be sent in report to cloud
    golioth_settings_status_t cumulative_status = GOLIOTH_SETTINGS_SUCCESS;

    // Iterate over settings object and call callback for each setting
    assert(_golioth_settings.callback);
    cJSON* setting = settings->child;
    while (setting) {
        const char* key = setting->string;
        if (strlen(key) > 15) {
            GLTH_LOGW(TAG, "Skipping setting because key too long: %s", key);
            cumulative_status = GOLIOTH_SETTINGS_KEY_NOT_VALID;
            goto next_setting;
        }

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
        }

        if (value.type != GOLIOTH_SETTINGS_VALUE_TYPE_UNKNOWN) {
            golioth_settings_status_t setting_status = _golioth_settings.callback(key, &value);
            if (setting_status != GOLIOTH_SETTINGS_SUCCESS) {
                cumulative_status = setting_status;
            }
        }

    next_setting:
        setting = setting->next;
    }

    send_status_report(client, version->valueint, cumulative_status);

cleanup:
    if (json) {
        cJSON_Delete(json);
        GSTATS_INC_FREE("on_settings_json");
    }
}

golioth_status_t golioth_settings_register_callback(
        golioth_client_t client,
        golioth_settings_cb callback) {
    if (_golioth_settings.initialized) {
        GLTH_LOGE(TAG, "Unable to register more than one callback");
        return GOLIOTH_ERR_NOT_ALLOWED;
    }

    if (!callback) {
        GLTH_LOGE(TAG, "Callback must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    _golioth_settings.initialized = true;
    _golioth_settings.callback = callback;

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
