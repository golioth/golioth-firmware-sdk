/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "golioth_fw_block_processor.h"
#include "golioth_util.h"
#include "golioth_fw_update.h"
#include <string.h>
#include <assert.h>

#define TAG "fw_block_processor"

static void block_stats_init(block_latency_stats_t* stats) {
    stats->block_min_ms = UINT32_MAX;
    stats->block_ema_ms = 0.0f;
    stats->block_max_ms = 0;
}

static void block_stats_update(block_latency_stats_t* stats, uint32_t block_latency_ms) {
    const float alpha = 0.01f;
    stats->block_min_ms = min(stats->block_min_ms, block_latency_ms);
    stats->block_ema_ms = alpha * (float)block_latency_ms + (1.0f - alpha) * stats->block_ema_ms;
    stats->block_max_ms = max(stats->block_max_ms, block_latency_ms);
}

static void download_init(
        download_ctx_t* ctx,
        golioth_client_t client,
        const golioth_ota_component_t* ota_component,
        uint8_t* download_buf) {
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

static golioth_status_t download_block(download_ctx_t* ctx) {
    if (ctx->is_last_block) {
        // We've already downloaded the last block
        return GOLIOTH_ERR_NO_MORE_DATA;
    }

    const uint64_t download_start_ms = golioth_sys_now_ms();

    GLTH_LOGI(
            TAG,
            "Downloading block index %" PRIu32 " (%" PRIu32 "/%" PRIu32 ")",
            (uint32_t)ctx->block_index,
            (uint32_t)ctx->block_index + 1,
            (uint32_t)ctx->total_num_blocks);

    GOLIOTH_STATUS_RETURN_IF_ERROR(golioth_ota_get_block_sync(
            ctx->client,
            ctx->ota_component->package,
            ctx->ota_component->version,
            ctx->block_index,
            ctx->download_buf,
            &ctx->block_bytes_downloaded,
            &ctx->is_last_block,
            GOLIOTH_WAIT_FOREVER));

    assert(ctx->block_bytes_downloaded <= GOLIOTH_OTA_BLOCKSIZE);
    block_stats_update(&ctx->block_stats, golioth_sys_now_ms() - download_start_ms);

    ctx->block_index++;

    assert(ctx->output_fn);
    return ctx->output_fn(ctx->download_buf, ctx->block_bytes_downloaded, ctx->output_fn_arg);
}

static void decompress_init(decompress_ctx_t* ctx, bool enable_decompression) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->is_enabled = enable_decompression;
    heatshrink_decoder_reset(&ctx->hsd);
    if (ctx->is_enabled) {
        GLTH_LOGI(TAG, "Compressed image detected");
    }
}

static void handle_block_init(handle_block_ctx_t* ctx, size_t component_size) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->component_size = component_size;
}

static golioth_status_t handle_block(const uint8_t* in_data, size_t in_data_size, void* arg) {
    handle_block_ctx_t* ctx = (handle_block_ctx_t*)arg;

    golioth_status_t status = fw_update_handle_block(
            in_data,
            in_data_size,
            ctx->bytes_handled,  // offset
            ctx->component_size);

    ctx->bytes_handled += in_data_size;
    return status;
}

static golioth_status_t decompress(const uint8_t* in_data, size_t in_data_size, void* arg) {
    decompress_ctx_t* ctx = (decompress_ctx_t*)arg;
    assert(ctx->output_fn);

    ctx->bytes_in += in_data_size;

    if (!ctx->is_enabled) {
        // no decompression required
        GOLIOTH_STATUS_RETURN_IF_ERROR(ctx->output_fn(in_data, in_data_size, ctx->output_fn_arg));
        ctx->bytes_out += in_data_size;
        return GOLIOTH_OK;
    }

    size_t total_sunk = 0;
    while (total_sunk < in_data_size) {
        // Sink the compressed data
        size_t sunk = 0;
        HSD_sink_res sink_res = heatshrink_decoder_sink(
                &ctx->hsd, (uint8_t*)&in_data[total_sunk], in_data_size - total_sunk, &sunk);
        if (sink_res != HSDR_SINK_OK) {
            GLTH_LOGE(TAG, "sink error: %d, sunk = %" PRIu32, sink_res, (uint32_t)sunk);
            return GOLIOTH_ERR_FAIL;
        }
        total_sunk += sunk;

        // Pull the uncompressed data out of the decoder
        HSD_poll_res pres;
        uint8_t decode_buffer[HEATSHRINK_STATIC_INPUT_BUFFER_SIZE];
        do {
            size_t poll_sz = 0;
            pres = heatshrink_decoder_poll(
                    &ctx->hsd, decode_buffer, sizeof(decode_buffer), &poll_sz);
            if (pres < 0) {
                GLTH_LOGE(TAG, "poll error: %d", pres);
                return GOLIOTH_ERR_FAIL;
            }

            GOLIOTH_STATUS_RETURN_IF_ERROR(
                    ctx->output_fn(decode_buffer, poll_sz, ctx->output_fn_arg));
            ctx->bytes_out += poll_sz;
        } while (pres == HSDR_POLL_MORE);
    }

    // TODO - compare decompressed size with size from the manifest
    // TODO - compute sha256 of decompressed image and verify it matches manifest

    return GOLIOTH_OK;
}

void fw_block_processor_init(
        fw_block_processor_ctx_t* ctx,
        golioth_client_t client,
        const golioth_ota_component_t* component,
        uint8_t* download_buf) {
    memset(ctx, 0, sizeof(*ctx));

    download_init(&ctx->download, client, component, download_buf);
    decompress_init(&ctx->decompress, component->is_compressed);
    handle_block_init(&ctx->handle_block, component->size);

    // Connect output of download to input of decompress
    ctx->download.output_fn = decompress;
    ctx->download.output_fn_arg = &ctx->decompress;

    // Connect output of decompress to input of handle_block
    ctx->decompress.output_fn = handle_block;
    ctx->decompress.output_fn_arg = &ctx->handle_block;
}

golioth_status_t fw_block_processor_process(fw_block_processor_ctx_t* ctx) {
    // Call the first function in the block processing chain.
    return download_block(&ctx->download);
}

void fw_block_processor_log_results(const fw_block_processor_ctx_t* ctx) {
    GLTH_LOGI(TAG, "Block Latency Stats:");
    GLTH_LOGI(TAG, "   Min: %" PRIu32 " ms", ctx->download.block_stats.block_min_ms);
    GLTH_LOGI(TAG, "   Ave: %.3f ms", ctx->download.block_stats.block_ema_ms);
    GLTH_LOGI(TAG, "   Max: %" PRIu32 " ms", ctx->download.block_stats.block_max_ms);
    GLTH_LOGI(TAG, "Total bytes written: %" PRIu32, (uint32_t)ctx->handle_block.bytes_handled);

    int32_t decompress_delta = ctx->decompress.bytes_out - ctx->decompress.bytes_in;
    if (decompress_delta > 0) {
        GLTH_LOGI(TAG, "Compression saved %" PRId32 " bytes", decompress_delta);
    }
}
