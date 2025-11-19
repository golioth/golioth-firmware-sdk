/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <golioth/client.h>
#include <golioth/golioth_debug.h>
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
    struct golioth_client *client;
    enum golioth_content_type content_type;
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    const char *path_prefix;
    char path[CONFIG_GOLIOTH_COAP_MAX_PATH_LEN + 1];
    enum golioth_coap_request_type type;

    enum golioth_status status;
    struct golioth_coap_rsp_code coap_rsp_code;
};

struct post_block_ctx
{
    enum golioth_status status;
    struct golioth_coap_rsp_code coap_rsp_code;
    golioth_sys_sem_t sem;
    size_t negotiated_blocksize_szx;

    bool is_last;
    size_t block_size;
    uint32_t block_idx;
    uint8_t *block_buffer;
    read_block_cb read_cb;
    void *callback_arg;

    struct blockwise_transfer transfer_ctx;
};

struct get_block_ctx
{
    size_t block_size;
    uint32_t block_idx;
    bool post_response;
    golioth_get_block_cb_fn get_cb;
    golioth_end_block_cb_fn end_cb;
    void *callback_arg;

    struct blockwise_transfer transfer_ctx;
};

static void on_block_rcvd(struct golioth_client *client,
                          enum golioth_status status,
                          const struct golioth_coap_rsp_code *coap_rsp_code,
                          const char *path,
                          const uint8_t *payload,
                          size_t payload_size,
                          bool is_last,
                          void *arg);

// Function to initialize the blockwise_transfer structure
static int blockwise_transfer_init(struct blockwise_transfer *ctx,
                                   struct golioth_client *client,
                                   const char *path_prefix,
                                   const char *path,
                                   enum golioth_content_type content_type)
{
    if (strlen(path) > CONFIG_GOLIOTH_COAP_MAX_PATH_LEN)
    {
        return -EINVAL;
    }
    strncpy(ctx->path, path, CONFIG_GOLIOTH_COAP_MAX_PATH_LEN + 1);

    ctx->client = client;
    ctx->path_prefix = path_prefix;
    ctx->content_type = content_type;
    golioth_coap_next_token(ctx->token);

    return 0;
}

/* Blockwise Uploads related functions */

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
    if (NULL != coap_rsp_code)
    {
        ctx->coap_rsp_code.code_class = coap_rsp_code->code_class;
        ctx->coap_rsp_code.code_detail = coap_rsp_code->code_detail;
    }
    ctx->negotiated_blocksize_szx = BLOCKSIZE_TO_SZX(block_size);

    golioth_sys_sem_give(ctx->sem);
}

