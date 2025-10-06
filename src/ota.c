/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <golioth/ota.h>
#include "coap_blockwise.h"
#include "coap_client.h"
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include "golioth/client.h"
#include "golioth_util.h"
#include <golioth/zcbor_utils.h>

#if defined(CONFIG_GOLIOTH_OTA) || defined(CONFIG_GOLIOTH_FW_UPDATE)

LOG_TAG_DEFINE(golioth_ota);

#define GOLIOTH_OTA_MANIFEST_PATH_PREFIX ".u/"
#define GOLIOTH_OTA_MANIFEST_PATH_DESIRED "desired"
#define GOLIOTH_OTA_COMPONENT_PATH_PREFIX ".u/c/"

enum
{
    MANIFEST_KEY_SEQUENCE_NUMBER = 1,
    MANIFEST_KEY_HASH = 2,
    MANIFEST_KEY_COMPONENTS = 3,
};

enum
{
    COMPONENT_KEY_PACKAGE = 1,
    COMPONENT_KEY_VERSION = 2,
    COMPONENT_KEY_HASH = 3,
    COMPONENT_KEY_SIZE = 4,
    COMPONENT_KEY_URI = 5,
    COMPONENT_KEY_BOOTLOADER = 6,
};

typedef struct
{
    uint8_t *buf;
    size_t *block_nbytes;
    bool *is_last;
} block_get_output_params_t;

struct component_tstr_value
{
    char *value;
    size_t value_len;
};

static enum golioth_ota_state _state = GOLIOTH_OTA_STATE_IDLE;
static golioth_sys_timer_t manifest_timer;
static bool manifest_timer_initialized = false;
static struct manifest_get_args
{
    struct golioth_client *client;
    golioth_get_cb_fn cb;
    void *arg;
} manifest_timer_arg;

size_t golioth_ota_size_to_nblocks(size_t component_size)
{
    size_t nblocks = component_size / CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE;
    if ((component_size % CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE) != 0)
    {
        nblocks++;
    }
    return nblocks;
}

const struct golioth_ota_component *golioth_ota_find_component(
    const struct golioth_ota_manifest *manifest,
    const char *package)
{
    // Scan the manifest until we find the component with matching package.
    const struct golioth_ota_component *found = NULL;
    for (size_t i = 0; i < manifest->num_components; i++)
    {
        const struct golioth_ota_component *c = &manifest->components[i];
        bool matches = (0 == strcmp(c->package, package));
        if (matches)
        {
            found = c;
            break;
        }
    }
    return found;
}

static void ota_manifest_timer_expiry(golioth_sys_timer_t timer, void *user_arg)
{
    struct manifest_get_args *get_args = user_arg;

    if (golioth_client_is_running(get_args->client))
    {
        enum golioth_status status =
            golioth_ota_get_manifest_async(get_args->client, get_args->cb, get_args->arg);

        if (GOLIOTH_OK != status)
        {
            GLTH_LOGE(TAG, "Periodic OTA manifest fetch failed");
        }
    }
    else
    {
        GLTH_LOGD(TAG, "Client not running; skipping periodic manifest fetch");
    }

    golioth_sys_timer_reset(timer);
}

enum golioth_status golioth_ota_manifest_subscribe(struct golioth_client *client,
                                                   golioth_get_cb_fn callback,
                                                   void *arg)
{
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    enum golioth_status status = golioth_coap_client_observe(client,
                                                             token,
                                                             GOLIOTH_OTA_MANIFEST_PATH_PREFIX,
                                                             GOLIOTH_OTA_MANIFEST_PATH_DESIRED,
                                                             GOLIOTH_CONTENT_TYPE_CBOR,
                                                             callback,
                                                             arg);
    if (GOLIOTH_OK != status)
    {
        return status;
    }

    manifest_timer_arg.client = client;
    manifest_timer_arg.cb = callback;
    manifest_timer_arg.arg = arg;

    if (!manifest_timer_initialized && CONFIG_GOLIOTH_OTA_MANIFEST_SUBSCRIPTION_POLL_INTERVAL_S > 0)
    {
        struct golioth_timer_config cfg = {
            .name = "ota_manifest_timer",
            .expiration_ms = CONFIG_GOLIOTH_OTA_MANIFEST_SUBSCRIPTION_POLL_INTERVAL_S * 1000,
            .fn = ota_manifest_timer_expiry,
            .user_arg = &manifest_timer_arg,
        };
        manifest_timer = golioth_sys_timer_create(&cfg);

        manifest_timer_initialized = true;
    }

    if (manifest_timer_initialized)
    {
        golioth_sys_timer_reset(manifest_timer);
    }

    return GOLIOTH_OK;
}

