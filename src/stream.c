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
    if (!client || !path || !cb)
    {
        return GOLIOTH_ERR_FAIL;
    }

    struct blockwise_transfer *ctx = golioth_blockwise_post_ctx_create(GOLIOTH_STREAM_PATH_PREFIX,
                                                                       path,
                                                                       content_type,
                                                                       cb,
                                                                       arg);
    if (!ctx)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    enum golioth_status status = golioth_blockwise_post(client, ctx, NULL);

    /* Upload complete, clean up allocated resources */
    golioth_blockwise_post_ctx_destroy(ctx);

    return status;
}


#endif  // CONFIG_GOLIOTH_STREAM
