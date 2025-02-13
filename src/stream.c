/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/stream.h>
#include "coap_client.h"
#include "coap_blockwise.h"

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
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_set(client,
                                   token,
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
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_set(client,
                                   token,
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
    return golioth_blockwise_post(client,
                                  GOLIOTH_STREAM_PATH_PREFIX,
                                  path,
                                  content_type,
                                  cb,
                                  NULL,
                                  arg);
}

struct blockwise_transfer *golioth_stream_blockwise_start(struct golioth_client *client,
                                                          const char *path,
                                                          enum golioth_content_type content_type)
{
    return golioth_blockwise_upload_start(client, GOLIOTH_STREAM_PATH_PREFIX, path, content_type);
}

void golioth_stream_blockwise_finish(struct blockwise_transfer *ctx)
{
    return golioth_blockwise_upload_finish(ctx);
}

enum golioth_status golioth_stream_blockwise_set_block_async(struct blockwise_transfer *ctx,
                                                             uint32_t block_idx,
                                                             const uint8_t *block_buffer,
                                                             size_t data_len,
                                                             bool is_last,
                                                             golioth_set_block_cb_fn callback,
                                                             void *callback_arg)
{
    return golioth_blockwise_upload_block(ctx,
                                          block_idx,
                                          block_buffer,
                                          data_len,
                                          is_last,
                                          callback,
                                          callback_arg,
                                          false,
                                          GOLIOTH_SYS_WAIT_FOREVER);
}


#endif  // CONFIG_GOLIOTH_STREAM
