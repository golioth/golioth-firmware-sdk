/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/gateway.h>
#include "coap_client.h"

enum golioth_status golioth_gateway_uplink_async(struct golioth_client *client,
                                                 const uint8_t *buf,
                                                 size_t buf_len,
                                                 golioth_set_cb_fn callback,
                                                 void *callback_arg)
{
    return golioth_coap_client_set(client,
                                   ".pouch",
                                   "",
                                   GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                   buf,
                                   buf_len,
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_gateway_uplink_sync(struct golioth_client *client,
                                                const uint8_t *buf,
                                                size_t buf_len,
                                                int32_t timeout_s)
{
    return golioth_coap_client_set(client,
                                   ".pouch",
                                   "",
                                   GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                   buf,
                                   buf_len,
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}
