/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/client.h>
#include <golioth/ota.h>
#include <golioth/golioth_status.h>
#include <bspatch.h>
#include <stdint.h>

typedef enum golioth_status (*process_fn)(const uint8_t *in, size_t in_size, void *arg);

typedef struct
{
    uint32_t block_min_ms;
    uint32_t block_max_ms;
} block_latency_stats_t;

typedef struct
{
    /// Golioth client used for downloading
    struct golioth_client *client;
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
    const struct golioth_ota_component *ota_component;
    /// Buffer where downloaded blocks will be copied to
    uint8_t *download_buf;
    /// Function to call when output is available
    process_fn output_fn;
    void *output_fn_arg;
} download_ctx_t;

typedef struct
{
    /// Context struct for calls into bspatch()
    struct bspatch_ctx bspatch_ctx;
    /// Input stream, required by bspatch()
    struct bspatch_stream_i old_stream;
    /// Output stream, required by bspatch()
    struct bspatch_stream_n new_stream;
    /// Function to call when output is available
    process_fn output_fn;
    void *output_fn_arg;
} patch_ctx_t;

typedef struct
{
    /// Number of bytes forwarded to fw_update_handle_block()
    int32_t bytes_handled;
    /// Size of the OTA component being handled
    size_t component_size;
} handle_block_ctx_t;

typedef struct
{
    download_ctx_t download;
    patch_ctx_t patch;
    handle_block_ctx_t handle_block;
} fw_block_processor_ctx_t;

void fw_block_processor_init(fw_block_processor_ctx_t *ctx,
                             struct golioth_client *client,
                             const struct golioth_ota_component *component,
                             uint8_t *download_buf);
enum golioth_status fw_block_processor_process(fw_block_processor_ctx_t *ctx);
void fw_block_processor_log_results(const fw_block_processor_ctx_t *ctx);
