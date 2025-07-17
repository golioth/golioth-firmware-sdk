/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/gateway.h>
#include <golioth/payload_utils.h>
#include "coap_blockwise.h"
#include "coap_client.h"

#if defined(CONFIG_GOLIOTH_GATEWAY)

struct gateway_downlink
{
    gateway_downlink_block_cb block_cb;
    gateway_downlink_end_cb end_cb;
    void *arg;
};

struct gateway_uplink
{
    struct blockwise_transfer *transfer_ctx;
    struct gateway_downlink *downlink;
};

static enum golioth_status downlink_block_cb_wrapper(struct golioth_client *client,
                                                     const char *path,
                                                     uint32_t block_idx,
                                                     const uint8_t *block_buffer,
                                                     size_t block_buffer_len,
                                                     bool is_last,
                                                     size_t negotiated_block_size,
                                                     void *arg)
{
    struct gateway_downlink *downlink = arg;

    return downlink->block_cb(block_buffer, block_buffer_len, is_last, downlink->arg);
}

static void downlink_end_cb_wrapper(struct golioth_client *client,
                                    enum golioth_status status,
                                    const struct golioth_coap_rsp_code *coap_rsp_code,
                                    const char *path,
                                    uint32_t block_idx,
                                    void *arg)
{
    struct gateway_downlink *downlink = arg;

    downlink->end_cb(status, coap_rsp_code, downlink->arg);

    golioth_sys_free(downlink);
}

struct gateway_uplink *golioth_gateway_uplink_start(struct golioth_client *client,
                                                    gateway_downlink_block_cb dnlk_block_cb,
                                                    gateway_downlink_end_cb dnlk_end_cb,
                                                    void *downlink_arg)
{
    struct gateway_uplink *uplink = golioth_sys_malloc(sizeof(struct gateway_uplink));
    if (NULL == uplink)
    {
        return NULL;
    }

    if (NULL != dnlk_block_cb && NULL != dnlk_end_cb)
    {
        uplink->downlink = golioth_sys_malloc(sizeof(struct gateway_downlink));
        if (NULL == uplink->downlink)
        {
            golioth_sys_free(uplink);
            return NULL;
        }
        uplink->downlink->block_cb = dnlk_block_cb;
        uplink->downlink->end_cb = dnlk_end_cb;
        uplink->downlink->arg = downlink_arg;
    }
    else
    {
        uplink->downlink = NULL;
    }

    uplink->transfer_ctx = golioth_blockwise_upload_start(client, ".pouch", "", GOLIOTH_CONTENT_TYPE_OCTET_STREAM);

    if (NULL == uplink->transfer_ctx)
    {
        golioth_sys_free(uplink->downlink);
        golioth_sys_free(uplink);
        return NULL;
    }

    return uplink;
}

enum golioth_status golioth_gateway_uplink_block(struct gateway_uplink *uplink,
                                                 uint32_t block_idx,
                                                 const uint8_t *buf,
                                                 size_t buf_len,
                                                 bool is_last,
                                                 golioth_set_block_cb_fn set_cb,
                                                 void *callback_arg)
{
    golioth_get_block_cb_fn get_block_cb = NULL;
    golioth_end_block_cb_fn end_block_cb = NULL;

    if (is_last && NULL != uplink->downlink)
    {
        get_block_cb = downlink_block_cb_wrapper;
        end_block_cb = downlink_end_cb_wrapper;
    }

    return golioth_blockwise_upload_block(uplink->transfer_ctx,
                                          block_idx,
                                          buf,
                                          buf_len,
                                          is_last,
                                          set_cb,
                                          get_block_cb,
                                          end_block_cb,
                                          callback_arg,
                                          uplink->downlink,
                                          false,
                                          GOLIOTH_SYS_WAIT_FOREVER);
}

void golioth_gateway_uplink_finish(struct gateway_uplink *uplink)
{
    golioth_blockwise_upload_finish(uplink->transfer_ctx);

    golioth_sys_free(uplink);
}

struct server_cert_context
{
    void *buf;
    size_t len;
    enum golioth_status status;
};

static void on_server_cert(struct golioth_client *client,
                           enum golioth_status status,
                           const struct golioth_coap_rsp_code *coap_rsp_code,
                           const char *path,
                           const uint8_t *payload,
                           size_t payload_size,
                           void *arg)
{
    struct server_cert_context *ctx = arg;

    if (status != GOLIOTH_OK)
    {
        ctx->status = status;
        return;
    }

    if (golioth_payload_is_null(payload, payload_size))
    {
        ctx->status = GOLIOTH_ERR_NULL;
        return;
    }

    if (payload_size > ctx->len) {
        ctx->status = GOLIOTH_ERR_MEM_ALLOC;
        return;
    }

    memcpy(ctx->buf, payload, payload_size);
    ctx->len = payload_size;
}

enum golioth_status golioth_gateway_server_cert_get(struct golioth_client *client,
                                                    void *buf, size_t *len, int32_t timeout_s)

{
    enum golioth_status status;
    struct server_cert_context ctx = {
        .buf = buf,
        .len = *len,
    };

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    status = golioth_coap_client_get(client,
                                   token,
                                   ".g/server-cert",
                                   "",
                                   GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                   on_server_cert,
                                   &ctx,
                                   true,
                                   timeout_s);
    if (status != GOLIOTH_OK) {
        return status;
    }

    *len = ctx.len;

    return GOLIOTH_OK;
}

#endif  // CONFIG_GOLIOTH_GATEWAY
