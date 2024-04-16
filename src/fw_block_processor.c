/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "fw_block_processor.h"
#include "golioth_util.h"
#include <golioth/fw_update.h>
#include <string.h>
#include <assert.h>

#if defined(CONFIG_GOLIOTH_FW_UPDATE)

LOG_TAG_DEFINE(fw_block_processor);

static void block_stats_init(block_latency_stats_t *stats)
{
    stats->block_min_ms = UINT32_MAX;
    stats->block_max_ms = 0;
}

static void block_stats_update(block_latency_stats_t *stats, uint32_t block_latency_ms)
{
    stats->block_min_ms = min(stats->block_min_ms, block_latency_ms);
    stats->block_max_ms = max(stats->block_max_ms, block_latency_ms);
}

static void download_init(download_ctx_t *ctx,
                          struct golioth_client *client,
                          const struct golioth_ota_component *ota_component,
                          uint8_t *download_buf)
{
    memset(ctx, 0, sizeof(*ctx));
    block_stats_init(&ctx->block_stats);
    ctx->ota_component = ota_component;
    ctx->client = client;

    assert(download_buf);
    ctx->download_buf = download_buf;

    // Note: total_num_blocks is an estimate of the number of blocks required to download,
    // based on the size of the component reported in the manifest.
    //
    // Due to the way this size is populated in the manifest on the server, the actual
    // number of blocks may be different.
    ctx->total_num_blocks = golioth_ota_size_to_nblocks(ota_component->size);
}

static enum golioth_status download_block(download_ctx_t *ctx)
{
    if (ctx->is_last_block)
    {
        // We've already downloaded the last block
        return GOLIOTH_ERR_NO_MORE_DATA;
    }

    const uint64_t download_start_ms = golioth_sys_now_ms();

    GLTH_LOGI(TAG,
              "Downloading block index %" PRIu32 " (%" PRIu32 "/%" PRIu32 ")",
              (uint32_t) ctx->block_index,
              (uint32_t) ctx->block_index + 1,
              (uint32_t) ctx->total_num_blocks);

    enum golioth_status status;
    int retry_count = 3;
    while (retry_count > 0)
    {
        status = golioth_ota_get_block_sync(ctx->client,
                                            ctx->ota_component->package,
                                            ctx->ota_component->version,
                                            ctx->block_index,
                                            ctx->download_buf,
                                            &ctx->block_bytes_downloaded,
                                            &ctx->is_last_block,
                                            GOLIOTH_SYS_WAIT_FOREVER);

        if (status == GOLIOTH_OK)
        {
            break;
        }
        else if (retry_count == 1)
        {
            GLTH_LOGE(TAG, "Unable to download block. Status: %d", status);
            return status;
        }
        else
        {
            GLTH_LOGW(TAG, "Failed to get block, will retry. Status: %d", status);
            golioth_sys_msleep(150);
            retry_count--;
        }
    }

    assert(ctx->block_bytes_downloaded <= GOLIOTH_OTA_BLOCKSIZE);
    block_stats_update(&ctx->block_stats, golioth_sys_now_ms() - download_start_ms);

    ctx->block_index++;

    assert(ctx->output_fn);
    return ctx->output_fn(ctx->download_buf, ctx->block_bytes_downloaded, ctx->output_fn_arg);
}

static void decompress_init(decompress_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));

#if CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_HEATSHRINK
    GLTH_LOGI(TAG, "Enabling heatshrink decompression");
    heatshrink_decoder_reset(&ctx->hsd);
#elif CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_ZLIB
    GLTH_LOGI(TAG, "Enabling zlib decompression");
    tinfl_init(&ctx->decompressor);
#endif
}

static int patch_old_read(const struct bspatch_stream_i *stream, void *buffer, int pos, int len)
{
    enum golioth_status status = fw_update_read_current_image_at_offset(buffer, len, pos);
    if (status != GOLIOTH_OK)
    {
        return -1;
    }
    return 0;
}

static int patch_new_write(const struct bspatch_stream_n *stream, const void *buffer, int length)
{
    patch_ctx_t *ctx = (patch_ctx_t *) stream->opaque;
    assert(ctx->output_fn);
    enum golioth_status status = ctx->output_fn(buffer, length, ctx->output_fn_arg);
    if (status != GOLIOTH_OK)
    {
        return -1;
    }
    return 0;
}

