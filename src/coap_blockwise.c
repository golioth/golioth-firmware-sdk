/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/client.h>
#include <string.h>
#include <golioth/golioth_debug.h>
#include <assert.h>
#include "coap_client.h"
#include "coap_blockwise.h"

LOG_TAG_DEFINE(coap_blockwise);

_Static_assert(BLOCKSIZE_TO_SZX(CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE) != -1,
               "GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE must be "
               "one of the following: 16,32,64,128,256,512,1024");
_Static_assert(BLOCKSIZE_TO_SZX(CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE) != -1,
               "GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE must be "
               "one of the following: 16,32,64,128,256,512,1024");

struct blockwise_transfer
{
    bool is_last;
    enum golioth_content_type content_type;
    golioth_sys_sem_t sem;
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
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
    enum golioth_status status;
    struct golioth_coap_rsp_code coap_rsp_code;
    void *callback_arg;
};

struct post_block_ctx
{
    enum golioth_status status;
    struct golioth_coap_rsp_code coap_rsp_code;
    golioth_sys_sem_t sem;
    size_t negotiated_blocksize_szx;
};

struct get_block_ctx
{
    uint8_t *buffer;
    size_t rcvd_bytes;
    enum golioth_status status;
    struct golioth_coap_rsp_code coap_rsp_code;
    golioth_sys_sem_t sem;
    bool is_last;
};

/* Blockwise Uploads related functions */

static void blockwise_upload_free(struct blockwise_transfer *ctx)
{
    if (!ctx)
    {
        return;
    }

    golioth_sys_sem_destroy(ctx->sem);
    free(ctx->block_buffer);
}

// Function to initialize the blockwise_transfer structure for uploads
struct blockwise_transfer *blockwise_upload_init(const char *path_prefix,
                                                 const char *path,
                                                 enum golioth_content_type content_type,
                                                 size_t block_buffer_size)
{
    if (strlen(path) > CONFIG_GOLIOTH_COAP_MAX_PATH_LEN)
    {
        GLTH_LOGE(TAG, "Path too long: %zu > %zu", strlen(path), CONFIG_GOLIOTH_COAP_MAX_PATH_LEN);
        return NULL;
    }

    struct blockwise_transfer *ctx = golioth_sys_malloc(sizeof(struct blockwise_transfer));
    if (NULL == ctx)
    {
        goto finish;
    }

    if (block_buffer_size)
    {
        ctx->block_buffer = golioth_sys_malloc(block_buffer_size);
        if (NULL == ctx->block_buffer)
        {
            goto finish_with_ctx;
        }
    }
    else
    {
        ctx->block_buffer = NULL;
    }

    ctx->sem = golioth_sys_sem_create(1, 0);
    if (NULL == ctx->sem)
    {
        goto finish_with_buff;
    }

    ctx->is_last = false;
    ctx->path_prefix = path_prefix;
    ctx->path = path;
    ctx->content_type = content_type;
    ctx->block_size = CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE;
    ctx->block_idx = 0;
    ctx->callback_arg = NULL;
    ctx->callback.read_cb = NULL; /* sets all union members to NULL */
    golioth_coap_next_token(ctx->token);

    return ctx;

finish_with_buff:
    free(ctx->block_buffer);

finish_with_ctx:
    free(ctx);

finish:
    return NULL;
}

// Blockwise upload's internal callback function that the COAP client calls
static void on_block_sent(struct golioth_client *client,
                          enum golioth_status status,
                          const struct golioth_coap_rsp_code *coap_rsp_code,
                          const char *path,
                          size_t block_size,
                          void *arg)
{
    assert(arg);
    struct post_block_ctx *ctx = arg;
    ctx->status = status;
    ctx->coap_rsp_code.code_class = coap_rsp_code->code_class;
    ctx->coap_rsp_code.code_detail = coap_rsp_code->code_detail;
    ctx->negotiated_blocksize_szx = BLOCKSIZE_TO_SZX(block_size);

    golioth_sys_sem_give(ctx->sem);
}

