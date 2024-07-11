/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/stream.h>
#include "coap_client.h"
#include <golioth/coap_blockwise.h>

#if defined(CONFIG_GOLIOTH_STREAM)

#define GOLIOTH_STREAM_PATH_PREFIX ".s/"

enum golioth_status golioth_stream_set_async(struct golioth_client *client,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             const uint8_t *buf,
                                             size_t buf_len,
                                             golioth_set_cb_fn callback,
                                             void *callback_arg)
{
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   content_type,
                                   buf,
                                   buf_len,
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_stream_set_sync(struct golioth_client *client,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            const uint8_t *buf,
                                            size_t buf_len,
                                            int32_t timeout_s)
{
    return golioth_coap_client_set(client,
                                   GOLIOTH_STREAM_PATH_PREFIX,
                                   path,
                                   content_type,
                                   buf,
                                   buf_len,
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

enum golioth_status golioth_stream_set_blockwise_sync(struct golioth_client *client,
                                                      const char *path,
                                                      enum golioth_content_type content_type,
                                                      stream_read_block_cb cb,
                                                      void *arg)
{
    return golioth_blockwise_post(client, GOLIOTH_STREAM_PATH_PREFIX, path, content_type, cb, arg);
}


#endif  // CONFIG_GOLIOTH_STREAM
