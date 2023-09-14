/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <string.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include "golioth_ota.h"
#include "golioth_coap_client.h"
#include "golioth_statistics.h"
#include "golioth_debug.h"
#include "golioth_util.h"
#include "zcbor_utils.h"
#include "zcbor_any_skip_fixed.h"

LOG_TAG_DEFINE(golioth_ota);

#define GOLIOTH_OTA_MANIFEST_PATH ".u/desired"
#define GOLIOTH_OTA_COMPONENT_PATH_PREFIX ".u/c/"

enum {
    MANIFEST_KEY_SEQUENCE_NUMBER = 1,
    MANIFEST_KEY_HASH = 2,
    MANIFEST_KEY_COMPONENTS = 3,
};

enum {
    COMPONENT_KEY_PACKAGE = 1,
    COMPONENT_KEY_VERSION = 2,
    COMPONENT_KEY_HASH = 3,
    COMPONENT_KEY_SIZE = 4,
    COMPONENT_KEY_URI = 5,
};

typedef struct {
    uint8_t* buf;
    size_t* block_nbytes;
    bool* is_last;
} block_get_output_params_t;

struct component_tstr_value {
    char* value;
    size_t value_len;
};

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
            client, "", GOLIOTH_OTA_MANIFEST_PATH, COAP_MEDIATYPE_APPLICATION_CBOR, callback, arg);
}

golioth_status_t golioth_ota_report_state_sync(
        golioth_client_t client,
        golioth_ota_state_t state,
        golioth_ota_reason_t reason,
        const char* package,
        const char* current_version,
        const char* target_version,
        int32_t timeout_s) {
    uint8_t encode_buf[64];
    ZCBOR_STATE_E(zse, 1, encode_buf, sizeof(encode_buf), 1);
    bool ok;

    ok = zcbor_map_start_encode(zse, 1);
    if (!ok) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(zse, "s") && zcbor_uint32_put(zse, state);
    if (!ok) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(zse, "r") && zcbor_uint32_put(zse, reason);
    if (!ok) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(zse, "pkg") && zcbor_tstr_put_term(zse, package);
    if (!ok) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    if (current_version && current_version[0] != '\0') {
        ok = zcbor_tstr_put_lit(zse, "v") && zcbor_tstr_put_term(zse, current_version);
        if (!ok) {
            return GOLIOTH_ERR_MEM_ALLOC;
        }
    }

    if (target_version && target_version[0] != '\0') {
        ok = zcbor_tstr_put_lit(zse, "t") && zcbor_tstr_put_term(zse, target_version);
        if (!ok) {
            return GOLIOTH_ERR_MEM_ALLOC;
        }
    }

    ok = zcbor_map_end_encode(zse, 1);
    if (!ok) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    _state = state;
    return golioth_coap_client_set(
            client,
            GOLIOTH_OTA_COMPONENT_PATH_PREFIX,
            package,
            COAP_MEDIATYPE_APPLICATION_CBOR,
            encode_buf,
            zse->payload - encode_buf,
            NULL,
            NULL,
            true,
            timeout_s);
}

static int component_entry_decode_value(zcbor_state_t* zsd, void* void_value) {
    struct component_tstr_value* value = void_value;
    struct zcbor_string tstr;
    bool ok;

    ok = zcbor_tstr_decode(zsd, &tstr);
    if (!ok) {
        return -EBADMSG;
    }

    if (tstr.len > value->value_len) {
        GLTH_LOGE(TAG, "Not enough space to store");
        return -ENOMEM;
    }

    memcpy(value->value, tstr.value, tstr.len);
    value->value[tstr.len] = '\0';

    return 0;
}

static int components_decode(zcbor_state_t* zsd, void* value) {
    golioth_ota_manifest_t* manifest = value;
    int err;
    bool ok;

    ok = zcbor_list_start_decode(zsd);
    if (!ok) {
        GLTH_LOGW(TAG, "Did not start CBOR list correctly");
        return -EBADMSG;
    }

    for (size_t i = 0; i < ARRAY_SIZE(manifest->components) && !zcbor_list_or_map_end(zsd); i++) {
        golioth_ota_component_t* component = &manifest->components[i];
        struct component_tstr_value package = {
                component->package,
                sizeof(component->package) - 1,
        };
        struct component_tstr_value version = {
                component->version,
                sizeof(component->version) - 1,
        };
        int64_t component_size;
        struct zcbor_map_entry map_entries[] = {
                ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_PACKAGE, component_entry_decode_value, &package),
                ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_VERSION, component_entry_decode_value, &version),
                ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_SIZE, zcbor_map_int64_decode, &component_size),
        };

        err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
        if (err) {
            GLTH_LOGE(TAG, "Failed to decode component number %zu", i);
            return err;
        }

        component->size = component_size;
        component->is_compressed =
                (CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_HEATSHRINK
                 || CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_ZLIB);

        manifest->num_components++;
    }

    ok = zcbor_list_end_decode(zsd);
    if (!ok) {
        GLTH_LOGW(TAG, "Did not end CBOR list correctly");
        return -EBADMSG;
    }

    return 0;
}

golioth_status_t golioth_ota_payload_as_manifest(
        const uint8_t* payload,
        size_t payload_size,
        golioth_ota_manifest_t* manifest) {
    ZCBOR_STATE_D(zsd, 3, payload, payload_size, 1);
    int64_t manifest_sequence_number;
    struct zcbor_map_entry map_entries[] = {
            ZCBOR_U32_MAP_ENTRY(
                    MANIFEST_KEY_SEQUENCE_NUMBER,
                    zcbor_map_int64_decode,
                    &manifest_sequence_number),
            ZCBOR_U32_MAP_ENTRY(MANIFEST_KEY_COMPONENTS, components_decode, manifest),
    };
    int err;

    memset(manifest, 0, sizeof(*manifest));

    err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
    if (err) {
        if (err == -ENOENT) {
            return GOLIOTH_OK;
        }

        GLTH_LOGW(TAG, "Failed to decode desired map: %d", err);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }

    manifest->seqnum = manifest_sequence_number;

    return GOLIOTH_OK;
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
