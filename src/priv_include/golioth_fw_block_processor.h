/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "golioth_client.h"
#include "golioth_ota.h"
#include "golioth_status.h"
#include <heatshrink_decoder.h>
#include <stdint.h>

typedef golioth_status_t (*process_fn)(const uint8_t* in, size_t in_size, void* arg);

typedef struct {
    uint32_t block_min_ms;
    float block_ema_ms;
    uint32_t block_max_ms;
} block_latency_stats_t;

typedef struct {
    /// Golioth client used for downloading
    golioth_client_t client;
    /// Estimate of the total number blocks to download
    size_t total_num_blocks;
    /// Index of current block to download
    size_t block_index;
    /// Number of bytes downloaded for the current block
    size_t block_bytes_downloaded;
    /// True, if the last block has been downloaded
    bool is_last_block;
    /// Statistics to measure block download latency
    block_latency_stats_t block_stats;
    /// OTA component to download
    const golioth_ota_component_t* ota_component;
    /// Buffer where downloaded blocks will be copied to
    uint8_t* download_buf;
    /// Function to call when output is available
    process_fn output_fn;
    void* output_fn_arg;
} download_ctx_t;

typedef struct {
    /// Dynamically allocated heatshrink decoder
    heatshrink_decoder* hsd;
    /// Number of bytes input to the decompressor
    int32_t bytes_in;
    /// Number of bytes output from the decompressor. If decompression
    /// is enabled, this number will be higher than bytes_in.
    int32_t bytes_out;
    /// Function to call when output is available
    process_fn output_fn;
    void* output_fn_arg;
} decompress_ctx_t;

typedef struct {
    /// Number of bytes forwarded to fw_update_handle_block()
    int32_t bytes_handled;
    /// Size of the OTA component being handled
    size_t component_size;
} handle_block_ctx_t;

typedef struct {
    download_ctx_t download;
    decompress_ctx_t decompress;
    handle_block_ctx_t handle_block;
} fw_block_processor_ctx_t;

void fw_block_processor_init(
        fw_block_processor_ctx_t* ctx,
        golioth_client_t client,
        const golioth_ota_component_t* component,
        uint8_t* download_buf);
golioth_status_t fw_block_processor_process(fw_block_processor_ctx_t* ctx);
void fw_block_processor_log_results(const fw_block_processor_ctx_t* ctx);
void fw_block_processor_deinit(fw_block_processor_ctx_t* ctx);