// Function to call the application's read block callback for obtaining
// blockwise upload data
static enum golioth_status call_read_block_callback(struct blockwise_transfer *ctx,
                                                    size_t *block_buffer_len)
{
    /* Do not allow user callback to directly change ctx->block_size value */
    size_t block_size = ctx->block_size;

    enum golioth_status status = ctx->callback.read_cb(ctx->block_idx,
                                                       ctx->block_buffer,
                                                       &block_size,
                                                       &ctx->is_last,
                                                       ctx->callback_arg);
    if (status != GOLIOTH_OK)
    {
        return status;
    }

    if ((ctx->is_last == false && block_size != ctx->block_size) || block_size > ctx->block_size
        || block_size == 0)
    {
        return GOLIOTH_ERR_INVALID_BLOCK_SIZE;
    }

    *block_buffer_len = block_size;
    return status;
}

// Return the next block idx based on a smaller block size
static int recalculate_next_block_idx(int from_idx, size_t from_larger_szx, size_t to_smaller_szx)
{
    assert(from_larger_szx >= to_smaller_szx);

    int next_idx_before_recalc = from_idx + 1;
    int size_change_multiplier = (1 << (from_larger_szx - to_smaller_szx));

    return next_idx_before_recalc * size_change_multiplier;
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
        .negotiated_blocksize_szx = BLOCKSIZE_TO_SZX(ctx->block_size),
    };

    enum golioth_status err = golioth_coap_client_set_block(client,
                                                            ctx->token,
                                                            ctx->path_prefix,
                                                            ctx->path,
                                                            ctx->is_last,
                                                            ctx->content_type,
                                                            ctx->block_idx,
                                                            arg.negotiated_blocksize_szx,
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

        if (arg.negotiated_blocksize_szx < BLOCKSIZE_TO_SZX(ctx->block_size))
        {
            /* Recalculate index so what was sent is now based on the new block_size */
            int next_block_idx = recalculate_next_block_idx(ctx->block_idx,
                                                            BLOCKSIZE_TO_SZX(ctx->block_size),
                                                            arg.negotiated_blocksize_szx);

            /* Subtract 1 to indicate the block we just sent; inc happens after return */
            ctx->block_idx = next_block_idx - 1;

            /* Store the new blocksize for future blocks */
            ctx->block_size = SZX_TO_BLOCKSIZE(arg.negotiated_blocksize_szx);
        }
    }

    return err;
}

// Function to manage blockwise upload and handle errors
static enum golioth_status process_blockwise_uploads(struct golioth_client *client,
                                                     struct blockwise_transfer *ctx)
{
    size_t block_buffer_len = ctx->block_size;
    enum golioth_status status = call_read_block_callback(ctx, &block_buffer_len);
    if (status == GOLIOTH_OK)
    {
        status = upload_single_block(client, ctx, block_buffer_len);
        if (status == GOLIOTH_OK)
        {
            /* Only advance block_idx if block was uploaded successfully */
            ctx->block_idx++;
        }
    }
    return status;
}