enum golioth_status golioth_ota_get_manifest_async(struct golioth_client *client,
                                                   golioth_get_cb_fn callback,
                                                   void *arg)
{
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    if (manifest_timer_initialized)
    {
        golioth_sys_timer_reset(manifest_timer);
    }

    return golioth_coap_client_get(client,
                                   token,
                                   GOLIOTH_OTA_MANIFEST_PATH_PREFIX,
                                   GOLIOTH_OTA_MANIFEST_PATH_DESIRED,
                                   GOLIOTH_CONTENT_TYPE_CBOR,
                                   callback,
                                   arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_ota_blockwise_manifest_async(struct golioth_client *client,
                                                         size_t block_idx,
                                                         golioth_get_block_cb_fn block_cb,
                                                         golioth_end_block_cb_fn end_cb,
                                                         void *arg)
{
    if (manifest_timer_initialized)
    {
        golioth_sys_timer_reset(manifest_timer);
    }

    return golioth_blockwise_get(client,
                                 GOLIOTH_OTA_MANIFEST_PATH_PREFIX,
                                 GOLIOTH_OTA_MANIFEST_PATH_DESIRED,
                                 GOLIOTH_CONTENT_TYPE_CBOR,
                                 block_idx,
                                 block_cb,
                                 end_cb,
                                 arg);
}

enum golioth_status golioth_ota_report_state_sync(struct golioth_client *client,
                                                  enum golioth_ota_state state,
                                                  enum golioth_ota_reason reason,
                                                  const char *package,
                                                  const char *current_version,
                                                  const char *target_version,
                                                  int32_t timeout_s)
{
    uint8_t encode_buf[64];
    ZCBOR_STATE_E(zse, 1, encode_buf, sizeof(encode_buf), 1);
    bool ok;

    ok = zcbor_map_start_encode(zse, 1);
    if (!ok)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(zse, "s") && zcbor_uint32_put(zse, state);
    if (!ok)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(zse, "r") && zcbor_uint32_put(zse, reason);
    if (!ok)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(zse, "pkg") && zcbor_tstr_put_term(zse, package, SIZE_MAX);
    if (!ok)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    if (current_version && current_version[0] != '\0')
    {
        ok = zcbor_tstr_put_lit(zse, "v") && zcbor_tstr_put_term(zse, current_version, SIZE_MAX);
        if (!ok)
        {
            return GOLIOTH_ERR_MEM_ALLOC;
        }
    }

    if (target_version && target_version[0] != '\0')
    {
        ok = zcbor_tstr_put_lit(zse, "t") && zcbor_tstr_put_term(zse, target_version, SIZE_MAX);
        if (!ok)
        {
            return GOLIOTH_ERR_MEM_ALLOC;
        }
    }

    ok = zcbor_map_end_encode(zse, 1);
    if (!ok)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    _state = state;
    return golioth_coap_client_set(client,
                                   token,
                                   GOLIOTH_OTA_COMPONENT_PATH_PREFIX,
                                   package,
                                   GOLIOTH_CONTENT_TYPE_CBOR,
                                   encode_buf,
                                   zse->payload - encode_buf,
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

static int component_entry_decode_value(zcbor_state_t *zsd, void *void_value)
{
    struct component_tstr_value *value = void_value;
    struct zcbor_string tstr;
    bool ok;

    ok = zcbor_tstr_decode(zsd, &tstr);
    if (!ok)
    {
        return -EBADMSG;
    }

    if (tstr.len > value->value_len)
    {
        GLTH_LOGE(TAG, "Not enough space to store");
        return -ENOMEM;
    }

    memcpy(value->value, tstr.value, tstr.len);
    value->value[tstr.len] = '\0';

    return 0;
}

static int components_decode(zcbor_state_t *zsd, void *value)
{
    struct golioth_ota_manifest *manifest = value;
    int err;
    bool ok;

    ok = zcbor_list_start_decode(zsd);
    if (!ok)
    {
        GLTH_LOGW(TAG, "Did not start CBOR list correctly");
        return -EBADMSG;
    }

    for (size_t i = 0; i < ARRAY_SIZE(manifest->components) && !zcbor_list_or_map_end(zsd); i++)
    {
        struct golioth_ota_component *component = &manifest->components[i];
        struct component_tstr_value package = {
            component->package,
            sizeof(component->package) - 1,
        };
        struct component_tstr_value version = {
            component->version,
            sizeof(component->version) - 1,
        };
        char hash_string[GOLIOTH_OTA_COMPONENT_HEX_HASH_LEN + 1];
        struct component_tstr_value hash = {
            hash_string,
            sizeof(hash_string) - 1,
        };
        struct component_tstr_value uri = {
            component->uri,
            sizeof(component->uri) - 1,
        };
        struct component_tstr_value bootloader_name = {
            component->bootloader,
            sizeof(component->bootloader) - 1,
        };

        int64_t component_size;
        struct zcbor_map_entry map_entries[] = {
            ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_PACKAGE, component_entry_decode_value, &package),
            ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_VERSION, component_entry_decode_value, &version),
            ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_HASH, component_entry_decode_value, &hash),
            ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_SIZE, zcbor_map_int64_decode, &component_size),
            ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_URI, component_entry_decode_value, &uri),
            ZCBOR_U32_MAP_ENTRY(COMPONENT_KEY_BOOTLOADER,
                                component_entry_decode_value,
                                &bootloader_name),
        };

        err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
        if (err)
        {
            GLTH_LOGE(TAG, "Failed to decode component number %zu", i);
            return err;
        }

        golioth_sys_hex2bin(hash_string,
                            strlen(hash_string),
                            component->hash,
                            sizeof(component->hash));
        component->size = component_size;

        manifest->num_components++;
    }

    ok = zcbor_list_end_decode(zsd);
    if (!ok)
    {
        /* Most likely reason for this error is that server sent multiple components but device was
         * configured to receive fewer */
        GLTH_LOGW(TAG, "Can't end CBOR list (check GOLIOTH_OTA_MAX_NUM_COMPONENTS)");
        return -EBADMSG;
    }

    return 0;
}

