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
    enum golioth_content_type content_type;
    golioth_sys_sem_t sem;
    uint32_t block_idx;
    /* Negotiated block size */
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

struct post_block_ctx
{
    enum golioth_status status;
    golioth_sys_sem_t sem;
};

struct get_block_ctx
{
    uint8_t *buffer;
    size_t rcvd_bytes;
    enum golioth_status status;
    golioth_sys_sem_t sem;
    bool is_last;
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
    ctx->block_size = CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_BLOCK_SIZE;
    ctx->block_idx = 0;
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
    struct post_block_ctx *ctx = arg;
    ctx->status = response->status;

    golioth_sys_sem_give(ctx->sem);
}

// Function to call the application's read block callback after a successful
// blockwise upload
static int call_read_block_callback(struct blockwise_transfer *ctx)
{
    size_t block_size = ctx->block_size;
    int err = ctx->callback.read_cb(ctx->block_idx,
                                    ctx->block_buffer,
                                    &block_size,
                                    &ctx->is_last,
                                    ctx->callback_arg);
    if (err != GOLIOTH_OK)
    {
        return -err;
    }
    return block_size;
}

// Function to upload a single block
static enum golioth_status upload_single_block(struct golioth_client *client,
                                               struct blockwise_transfer *ctx,
                                               size_t block_size)
{
    assert(block_size > 0 && block_size <= ctx->block_size);

    struct post_block_ctx arg = {
        .status = GOLIOTH_ERR_FAIL,
        .sem = ctx->sem,
    };

    enum golioth_status err = golioth_coap_client_set_block(client,
                                                            ctx->path_prefix,
                                                            ctx->path,
                                                            ctx->is_last,
                                                            ctx->content_type,
                                                            ctx->block_idx,
                                                            ctx->block_buffer,
                                                            block_size,
                                                            on_block_sent,
                                                            &arg,
                                                            false,
                                                            GOLIOTH_SYS_WAIT_FOREVER);
    if (GOLIOTH_OK == err)
    {
        golioth_sys_sem_take(ctx->sem, GOLIOTH_SYS_WAIT_FOREVER);
        err = arg.status;
    }

    return err;
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
    int ret = call_read_block_callback(ctx);
    if (ret < 0)
    {
        status = -ret;
    }
    else
    {
        status = upload_single_block(client, ctx, (size_t) ret);
        if (status == GOLIOTH_OK)
        {
            ctx->block_idx++;
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
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish;
    }

    uint8_t *data_buff = malloc(CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_BLOCK_SIZE);
    if (!data_buff)
    {
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish_with_ctx;
    }

    blockwise_upload_init(ctx, data_buff, path_prefix, path, content_type, cb, callback_arg);

    ctx->sem = golioth_sys_sem_create(1, 0);
    if (!ctx->sem)
    {
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish_with_buff;
    }

    while (!ctx->is_last)
    {
        if ((status = process_blockwise_uploads(client, ctx)) != GOLIOTH_OK)
        {
            break;
        }
    }

    /* Upload complete, clean up allocated resources */

    golioth_sys_sem_destroy(ctx->sem);

finish_with_buff:
    free(data_buff);

finish_with_ctx:
    free(ctx);

finish:
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
    ctx->block_size = CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_BLOCK_SIZE;
    ctx->block_idx = 0;
    ctx->block_buffer = data_buf;
    ctx->callback_arg = callback_arg;
    ctx->callback.write_cb = cb;
}

// Function to call the application's write block callback after a successful
// blockwise download
static void call_write_block_callback(struct blockwise_transfer *ctx, size_t rcvd_bytes)
{
    if (ctx->callback.write_cb(ctx->block_idx,
                               ctx->block_buffer,
                               rcvd_bytes,
                               ctx->is_last,
                               ctx->callback_arg)
        != GOLIOTH_OK)
    {
        // TODO: handle application callback error
    }
    ctx->block_idx++;
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
    struct get_block_ctx *ctx = arg;
    assert(ctx->buffer);

    // copy blockwise download values returned by COAP client into ctx
    ctx->is_last = is_last;
    memcpy(ctx->buffer, payload, payload_size);
    ctx->rcvd_bytes = payload_size;
    ctx->status = response->status;

    golioth_sys_sem_give(ctx->sem);
}

// Function to download a single block
static enum golioth_status download_single_block(struct golioth_client *client,
                                                 struct blockwise_transfer *ctx,
                                                 size_t *rcvd_bytes)
{
    struct get_block_ctx arg = {
        .is_last = false,
        .status = GOLIOTH_ERR_FAIL,
        .buffer = ctx->block_buffer,
        .rcvd_bytes = 0,
        .sem = ctx->sem,
    };

    enum golioth_status err = golioth_coap_client_get_block(client,
                                                            ctx->path_prefix,
                                                            ctx->path,
                                                            ctx->content_type,
                                                            ctx->block_idx,
                                                            ctx->block_size,
                                                            on_block_rcvd,
                                                            &arg,
                                                            false,
                                                            GOLIOTH_SYS_WAIT_FOREVER);
    if (GOLIOTH_OK == err)
    {
        golioth_sys_sem_take(ctx->sem, GOLIOTH_SYS_WAIT_FOREVER);
        err = arg.status;

        // TODO: These don't need to be stored in the ctx struct
        ctx->is_last = arg.is_last;
        *rcvd_bytes = arg.rcvd_bytes;
    }

    return err;
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
    size_t rcvd_bytes = 0;
    enum golioth_status status = download_single_block(client, ctx, &rcvd_bytes);
    if (status == GOLIOTH_OK)
    {
        call_write_block_callback(ctx, rcvd_bytes);
    }
    else
    {
        // TODO: handle_download_error
        status = handle_download_error();
    }
    return status;
}

enum golioth_status golioth_blockwise_get_from_block_idx(struct golioth_client *client,
                                                         const char *path_prefix,
                                                         const char *path,
                                                         enum golioth_content_type content_type,
                                                         write_block_cb cb,
                                                         void *callback_arg,
                                                         uint32_t block_idx)
{
    enum golioth_status status = GOLIOTH_ERR_FAIL;
    if (!client || !path || !cb)
    {
        return status;
    }

    struct blockwise_transfer *ctx = malloc(sizeof(struct blockwise_transfer));
    if (!ctx)
    {
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish;
    }

    uint8_t *data_buff = malloc(CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_BUFFER_SIZE);
    if (!data_buff)
    {
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish_with_ctx;
    }

    blockwise_download_init(ctx, data_buff, path_prefix, path, content_type, cb, callback_arg);

    ctx->block_idx = block_idx;

    ctx->sem = golioth_sys_sem_create(1, 0);
    if (!ctx->sem)
    {
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish_with_buff;
    }


    while (!ctx->is_last)
    {
        if ((status = process_blockwise_downloads(client, ctx)) != GOLIOTH_OK)
        {
            break;
        }
    }

    /* Download complete. Clean up allocated resources. */

    golioth_sys_sem_destroy(ctx->sem);

finish_with_buff:
    free(data_buff);

finish_with_ctx:
    free(ctx);

finish:
    return status;
}

enum golioth_status golioth_blockwise_get(struct golioth_client *client,
                                          const char *path_prefix,
                                          const char *path,
                                          enum golioth_content_type content_type,
                                          write_block_cb cb,
                                          void *callback_arg)
{
    return golioth_blockwise_get_from_block_idx(client,
                                                path_prefix,
                                                path,
                                                content_type,
                                                cb,
                                                callback_arg,
                                                0);
}
