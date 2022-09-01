/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <esp_log.h>
#include <cJSON.h>
#include "golioth_coap_client.h"
#include "golioth_rpc.h"
#include "golioth_util.h"
#include "golioth_time.h"
#include "golioth_statistics.h"

#define TAG "golioth_rpc"

// Request:
//
// {
//      "id": "id_string,
//      "method": "method_name_string",
//      "params": [...]
// }
//
// Response:
//
// {
//      "id": "id_string",
//      "statusCode": integer,
//      "detail": {...}
// }

#if (CONFIG_GOLIOTH_RPC_ENABLE == 1)

#define GOLIOTH_RPC_PATH_PREFIX ".rpc/"

typedef struct {
    const char* method;
    golioth_rpc_cb_fn callback;
    void* callback_arg;
} golioth_rpc_t;

static golioth_rpc_t _rpcs[CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS];
static int _num_rpcs;

static golioth_status_t golioth_rpc_ack_internal(
        golioth_client_t client,
        const char* call_id,
        golioth_rpc_status_t status_code,
        uint8_t* detail,
        size_t detail_len) {
    char buf[256] = {};
    if (detail_len > 0) {
        snprintf(
                buf,
                sizeof(buf),
                "{ \"id\": \"%s\", \"statusCode\": %d, \"detail\": %s }",
                call_id,
                status_code,
                detail);
    } else {
        snprintf(buf, sizeof(buf), "{ \"id\": \"%s\", \"statusCode\": %d }", call_id, status_code);
    }
    return golioth_coap_client_set(
            client,
            GOLIOTH_RPC_PATH_PREFIX,
            "status",
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)buf,
            strlen(buf),
            NULL,
            NULL,
            false,
            GOLIOTH_WAIT_FOREVER);
}

static void on_rpc(
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

    ESP_LOG_BUFFER_HEXDUMP(TAG, payload, min(64, payload_size), ESP_LOG_DEBUG);

    cJSON* json = cJSON_ParseWithLength((const char*)payload, payload_size);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse rpc call");
        goto cleanup;
    }
    GSTATS_INC_ALLOC("on_rpc_json");

    const cJSON* rpc_call_id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if (!rpc_call_id || !cJSON_IsString(rpc_call_id)) {
        ESP_LOGE(TAG, "Key id not found");
        goto cleanup;
    }

    const cJSON* rpc_method = cJSON_GetObjectItemCaseSensitive(json, "method");
    if (!rpc_method || !cJSON_IsString(rpc_method)) {
        ESP_LOGE(TAG, "Key method not found");
        goto cleanup;
    }

    const cJSON* params = cJSON_GetObjectItemCaseSensitive(json, "params");
    if (!params) {
        ESP_LOGE(TAG, "Key params not found");
        goto cleanup;
    }

    uint8_t detail[64] = {};
    const char* call_id = rpc_call_id->valuestring;
    ESP_LOGD(TAG, "Calling RPC callback for call id :%s", call_id);

    bool method_found = false;
    for (int i = 0; i < _num_rpcs; i++) {
        const golioth_rpc_t* rpc = &_rpcs[i];
        if (strcmp(rpc->method, rpc_method->valuestring) == 0) {
            method_found = true;
            golioth_rpc_status_t status = rpc->callback(
                    rpc_method->valuestring,
                    params,
                    detail,
                    sizeof(detail) - 1,  // -1 to ensure it's NULL-terminated
                    rpc->callback_arg);
            ESP_LOGD(TAG, "RPC status code %d for call id :%s", status, call_id);
            golioth_rpc_ack_internal(client, call_id, status, detail, strlen((const char*)detail));
            break;
        }
    }
    if (!method_found) {
        golioth_rpc_ack_internal(client, call_id, RPC_UNAVAILABLE, detail, 0);
    }

cleanup:
    if (json) {
        cJSON_Delete(json);
        GSTATS_INC_FREE("on_rpc_json");
    }
}

golioth_status_t golioth_rpc_register(
        golioth_client_t client,
        const char* method,
        golioth_rpc_cb_fn callback,
        void* callback_arg) {
    if (_num_rpcs >= CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS) {
        ESP_LOGE(
                TAG,
                "Unable to register, can't register more than %d methods",
                CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS);
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    golioth_rpc_t* rpc = &_rpcs[_num_rpcs];

    rpc->method = method;
    rpc->callback = callback;
    rpc->callback_arg = callback_arg;

    _num_rpcs++;
    if (_num_rpcs == 1) {
        return golioth_coap_client_observe_async(
                client, GOLIOTH_RPC_PATH_PREFIX, "", COAP_MEDIATYPE_APPLICATION_JSON, on_rpc, NULL);
    }
    return GOLIOTH_OK;
}
#else  // CONFIG_GOLIOTH_RPC_ENABLE

golioth_status_t golioth_rpc_register(
        golioth_client_t client,
        const char* method,
        golioth_rpc_cb_fn callback,
        void* callback_arg) {
    return GOLIOTH_ERR_NOT_IMPLEMENTED;
}

#endif  // CONFIG_GOLIOTH_RPC_ENABLE