enum golioth_status golioth_ota_payload_as_manifest(const uint8_t *payload,
                                                    size_t payload_size,
                                                    struct golioth_ota_manifest *manifest)
{
    ZCBOR_STATE_D(zsd, 3, payload, payload_size, 1, 0);
    int64_t manifest_sequence_number;
    struct zcbor_map_entry map_entries[] = {
        ZCBOR_U32_MAP_ENTRY(MANIFEST_KEY_SEQUENCE_NUMBER,
                            zcbor_map_int64_decode,
                            &manifest_sequence_number),
        ZCBOR_U32_MAP_ENTRY(MANIFEST_KEY_COMPONENTS, components_decode, manifest),
    };
    int err;

    memset(manifest, 0, sizeof(*manifest));

    err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
    if (err)
    {
        if (err == -ENOENT)
        {
            return GOLIOTH_OK;
        }

        GLTH_LOGW(TAG, "Failed to decode desired map: %d", err);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }

    manifest->seqnum = manifest_sequence_number;

    return GOLIOTH_OK;
}

static void on_block_rcvd(struct golioth_client *client,
                          enum golioth_status status,
                          const struct golioth_coap_rsp_code *coap_rsp_code,
                          const char *path,
                          const uint8_t *payload,
                          size_t payload_size,
                          bool is_last,
                          void *arg)
{
    assert(arg);
    assert(payload_size <= CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE);

    if (status != GOLIOTH_OK)
    {
        return;
    }

    block_get_output_params_t *out_params = (block_get_output_params_t *) arg;
    assert(out_params->buf);
    assert(out_params->block_nbytes);

    if (out_params->is_last)
    {
        *out_params->is_last = is_last;
    }

    memcpy(out_params->buf, payload, payload_size);
    *out_params->block_nbytes = payload_size;
}

