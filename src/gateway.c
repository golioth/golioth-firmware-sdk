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

#define GOLIOTH_GATEWAY_PATH_PREFIX ".g/"

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

struct server_cert_context
{
    void *buf;
    size_t buf_len;
    size_t received_size;
    struct k_sem sem;
    enum golioth_status status;
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

    uplink->transfer_ctx = golioth_blockwise_upload_start(client,
                                                          GOLIOTH_GATEWAY_PATH_PREFIX,
                                                          "pouch",
                                                          GOLIOTH_CONTENT_TYPE_OCTET_STREAM);

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
                                          GOLIOTH_SYS_WAIT_FOREVER);
}

void golioth_gateway_uplink_finish(struct gateway_uplink *uplink)
{
    golioth_blockwise_upload_finish(uplink->transfer_ctx);

    golioth_sys_free(uplink);
}

static enum golioth_status server_cert_data(struct golioth_client *client,
                                            const char *path,
                                            uint32_t block_idx,
                                            const uint8_t *block_buffer,
                                            size_t block_buffer_len,
                                            bool is_last,
                                            size_t negotiated_block_size,
                                            void *arg)
{
    struct server_cert_context *ctx = arg;

    size_t offset = negotiated_block_size * block_idx;
    if (offset + block_buffer_len > ctx->buf_len)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    void *dst = (void *) (((intptr_t) ctx->buf) + offset);

    memcpy(dst, block_buffer, block_buffer_len);
    ctx->received_size += block_buffer_len;

    return GOLIOTH_OK;
}

static void server_cert_end(struct golioth_client *client,
                            enum golioth_status status,
                            const struct golioth_coap_rsp_code *coap_rsp_code,
                            const char *path,
                            uint32_t block_idx,
                            void *arg)
{
    struct server_cert_context *ctx = arg;

    ctx->status = status;
    k_sem_give(&ctx->sem);
}

enum golioth_status golioth_gateway_server_cert_get(struct golioth_client *client,
                                                    void *buf,
                                                    size_t *len)

{
    enum golioth_status status;
    struct server_cert_context ctx = {
        .buf = buf,
        .buf_len = *len,
        .received_size = 0,
    };
    k_sem_init(&ctx.sem, 0, 1);

    status = golioth_blockwise_get(client,
                                   GOLIOTH_GATEWAY_PATH_PREFIX,
                                   "server-cert",
                                   GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                   0,
                                   server_cert_data,
                                   server_cert_end,
                                   &ctx);
    if (status != GOLIOTH_OK)
    {
        return status;
    }

    k_sem_take(&ctx.sem, K_FOREVER);

    *len = ctx.received_size;

    return ctx.status;
}

enum golioth_status golioth_gateway_device_cert_set(struct golioth_client *client,
                                                    const void *buf,
                                                    size_t len,
                                                    golioth_set_cb_fn set_cb,
                                                    void *callback_arg,
                                                    int32_t timeout_s)
{
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_set(client,
                                   token,
                                   GOLIOTH_GATEWAY_PATH_PREFIX,
                                   "device-cert",
                                   GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                   buf,
                                   len,
                                   set_cb,
                                   callback_arg,
                                   timeout_s);
}

#endif  // CONFIG_GOLIOTH_GATEWAY