enum golioth_status golioth_blockwise_post(struct golioth_client *client,
                                           const char *path_prefix,
                                           const char *path,
                                           enum golioth_content_type content_type,
                                           read_block_cb read_cb,
                                           golioth_set_cb_fn set_cb,
                                           void *callback_arg)
{
    enum golioth_status status = GOLIOTH_ERR_FAIL;
    if (!client || !path || !read_cb)
    {
        return status;
    }

    struct blockwise_transfer *ctx =
        blockwise_upload_init(path_prefix,
                              path,
                              content_type,
                              CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE);
    if (!ctx)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ctx->callback.read_cb = read_cb;
    ctx->callback_arg = callback_arg;

    while (!ctx->is_last)
    {
        if ((status = process_blockwise_uploads(client, ctx)) != GOLIOTH_OK)
        {
            break;
        }
    }

    if (set_cb)
    {

        if (status != GOLIOTH_OK)
        {
            set_cb(client, status, NULL, path, callback_arg);
        }
        else
        {
            struct golioth_coap_rsp_code *rsp_code = NULL;

            if (ctx->status == GOLIOTH_OK || ctx->status == GOLIOTH_ERR_COAP_RESPONSE)
            {
                rsp_code = &ctx->coap_rsp_code;
            }
            set_cb(client, ctx->status, rsp_code, path, callback_arg);
        }
    }

    /* Upload complete, clean up allocated resources */
    blockwise_upload_free(ctx);

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
    ctx->block_size = CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE;
    ctx->block_idx = 0;
    ctx->block_buffer = data_buf;
    ctx->callback_arg = callback_arg;
    ctx->callback.write_cb = cb;
    golioth_coap_next_token(ctx->token);
}

// Function to call the application's write block callback after a successful
// blockwise download
static enum golioth_status call_write_block_callback(struct blockwise_transfer *ctx,
                                                     size_t rcvd_bytes)
{
    enum golioth_status status = ctx->callback.write_cb(ctx->block_idx,
                                                        ctx->block_buffer,
                                                        rcvd_bytes,
                                                        ctx->is_last,
                                                        ctx->block_size,
                                                        ctx->callback_arg);

    if (GOLIOTH_OK == status)
    {
        /* Only advance block_idx if block was stored successfully */
        ctx->block_idx++;
    }

    return status;
}

// Blockwise download's internal callback function that the COAP client calls
static void on_block_rcvd(struct golioth_client *client,
                          enum golioth_status status,
                          const struct golioth_coap_rsp_code *coap_rsp_code,
                          const char *path,
                          const uint8_t *payload,
                          size_t payload_size,
                          bool is_last,
                          void *arg)
{
    // assert valid values of arg, payload size and block_buffer
    assert(arg);
    assert(payload_size <= CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE);
    struct get_block_ctx *ctx = arg;
    assert(ctx->buffer);

    // copy blockwise download values returned by COAP client into ctx
    ctx->status = status;
    ctx->is_last = is_last;
    memcpy(ctx->buffer, payload, payload_size);
    ctx->rcvd_bytes = payload_size;

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
                                                            ctx->token,
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

// Function to manage blockwise downloads and handle errors
static enum golioth_status process_blockwise_downloads(struct golioth_client *client,
                                                       struct blockwise_transfer *ctx)
{
    size_t rcvd_bytes = 0;
    enum golioth_status status = download_single_block(client, ctx, &rcvd_bytes);

    if (status == GOLIOTH_OK)
    {
        status = call_write_block_callback(ctx, rcvd_bytes);
    }

    return status;
}

enum golioth_status golioth_blockwise_get(struct golioth_client *client,
                                          const char *path_prefix,
                                          const char *path,
                                          enum golioth_content_type content_type,
                                          uint32_t *block_idx,
                                          write_block_cb cb,
                                          void *callback_arg)
{
    enum golioth_status status = GOLIOTH_ERR_FAIL;
    if (!client || !path || !cb)
    {
        return GOLIOTH_ERR_NULL;
    }

    struct blockwise_transfer *ctx = malloc(sizeof(struct blockwise_transfer));
    if (!ctx)
    {
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish;
    }

    uint8_t *data_buff = malloc(CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE);
    if (!data_buff)
    {
        status = GOLIOTH_ERR_MEM_ALLOC;
        goto finish_with_ctx;
    }

    blockwise_download_init(ctx, data_buff, path_prefix, path, content_type, cb, callback_arg);

    if (block_idx)
    {
        ctx->block_idx = *block_idx;
    }

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

    if (block_idx)
    {
        *block_idx = ctx->block_idx;
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
