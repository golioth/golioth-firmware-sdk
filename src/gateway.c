/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/gateway.h>
#include "coap_client.h"
#include "coap_blockwise.h"

#if defined(CONFIG_GOLIOTH_GATEWAY)

#define GATEWAY_URI ".pouch"
#define DOWNLINK_URI GATEWAY_URI

struct blockwise_transfer *golioth_gateway_uplink_start(struct golioth_client *client)
{
    return golioth_blockwise_upload_start(client,
                                          GATEWAY_URI,
                                          "",
                                          GOLIOTH_CONTENT_TYPE_OCTET_STREAM);
}

enum golioth_status golioth_gateway_uplink_block(struct blockwise_transfer *ctx,
                                                 uint32_t block_idx,
                                                 const uint8_t *buf,
                                                 size_t buf_len,
                                                 bool is_last,
                                                 golioth_set_block_cb_fn set_cb,
                                                 void *callback_arg)
{
    return golioth_blockwise_upload_block(ctx,
                                          block_idx,
                                          buf,
                                          buf_len,
                                          is_last,
                                          set_cb,
                                          callback_arg,
                                          false,
                                          GOLIOTH_SYS_WAIT_FOREVER);
}

void golioth_gateway_uplink_finish(struct blockwise_transfer *ctx)
{
    golioth_blockwise_upload_finish(ctx);
}

struct gateway_downlink_block_ctx
{
    gateway_downlink_block_cb cb;
    void *arg;
};

static enum golioth_status downlink_block_cb_wrapper(uint32_t block_idx,
                                                     uint8_t *block_buffer,
                                                     size_t block_buffer_len,
                                                     bool is_last,
                                                     size_t negotiated_block_size,
                                                     void *callback_arg)
{
    struct gateway_downlink_block_ctx *ctx = callback_arg;

    return ctx->cb(block_buffer, block_buffer_len, is_last, ctx->arg);
}

enum golioth_status golioth_gateway_downlink_get(struct golioth_client *client,
                                                 const char *device_id,
                                                 gateway_downlink_block_cb cb,
                                                 void *arg)

{
    struct gateway_downlink_block_ctx ctx = {
        .cb = cb,
        .arg = arg,
    };

    return golioth_blockwise_get(client,
                                 DOWNLINK_URI,
                                 "",
                                 GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                 0,
                                 downlink_block_cb_wrapper,
                                 &ctx);
}

#endif  // CONFIG_GOLIOTH_GATEWAY