// Function to call the application's read block callback for obtaining
// blockwise upload data
static enum golioth_status call_read_block_callback(struct post_block_ctx *ctx,
                                                    size_t *block_buffer_len)
{
    /* Do not allow user callback to directly change ctx->block_size value */
    size_t block_size = ctx->block_size;

    enum golioth_status status = ctx->read_cb(ctx->block_idx,
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
                                               struct post_block_ctx *ctx,
                                               size_t block_size)
{
    enum golioth_status err = golioth_coap_client_set_block(client,
                                                            ctx->transfer_ctx.token,
                                                            ctx->transfer_ctx.path_prefix,
                                                            ctx->transfer_ctx.path,
                                                            ctx->is_last,
                                                            ctx->transfer_ctx.content_type,
                                                            ctx->block_idx,
                                                            ctx->negotiated_blocksize_szx,
                                                            ctx->block_buffer,
                                                            block_size,
                                                            on_block_sent,
                                                            ctx,
                                                            NULL,
                                                            NULL,
                                                            false,
                                                            GOLIOTH_SYS_WAIT_FOREVER);

    if (GOLIOTH_OK == err)
    {
        golioth_sys_sem_take(ctx->sem, GOLIOTH_SYS_WAIT_FOREVER);
        err = ctx->status;

        if (ctx->negotiated_blocksize_szx < BLOCKSIZE_TO_SZX(ctx->block_size))
        {
            /* Recalculate index so what was sent is now based on the new block_size */
            int next_block_idx = recalculate_next_block_idx(ctx->block_idx,
                                                            BLOCKSIZE_TO_SZX(ctx->block_size),
                                                            ctx->negotiated_blocksize_szx);

            /* Subtract 1 to indicate the block we just sent; inc happens after return */
            ctx->block_idx = next_block_idx - 1;

            /* Store the new blocksize for future blocks */
            ctx->block_size = SZX_TO_BLOCKSIZE(ctx->negotiated_blocksize_szx);
        }
    }

    return err;
}

// Function to manage blockwise upload and handle errors
static enum golioth_status process_blockwise_uploads(struct golioth_client *client,
                                                     struct post_block_ctx *ctx)
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
    if (NULL == client || NULL == path || NULL == read_cb)
    {
        return status;
    }

    status = GOLIOTH_ERR_MEM_ALLOC;

    struct post_block_ctx ctx;

    if (0 != blockwise_transfer_init(&ctx.transfer_ctx, client, path_prefix, path, content_type))
    {
        return GOLIOTH_ERR_FAIL;
    }
    ctx.transfer_ctx.type = GOLIOTH_COAP_REQUEST_POST_BLOCK;

    ctx.block_buffer = golioth_sys_malloc(CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE);
    if (NULL == ctx.block_buffer)
    {
        goto finish;
    }

    ctx.sem = golioth_sys_sem_create(1, 0);
    if (NULL == ctx.sem)
    {
        goto finish_with_block_buffer;
    }

    ctx.status = GOLIOTH_ERR_FAIL;
    ctx.is_last = false;
    ctx.block_size = CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE;
    ctx.block_idx = 0;
    ctx.read_cb = read_cb;
    ctx.callback_arg = callback_arg;
    ctx.negotiated_blocksize_szx = BLOCKSIZE_TO_SZX(CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE);

    while (!ctx.is_last)
    {
        if ((status = process_blockwise_uploads(client, &ctx)) != GOLIOTH_OK)
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

            if (ctx.status == GOLIOTH_OK || ctx.status == GOLIOTH_ERR_COAP_RESPONSE)
            {
                rsp_code = &ctx.coap_rsp_code;
            }
            set_cb(client, ctx.status, rsp_code, path, callback_arg);
        }
    }

    /* Upload complete, clean up allocated resources */
    golioth_sys_sem_destroy(ctx.sem);

finish_with_block_buffer:
    free(ctx.block_buffer);

finish:
    return status;
}

// Create an async upload context
struct blockwise_transfer *golioth_blockwise_upload_start(struct golioth_client *client,
                                                          const char *path_prefix,
                                                          const char *path,
                                                          enum golioth_content_type content_type)
{
    if (NULL == client || NULL == path_prefix || NULL == path)
    {
        return NULL;
    }

    if (strlen(path) > CONFIG_GOLIOTH_COAP_MAX_PATH_LEN)
    {
        GLTH_LOGE(TAG, "Path too long: %zu > %zu", strlen(path), CONFIG_GOLIOTH_COAP_MAX_PATH_LEN);
        return NULL;
    }

    struct blockwise_transfer *ctx = golioth_sys_malloc(sizeof(struct blockwise_transfer));
    if (NULL == ctx)
    {
        return NULL;
    }

    if (0 != blockwise_transfer_init(ctx, client, path_prefix, path, content_type))
    {
        goto finish_with_ctx;
    }
    ctx->type = GOLIOTH_COAP_REQUEST_POST_BLOCK;

    return ctx;

finish_with_ctx:
    golioth_sys_free(ctx);
    return NULL;
}

// Destroy an async upload context
void golioth_blockwise_upload_finish(struct blockwise_transfer *ctx)
{
    if (NULL == ctx)
    {
        GLTH_LOGW(TAG, "Attempt to free NULL context");
        return;
    }

    golioth_sys_free(ctx);
}

// Send a single block asynchronously
enum golioth_status golioth_blockwise_upload_block(struct blockwise_transfer *ctx,
                                                   uint32_t block_idx,
                                                   const uint8_t *block_buffer,
                                                   size_t block_len,
                                                   bool is_last,
                                                   golioth_set_block_cb_fn set_cb,
                                                   golioth_get_block_cb_fn get_cb,
                                                   golioth_end_block_cb_fn end_cb,
                                                   void *callback_arg,
                                                   void *rsp_callback_arg,
                                                   bool is_synchronous,
                                                   int32_t timeout_s)
{
    if (NULL == ctx || NULL == block_buffer)
    {
        return GOLIOTH_ERR_NULL;
    }

    struct get_block_ctx *rsp_ctx = NULL;
    coap_get_block_cb_fn rsp_cb = NULL;
    if (is_last && NULL != get_cb && NULL != end_cb)
    {
        rsp_ctx = malloc(sizeof(struct get_block_ctx));
        rsp_ctx->block_idx = 0;
        rsp_ctx->block_size = CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE;
        rsp_ctx->post_response = true;
        memcpy(&rsp_ctx->transfer_ctx, ctx, sizeof(struct blockwise_transfer));
        rsp_ctx->get_cb = get_cb;
        rsp_ctx->end_cb = end_cb;
        rsp_ctx->callback_arg = rsp_callback_arg;

        rsp_cb = on_block_rcvd;
    }

    return golioth_coap_client_set_block(
        ctx->client,
        ctx->token,
        ctx->path_prefix,
        ctx->path,
        is_last,
        ctx->content_type,
        block_idx,
        BLOCKSIZE_TO_SZX(CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE),
        block_buffer,
        block_len,
        set_cb,
        callback_arg,
        rsp_cb,
        rsp_ctx,
        is_synchronous,
        timeout_s);
}

