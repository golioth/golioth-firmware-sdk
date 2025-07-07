/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/gateway.h>
#include "coap_blockwise.h"

#if defined(CONFIG_GOLIOTH_GATEWAY)

struct blockwise_transfer *golioth_gateway_uplink_start(struct golioth_client *client)
{
    return golioth_blockwise_upload_start(client, ".pouch", "", GOLIOTH_CONTENT_TYPE_OCTET_STREAM);
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
                                          NULL,
                                          NULL,
                                          callback_arg,
                                          NULL,
                                          false,
                                          GOLIOTH_SYS_WAIT_FOREVER);
}

void golioth_gateway_uplink_finish(struct blockwise_transfer *ctx)
{
    golioth_blockwise_upload_finish(ctx);
}

#endif  // CONFIG_GOLIOTH_GATEWAY
