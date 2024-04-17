/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/client.h>
#include <string.h>
#include <assert.h>
#include "coap_client.h"
#include "coap_blockwise.h"

struct blockwise_transfer
{
    bool is_last;
    enum golioth_status status;
    int retry_count;
    uint32_t offset;
    size_t block_size;
    const char *path_prefix;
    const char *path;
    uint8_t *block_buffer;
    union
    {
        // TODO: add read_block_cb here
        write_block_cb write_cb;
    } callback;
    void *callback_arg;
};

// Function to initialize the blockwise_transfer structure
static void blockwise_download_init(struct blockwise_transfer *ctx,
                                    uint8_t *data_buf,
                                    const char *path_prefix,
                                    const char *path,
                                    write_block_cb cb,
                                    void *callback_arg)
{
    ctx->is_last = false;
    ctx->path_prefix = path_prefix;
    ctx->path = path;
    ctx->retry_count = 0;
    ctx->block_size = 0;
    ctx->offset = 0;
    ctx->block_buffer = data_buf;
    ctx->callback_arg = callback_arg;
    ctx->callback.write_cb = cb;
}

// Function to call the application's write block callback after a successful
// blockwise download
static void call_write_block_callback(struct blockwise_transfer *ctx)
{
    if (ctx->callback.write_cb(ctx->offset,
                               ctx->block_buffer,
                               ctx->block_size,
                               ctx->is_last,
                               ctx->callback_arg)
        != GOLIOTH_OK)
    {
        // TODO: handle application callback error
    }
    ctx->offset++;
}

// Blockwise download's internal callback function that the COAP client calls
static void on_block_rcvd(struct golioth_client *client,
                          const struct golioth_response *response,
                          const char *path,
                          const uint8_t *payload,
                          size_t payload_size,
                          bool is_last,
                          void *arg)
{
    // assert valid values of arg, payload size and block_buffer
    assert(arg);
    assert(payload_size <= CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_BUFFER_SIZE);
    struct blockwise_transfer *ctx = (struct blockwise_transfer *) arg;
    assert(ctx->block_buffer);

    // copy blockwise download values returned by COAP client into ctx
    ctx->is_last = is_last;
    memcpy(ctx->block_buffer, payload, payload_size);
    ctx->block_size = payload_size;
    ctx->status = response->status;
}

// Function to download a single block
static enum golioth_status download_single_block(struct golioth_client *client,
                                                 struct blockwise_transfer *ctx)
{
    return golioth_coap_client_get_block(client,
                                         ctx->path_prefix,
                                         ctx->path,
                                         GOLIOTH_CONTENT_TYPE_JSON,
                                         ctx->offset,
                                         ctx->block_size,
                                         on_block_rcvd,
                                         ctx,
                                         true,
                                         GOLIOTH_SYS_WAIT_FOREVER);
}

// Function to handle blockwise download errors
static enum golioth_status handle_download_error()
{
    // TODO: handle errors like disconnects etc
    return GOLIOTH_OK;
}

// Function to manage blockwise downloads and handle errors
static enum golioth_status process_blockwise_downloads(struct golioth_client *client,
                                                       struct blockwise_transfer *ctx)
{
    enum golioth_status status = download_single_block(client, ctx);
    if (ctx->status == GOLIOTH_OK)
    {
        call_write_block_callback(ctx);
    }
    else
    {
        // TODO: handle_download_error
        status = handle_download_error();
    }
    return status;
}

enum golioth_status golioth_blockwise_get(struct golioth_client *client,
                                          const char *path_prefix,
                                          const char *path,
                                          write_block_cb cb,
                                          void *callback_arg)
{
    enum golioth_status status = GOLIOTH_OK;
    assert(client);
    assert(path);
    assert(cb);
    struct blockwise_transfer *ctx = malloc(sizeof(struct blockwise_transfer));
    uint8_t *data_buff = malloc(CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_BUFFER_SIZE);
    assert(data_buff);
    blockwise_download_init(ctx, data_buff, path_prefix, path, cb, callback_arg);
    while (!ctx->is_last)
    {
        if ((status = process_blockwise_downloads(client, ctx)) != GOLIOTH_OK)
        {
            break;
        }
    }

    // Download complete. Free the memory
    free(data_buff);
    free(ctx);

    return status;
}
