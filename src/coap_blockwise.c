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
    enum golioth_content_type content_type;
    int retry_count;
    uint32_t offset;
    size_t block_size;
    const char *path_prefix;
    const char *path;
    uint8_t *block_buffer;
    union
    {
        read_block_cb read_cb;
        write_block_cb write_cb;
    } callback;
    void *callback_arg;
};

/* Blockwise Uploads related functions */

// Function to initialize the blockwise_transfer structure for uploads
static void blockwise_upload_init(struct blockwise_transfer *ctx,
                                  uint8_t *data_buf,
                                  const char *path_prefix,
                                  const char *path,
                                  enum golioth_content_type content_type,
                                  read_block_cb cb,
                                  void *callback_arg)
{
    ctx->is_last = false;
    ctx->path_prefix = path_prefix;
    ctx->path = path;
    ctx->content_type = content_type;
    ctx->retry_count = 0;
    ctx->block_size = 0;
    ctx->offset = 0;
    ctx->block_buffer = data_buf;
    ctx->callback_arg = callback_arg;
    ctx->callback.read_cb = cb;
}

// Blockwise upload's internal callback function that the COAP client calls
static void on_block_sent(struct golioth_client *client,
                          const struct golioth_response *response,
                          const char *path,
                          void *arg)
{
    assert(arg);
    struct blockwise_transfer *ctx = (struct blockwise_transfer *) arg;
    ctx->status = response->status;
}

// Function to call the application's read block callback after a successful
// blockwise upload
static void call_read_block_callback(struct blockwise_transfer *ctx)
{
    int err = ctx->callback.read_cb(ctx->offset,
                                    ctx->block_buffer,
                                    &ctx->block_size,
                                    &ctx->is_last,
                                    ctx->callback_arg);
    if (err != GOLIOTH_OK)
    {
        // TODO: handle application callback error
    }
}

// Function to upload a single block
static enum golioth_status upload_single_block(struct golioth_client *client,
                                               struct blockwise_transfer *ctx)
{
    assert(ctx->block_size > 0 && ctx->block_size <= CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_BLOCK_SIZE);
    return golioth_coap_client_set_block(client,
                                         ctx->path_prefix,
                                         ctx->path,
                                         ctx->is_last,
                                         GOLIOTH_CONTENT_TYPE_JSON,
                                         ctx->offset,
                                         ctx->block_buffer,
                                         ctx->block_size,
                                         on_block_sent,
                                         ctx,
                                         true,
                                         GOLIOTH_SYS_WAIT_FOREVER);
}

// Function to handle blockwise upload errors
static enum golioth_status handle_upload_error()
{
    // TODO: handle errors like disconnects etc
    return GOLIOTH_OK;
}

// Function to manage blockwise upload and handle errors
static enum golioth_status process_blockwise_uploads(struct golioth_client *client,
                                                     struct blockwise_transfer *ctx)
{
    enum golioth_status status = GOLIOTH_ERR_FAIL;
    ctx->block_size = 0;
    call_read_block_callback(ctx);
    if (ctx->block_size)
    {
        status = upload_single_block(client, ctx);
        if (ctx->status == GOLIOTH_OK)
        {
            ctx->offset++;
        }
        else
        {
            // TODO: handle_upload_error
            status = handle_upload_error();
        }
    }
    return status;
}

enum golioth_status golioth_blockwise_post(struct golioth_client *client,
                                           const char *path_prefix,
                                           const char *path,
                                           enum golioth_content_type content_type,
                                           read_block_cb cb,
                                           void *callback_arg)
{
    enum golioth_status status = GOLIOTH_ERR_FAIL;
    if (!client || !path || !cb)
    {
        return status;
    }

    struct blockwise_transfer *ctx = malloc(sizeof(struct blockwise_transfer));
    if (!ctx)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    uint8_t *data_buff = malloc(CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_BLOCK_SIZE);
    if (!data_buff)
    {
        free(ctx);
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    blockwise_upload_init(ctx, data_buff, path_prefix, path, content_type, cb, callback_arg);
    while (!ctx->is_last)
    {
        if ((status = process_blockwise_uploads(client, ctx)) != GOLIOTH_OK)
        {
            break;
        }
    }

    // Upload complete. Free the memory
    free(data_buff);
    free(ctx);

    return status;
}


/* Blockwise Downloads related functions */

// Function to initialize the blockwise_transfer structure for downloads
static void blockwise_download_init(struct blockwise_transfer *ctx,
                                    uint8_t *data_buf,
                                    const char *path_prefix,
                                    const char *path,
                                    enum golioth_content_type content_type,
                                    write_block_cb cb,
                                    void *callback_arg)
{
    ctx->is_last = false;
    ctx->path_prefix = path_prefix;
    ctx->path = path;
    ctx->content_type = content_type;
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
                                         ctx->content_type,
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
                                          enum golioth_content_type content_type,
                                          write_block_cb cb,
                                          void *callback_arg)
{
    enum golioth_status status = GOLIOTH_ERR_FAIL;
    if (!client || !path || !cb)
    {
        return status;
    }

    struct blockwise_transfer *ctx = malloc(sizeof(struct blockwise_transfer));
    if (!ctx)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    uint8_t *data_buff = malloc(CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_BUFFER_SIZE);
    if (!data_buff)
    {
        free(ctx);
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    blockwise_download_init(ctx, data_buff, path_prefix, path, content_type, cb, callback_arg);
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