static void patch_init(patch_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->old_stream.read = patch_old_read;
    ctx->old_stream.opaque = ctx;
    ctx->new_stream.write = patch_new_write;
    ctx->new_stream.opaque = ctx;

#if CONFIG_GOLIOTH_OTA_PATCH
    GLTH_LOGI(TAG, "Patching enabled");
#endif
}

static void handle_block_init(handle_block_ctx_t *ctx, size_t component_size)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->component_size = component_size;
}

static enum golioth_status handle_block(const uint8_t *in_data, size_t in_data_size, void *arg)
{
    handle_block_ctx_t *ctx = (handle_block_ctx_t *) arg;

    enum golioth_status status = fw_update_handle_block(in_data,
                                                        in_data_size,
                                                        ctx->bytes_handled,  // offset
                                                        ctx->component_size);

    ctx->bytes_handled += in_data_size;
    return status;
}

#if CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_HEATSHRINK
static enum golioth_status decompress_heatshrink(decompress_ctx_t *ctx,
                                                 const uint8_t *in_data,
                                                 size_t in_data_size)
{
    size_t total_sunk = 0;
    while (total_sunk < in_data_size)
    {
        // Sink the compressed data
        size_t sunk = 0;
        HSD_sink_res sink_res = heatshrink_decoder_sink(&ctx->hsd,
                                                        (uint8_t *) &in_data[total_sunk],
                                                        in_data_size - total_sunk,
                                                        &sunk);
        if (sink_res != HSDR_SINK_OK)
        {
            GLTH_LOGE(TAG, "sink error: %d, sunk = %" PRIu32, sink_res, (uint32_t) sunk);
            return GOLIOTH_ERR_FAIL;
        }
        total_sunk += sunk;

        // Pull the uncompressed data out of the decoder
        HSD_poll_res pres;
        uint8_t decode_buffer[HEATSHRINK_STATIC_INPUT_BUFFER_SIZE];
        do
        {
            size_t poll_sz = 0;
            pres =
                heatshrink_decoder_poll(&ctx->hsd, decode_buffer, sizeof(decode_buffer), &poll_sz);
            if (pres < 0)
            {
                GLTH_LOGE(TAG, "poll error: %d", pres);
                return GOLIOTH_ERR_FAIL;
            }

            GOLIOTH_STATUS_RETURN_IF_ERROR(
                ctx->output_fn(decode_buffer, poll_sz, ctx->output_fn_arg));
            ctx->bytes_out += poll_sz;
        } while (pres == HSDR_POLL_MORE);
    }

    return GOLIOTH_OK;
}
#endif

#if CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_ZLIB
static enum golioth_status decompress_zlib(decompress_ctx_t *ctx,
                                           const uint8_t *in_data,
                                           size_t in_data_size)
{
    GLTH_LOGD(TAG, "tinfl_decompress input: %zd", in_data_size);

    size_t in_buf_ofs = 0;
    tinfl_status status = TINFL_STATUS_HAS_MORE_OUTPUT;
    while (status == TINFL_STATUS_HAS_MORE_OUTPUT)
    {
        // This is basically the inner loop of tinfl_decompress_mem_to_callback() in miniz_tinfl.c
        size_t in_buf_size = in_data_size - in_buf_ofs;
        size_t dst_buf_size = TINFL_LZ_DICT_SIZE - ctx->dict_ofs;

        mz_uint32 flags = TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_HAS_MORE_INPUT;

        status = tinfl_decompress(&ctx->decompressor,
                                  (const mz_uint8 *) in_data + in_buf_ofs,
                                  &in_buf_size,
                                  ctx->zlib_dict,
                                  ctx->zlib_dict + ctx->dict_ofs,
                                  &dst_buf_size,
                                  flags);
        in_buf_ofs += in_buf_size;

        GLTH_LOGD(TAG, "tinfl_decompress status: %" PRId32, (int32_t) status);

        // If we got a negative status, return error
        if (status < 0)
        {
            GLTH_LOGE(TAG, "tinfl_decompress error: %" PRId32, (int32_t) status);
            return GOLIOTH_ERR_FAIL;
        }

        if (dst_buf_size > 0)
        {
            // output was generated by tinfl_decompress, forward to output_fn
            GOLIOTH_STATUS_RETURN_IF_ERROR(
                ctx->output_fn(ctx->zlib_dict + ctx->dict_ofs, dst_buf_size, ctx->output_fn_arg));
            ctx->bytes_out += dst_buf_size;
        }

        ctx->dict_ofs = (ctx->dict_ofs + dst_buf_size) & (TINFL_LZ_DICT_SIZE - 1);
    }

    return GOLIOTH_OK;
}
#endif

