/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/golioth_status.h>
#include <golioth/client.h>

enum golioth_status golioth_gateway_uplink_async(struct golioth_client *client,
                                                 const uint8_t *buf,
                                                 size_t buf_len,
                                                 golioth_set_cb_fn callback,
                                                 void *callback_arg);

enum golioth_status golioth_gateway_uplink_sync(struct golioth_client *client,
                                                const uint8_t *buf,
                                                size_t buf_len,
                                                int32_t timeout_s);