/* Blockwise Downloads related functions */

static enum golioth_status download_single_block(struct golioth_client *client,
                                                 struct get_block_ctx *ctx);

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

    if (GOLIOTH_OK == status)
    {
        status = ctx->get_cb(client,
                             path,
                             ctx->block_idx,
                             payload,
                             payload_size,
                             is_last,
                             ctx->block_size,
                             ctx->callback_arg);
    }

    /* If we are ending due to an error on the first block of a response to a post,
       then the error was already sent to the application via the set_block callback.
       Just free the context but do not notify the application. */
    if (ctx->post_response && ctx->block_idx == 0 && GOLIOTH_OK != status)
    {
        golioth_sys_free(ctx);
    }
    else if (is_last || GOLIOTH_OK != status)
    {
        ctx->end_cb(client, status, coap_rsp_code, path, ctx->block_idx, ctx->callback_arg);

        golioth_sys_free(ctx);
    }
    else
    {
        ctx->block_idx++;
        status = download_single_block(client, ctx);
        if (GOLIOTH_OK != status)
        {
            ctx->end_cb(client, status, NULL, path, ctx->block_idx, ctx->callback_arg);

            golioth_sys_free(ctx);
        }
    }
}

// Function to download a single block
static enum golioth_status download_single_block(struct golioth_client *client,
                                                 struct get_block_ctx *ctx)
{
    if (GOLIOTH_COAP_REQUEST_GET_BLOCK == ctx->transfer_ctx.type)
    {
        return golioth_coap_client_get_block(client,
                                             ctx->transfer_ctx.token,
                                             ctx->transfer_ctx.path_prefix,
                                             ctx->transfer_ctx.path,
                                             ctx->transfer_ctx.content_type,
                                             ctx->block_idx,
                                             ctx->block_size,
                                             on_block_rcvd,
                                             ctx,
                                             false,
                                             GOLIOTH_SYS_WAIT_FOREVER);
    }
    else
    {
        return golioth_coap_client_get_rsp_block(client,
                                                 ctx->transfer_ctx.token,
                                                 ctx->transfer_ctx.path_prefix,
                                                 ctx->transfer_ctx.path,
                                                 ctx->transfer_ctx.content_type,
                                                 ctx->block_idx,
                                                 ctx->block_size,
                                                 on_block_rcvd,
                                                 ctx,
                                                 false,
                                                 GOLIOTH_SYS_WAIT_FOREVER);
    }
}

enum golioth_status golioth_blockwise_get(struct golioth_client *client,
                                          const char *path_prefix,
                                          const char *path,
                                          enum golioth_content_type content_type,
                                          uint32_t block_idx,
                                          golioth_get_block_cb_fn block_cb,
                                          golioth_end_block_cb_fn end_cb,
                                          void *callback_arg)
{
    if (NULL == client || NULL == path || NULL == block_cb || NULL == end_cb)
    {
        return GOLIOTH_ERR_NULL;
    }

    struct get_block_ctx *ctx = golioth_sys_malloc(sizeof(struct get_block_ctx));
    if (NULL == ctx)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    if (0 != blockwise_transfer_init(&ctx->transfer_ctx, client, path_prefix, path, content_type))
    {
        golioth_sys_free(ctx);
        return GOLIOTH_ERR_FAIL;
    }
    ctx->transfer_ctx.type = GOLIOTH_COAP_REQUEST_GET_BLOCK;

    ctx->block_size = CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE;
    ctx->post_response = false;
    ctx->get_cb = block_cb;
    ctx->end_cb = end_cb;
    ctx->callback_arg = callback_arg;
    ctx->block_idx = block_idx;

    return download_single_block(client, ctx);
}