struct ota_component_blockwise_ctx
{
    const struct golioth_ota_component *component;
    ota_component_block_write_cb block_cb;
    ota_component_download_end_cb end_cb;
    void *arg;
};

static enum golioth_status ota_component_write_cb_wrapper(struct golioth_client *client,
                                                          const char *path,
                                                          uint32_t block_idx,
                                                          const uint8_t *block_buffer,
                                                          size_t block_buffer_len,
                                                          bool is_last,
                                                          size_t negotiated_block_size,
                                                          void *arg)
{
    assert(arg);
    struct ota_component_blockwise_ctx *ctx = arg;

    enum golioth_status status = ctx->block_cb(ctx->component,
                                               block_idx,
                                               block_buffer,
                                               block_buffer_len,
                                               is_last,
                                               negotiated_block_size,
                                               ctx->arg);

    return status;
}

static void ota_component_download_end_cb_wrapper(struct golioth_client *client,
                                                  enum golioth_status status,
                                                  const struct golioth_coap_rsp_code *coap_rsp_code,
                                                  const char *path,
                                                  uint32_t block_idx,
                                                  void *arg)
{
    assert(arg);
    struct ota_component_blockwise_ctx *ctx = arg;

    ctx->end_cb(status, coap_rsp_code, ctx->component, block_idx, ctx->arg);

    golioth_sys_free(ctx);
}

enum golioth_status golioth_ota_download_component(struct golioth_client *client,
                                                   const struct golioth_ota_component *component,
                                                   uint32_t block_idx,
                                                   ota_component_block_write_cb block_cb,
                                                   ota_component_download_end_cb end_cb,
                                                   void *arg)
{
    struct ota_component_blockwise_ctx *ctx =
        golioth_sys_malloc(sizeof(struct ota_component_blockwise_ctx));
    ctx->component = component;
    ctx->block_cb = block_cb;
    ctx->end_cb = end_cb;
    ctx->arg = arg;


    return golioth_blockwise_get(client,
                                 "",
                                 component->uri,
                                 GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                 block_idx,
                                 ota_component_write_cb_wrapper,
                                 ota_component_download_end_cb_wrapper,
                                 ctx);
}

enum golioth_status golioth_ota_get_block_sync(struct golioth_client *client,
                                               const char *package,
                                               const char *version,
                                               size_t block_index,
                                               uint8_t *buf,
                                               size_t *block_nbytes,
                                               bool *is_last,
                                               int32_t timeout_s)
{
    char path[GOLIOTH_OTA_MAX_COMPONENT_URI_LEN] = {};
    snprintf(path, sizeof(path), "%s@%s", package, version);
    block_get_output_params_t out_params = {
        .buf = buf,
        .block_nbytes = block_nbytes,
        .is_last = is_last,
    };

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    enum golioth_status status = GOLIOTH_OK;
    status = golioth_coap_client_get_block(client,
                                           token,
                                           GOLIOTH_OTA_COMPONENT_PATH_PREFIX,
                                           path,
                                           GOLIOTH_CONTENT_TYPE_JSON,
                                           block_index,
                                           CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE,
                                           on_block_rcvd,
                                           &out_params,
                                           true,
                                           timeout_s);

    return status;
}

enum golioth_ota_state golioth_ota_get_state(void)
{
    return _state;
}

#endif  // CONFIG_GOLIOTH_OTA || CONFIG_GOLIOTH_FW_UPDATE