static enum golioth_status decompress_none(decompress_ctx_t *ctx,
                                           const uint8_t *in_data,
                                           size_t in_data_size)
{
    GOLIOTH_STATUS_RETURN_IF_ERROR(ctx->output_fn(in_data, in_data_size, ctx->output_fn_arg));
    ctx->bytes_out += in_data_size;
    return GOLIOTH_OK;
}

static enum golioth_status decompress(const uint8_t *in_data, size_t in_data_size, void *arg)
{
    decompress_ctx_t *ctx = (decompress_ctx_t *) arg;
    assert(ctx->output_fn);

    ctx->bytes_in += in_data_size;

#if CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_HEATSHRINK
    return decompress_heatshrink(ctx, in_data, in_data_size);
#elif CONFIG_GOLIOTH_OTA_DECOMPRESS_METHOD_ZLIB
    return decompress_zlib(ctx, in_data, in_data_size);
#endif

    return decompress_none(ctx, in_data, in_data_size);

    // TODO - compare decompressed size with size from the manifest
    // TODO - compute sha256 of decompressed image and verify it matches manifest
}

static enum golioth_status patch(const uint8_t *in_data, size_t in_data_size, void *arg)
{
    patch_ctx_t *ctx = (patch_ctx_t *) arg;
    assert(ctx->output_fn);

#if CONFIG_GOLIOTH_OTA_PATCH == 0
    // no patching required
    return ctx->output_fn(in_data, in_data_size, ctx->output_fn_arg);
#endif

    // Note: bspatch() will write data to new_stream, which calls patch_new_write()
    int err = bspatch(&ctx->bspatch_ctx, &ctx->old_stream, &ctx->new_stream, in_data, in_data_size);
    if (err != BSPATCH_SUCCESS)
    {
        GLTH_LOGE(TAG, "patch error: %d", err);
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

void fw_block_processor_init(fw_block_processor_ctx_t *ctx,
                             struct golioth_client *client,
                             const struct golioth_ota_component *component,
                             uint8_t *download_buf)
{
    memset(ctx, 0, sizeof(*ctx));

    download_init(&ctx->download, client, component, download_buf);
    decompress_init(&ctx->decompress);
    patch_init(&ctx->patch);
    handle_block_init(&ctx->handle_block, component->size);

    // Connect output of download to input of decompress
    ctx->download.output_fn = decompress;
    ctx->download.output_fn_arg = &ctx->decompress;

    // Connect output of decompress to input of patch
    ctx->decompress.output_fn = patch;
    ctx->decompress.output_fn_arg = &ctx->patch;

    // Connect output of patch to input of handle_block
    ctx->patch.output_fn = handle_block;
    ctx->patch.output_fn_arg = &ctx->handle_block;
}

enum golioth_status fw_block_processor_process(fw_block_processor_ctx_t *ctx)
{
    // Call the first function in the block processing chain.
    return download_block(&ctx->download);
}

void fw_block_processor_log_results(const fw_block_processor_ctx_t *ctx)
{
    GLTH_LOGI(TAG, "Block Latency Stats:");
    GLTH_LOGI(TAG, "   Min: %" PRIu32 " ms", ctx->download.block_stats.block_min_ms);
    GLTH_LOGI(TAG, "   Max: %" PRIu32 " ms", ctx->download.block_stats.block_max_ms);
    GLTH_LOGI(TAG, "Total bytes written: %" PRIu32, (uint32_t) ctx->handle_block.bytes_handled);

    int32_t decompress_delta = ctx->decompress.bytes_out - ctx->decompress.bytes_in;
    if (decompress_delta > 0)
    {
        GLTH_LOGI(TAG, "Compression saved %" PRId32 " bytes", decompress_delta);
    }
}

#endif  // CONFIG_GOLIOTH_FW_UPDATE
