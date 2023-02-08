/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <cJSON.h>
#include "golioth_ota.h"
#include "golioth_coap_client.h"
#include "golioth_statistics.h"
#include "golioth_debug.h"

#define TAG "golioth_ota"

#define GOLIOTH_OTA_MANIFEST_PATH ".u/desired"
#define GOLIOTH_OTA_COMPONENT_PATH_PREFIX ".u/c/"

typedef struct {
    uint8_t* buf;
    size_t* block_nbytes;
    bool* is_last;
} block_get_output_params_t;

static golioth_ota_state_t _state = GOLIOTH_OTA_STATE_IDLE;

size_t golioth_ota_size_to_nblocks(size_t component_size) {
    size_t nblocks = component_size / GOLIOTH_OTA_BLOCKSIZE;
    if ((component_size % GOLIOTH_OTA_BLOCKSIZE) != 0) {
        nblocks++;
    }
    return nblocks;
}

const golioth_ota_component_t* golioth_ota_find_component(
        const golioth_ota_manifest_t* manifest,
        const char* package) {
    // Scan the manifest until we find the component with matching package.
    const golioth_ota_component_t* found = NULL;
    for (size_t i = 0; i < manifest->num_components; i++) {
        const golioth_ota_component_t* c = &manifest->components[i];
        bool matches = (0 == strcmp(c->package, package));
        if (matches) {
            found = c;
            break;
        }
    }
    return found;
}

golioth_status_t golioth_ota_observe_manifest_async(
        golioth_client_t client,
        golioth_get_cb_fn callback,
        void* arg) {
    return golioth_coap_client_observe_async(
            client, "", GOLIOTH_OTA_MANIFEST_PATH, COAP_MEDIATYPE_APPLICATION_JSON, callback, arg);
}

golioth_status_t golioth_ota_report_state_sync(
        golioth_client_t client,
        golioth_ota_state_t state,
        golioth_ota_reason_t reason,
        const char* package,
        const char* current_version,
        const char* target_version,
        int32_t timeout_s) {
    char jsonbuf[128] = {};
    cJSON* json = cJSON_CreateObject();
    GSTATS_INC_ALLOC("json");
    cJSON_AddNumberToObject(json, "state", state);
    cJSON_AddNumberToObject(json, "reason", reason);
    cJSON_AddStringToObject(json, "package", package);
    if (current_version) {
        cJSON_AddStringToObject(json, "version", current_version);
    }
    if (target_version) {
        cJSON_AddStringToObject(json, "target", target_version);
    }
    bool printed = cJSON_PrintPreallocated(json, jsonbuf, sizeof(jsonbuf) - 5, false);
    assert(printed);
    cJSON_Delete(json);
    GSTATS_INC_FREE("json");

    _state = state;
    return golioth_coap_client_set(
            client,
            GOLIOTH_OTA_COMPONENT_PATH_PREFIX,
            package,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)jsonbuf,
            strlen(jsonbuf),
            NULL,
            NULL,
            true,
            timeout_s);
}

golioth_status_t golioth_ota_payload_as_manifest(
        const uint8_t* payload,
        size_t payload_size,
        golioth_ota_manifest_t* manifest) {
    golioth_status_t ret = GOLIOTH_OK;
    memset(manifest, 0, sizeof(*manifest));

    cJSON* json = cJSON_ParseWithLength((const char*)payload, payload_size);
    if (!json) {
        GLTH_LOGE(TAG, "Failed to parse manifest");
        ret = GOLIOTH_ERR_INVALID_FORMAT;
        goto cleanup;
    }
    GSTATS_INC_ALLOC("json");

    const cJSON* seqnum = cJSON_GetObjectItemCaseSensitive(json, "sequenceNumber");
    if (!seqnum || !cJSON_IsNumber(seqnum)) {
        GLTH_LOGE(TAG, "Key sequenceNumber not found");
        ret = GOLIOTH_ERR_INVALID_FORMAT;
        goto cleanup;
    }
    manifest->seqnum = seqnum->valueint;

    cJSON* components = cJSON_GetObjectItemCaseSensitive(json, "components");
    cJSON* component = NULL;
    cJSON_ArrayForEach(component, components) {
        golioth_ota_component_t* c = &manifest->components[manifest->num_components++];

        const cJSON* package = cJSON_GetObjectItemCaseSensitive(component, "package");
        if (!package || !cJSON_IsString(package)) {
            GLTH_LOGE(TAG, "Key package not found");
            ret = GOLIOTH_ERR_INVALID_FORMAT;
            goto cleanup;
        }
        strncpy(c->package, package->valuestring, CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN);

        const cJSON* version = cJSON_GetObjectItemCaseSensitive(component, "version");
        if (!version || !cJSON_IsString(version)) {
            GLTH_LOGE(TAG, "Key version not found");
            ret = GOLIOTH_ERR_INVALID_FORMAT;
            goto cleanup;
        }
        strncpy(c->version, version->valuestring, CONFIG_GOLIOTH_OTA_MAX_VERSION_LEN);

        const cJSON* size = cJSON_GetObjectItemCaseSensitive(component, "size");
        if (!size || !cJSON_IsNumber(size)) {
            GLTH_LOGE(TAG, "Key size not found");
            ret = GOLIOTH_ERR_INVALID_FORMAT;
            goto cleanup;
        }
        c->size = size->valueint;
    }

cleanup:
    if (json) {
        cJSON_Delete(json);
        GSTATS_INC_FREE("json");
    }
    return ret;
}

static void on_block_rcvd(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        bool is_last,
        void* arg) {
    assert(arg);
    assert(payload_size <= GOLIOTH_OTA_BLOCKSIZE);

    if (response->status != GOLIOTH_OK) {
        return;
    }

    block_get_output_params_t* out_params = (block_get_output_params_t*)arg;
    assert(out_params->buf);
    assert(out_params->block_nbytes);

    if (out_params->is_last) {
        *out_params->is_last = is_last;
    }

    memcpy(out_params->buf, payload, payload_size);
    *out_params->block_nbytes = payload_size;
}

golioth_status_t golioth_ota_get_block_sync(
        golioth_client_t client,
        const char* package,
        const char* version,
        size_t block_index,
        uint8_t* buf,  // must be at least GOLIOTH_OTA_BLOCKSIZE bytes
        size_t* block_nbytes,
        bool* is_last,
        int32_t timeout_s) {
    char path[CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN + CONFIG_GOLIOTH_OTA_MAX_VERSION_LEN + 2] =
            {};
    snprintf(path, sizeof(path), "%s@%s", package, version);
    block_get_output_params_t out_params = {
            .buf = buf,
            .block_nbytes = block_nbytes,
            .is_last = is_last,
    };

    // TODO - use Content-Format 10742 (application/octet-stream with heatshink encoding)
    //        once it is supported by the cloud.
    //
    // Ref: https://golioth.atlassian.net/wiki/spaces/EN/pages/262275073/OTA+Compressed+Artifacts

    golioth_status_t status = GOLIOTH_OK;
    status = golioth_coap_client_get_block(
            client,
            GOLIOTH_OTA_COMPONENT_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            block_index,
            GOLIOTH_OTA_BLOCKSIZE,
            on_block_rcvd,
            &out_params,
            true,
            timeout_s);

    return status;
}

golioth_ota_state_t golioth_ota_get_state(void) {
    return _state;
}
