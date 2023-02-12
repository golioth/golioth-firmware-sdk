/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// TODO - fw_update_destroy, to clean up resources and stop thread

#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <heatshrink_decoder.h>
#include "golioth_sys.h"
#include "golioth_fw_update.h"
#include "golioth_statistics.h"
#include "golioth_util.h"

#define TAG "golioth_fw_update"

#define HEATSHRINK_WINDOW_SZ2 8
#define HEATSHRINK_LOOKAHEAD_SZ2 4
#define HEATSHRINK_DECODE_BUFFER_SIZE 512

typedef struct {
    uint32_t block_min_ms;
    float block_ema_ms;
    uint32_t block_max_ms;
} block_latency_stats_t;

typedef golioth_status_t (*process_fn)(const uint8_t* in, size_t in_size, void* arg);

typedef struct {
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
} decompress_ctx_t;

static golioth_client_t _client;
static golioth_sys_sem_t _manifest_rcvd;
static golioth_ota_manifest_t _ota_manifest;
static uint8_t _ota_block_buffer[GOLIOTH_OTA_BLOCKSIZE + 1];
static const golioth_ota_component_t* _main_component;
static golioth_fw_update_state_change_callback _state_callback;
static void* _state_callback_arg;
static golioth_fw_update_config_t _config;

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

static void download_init(download_ctx_t* ctx, const golioth_ota_component_t* ota_component) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->ota_component = ota_component;
    block_stats_init(&ctx->block_stats);
}

static golioth_status_t download_block(download_ctx_t* ctx, uint8_t* output_buf) {
    const uint64_t download_start_ms = golioth_sys_now_ms();

    GOLIOTH_STATUS_RETURN_IF_ERROR(golioth_ota_get_block_sync(
            _client,
            ctx->ota_component->package,
            ctx->ota_component->version,
            ctx->block_index,
            output_buf,
            &ctx->block_bytes_downloaded,
            &ctx->is_last_block,
            GOLIOTH_WAIT_FOREVER));

    assert(ctx->block_bytes_downloaded <= GOLIOTH_OTA_BLOCKSIZE);
    block_stats_update(&ctx->block_stats, golioth_sys_now_ms() - download_start_ms);

    assert(ctx->output_fn);
    return ctx->output_fn(output_buf, ctx->block_bytes_downloaded, ctx->output_fn_arg);
}

static void decompress_init(decompress_ctx_t* ctx, bool enable_decompression) {
    memset(ctx, 0, sizeof(*ctx));
    if (enable_decompression) {
        GLTH_LOGI(TAG, "Compressed image detected");
        ctx->hsd = heatshrink_decoder_alloc(
                GOLIOTH_OTA_BLOCKSIZE,  // input_buffer_size
                HEATSHRINK_WINDOW_SZ2,
                HEATSHRINK_LOOKAHEAD_SZ2);
    }
}

static golioth_status_t decompress_and_handle(
        const uint8_t* in_data,
        size_t in_data_size,
        void* arg) {
    decompress_ctx_t* ctx = (decompress_ctx_t*)arg;
    ctx->bytes_in += in_data_size;

    if (!ctx->hsd) {
        // no decompression required
        GOLIOTH_STATUS_RETURN_IF_ERROR(fw_update_handle_block(
                in_data,
                in_data_size,
                ctx->bytes_out,  // offset
                _main_component->size));

        ctx->bytes_out += in_data_size;
        return GOLIOTH_OK;
    }

    // Sink the compressed data
    size_t sunk = 0;
    HSD_sink_res sink_res =
            heatshrink_decoder_sink(ctx->hsd, (uint8_t*)in_data, in_data_size, &sunk);
    if (sink_res != HSDR_SINK_OK) {
        GLTH_LOGE(TAG, "sink error: %d, sunk = %" PRIu32, sink_res, (uint32_t)sunk);
        return GOLIOTH_ERR_FAIL;
    }

    // Pull the uncompressed data out of the decoder
    HSD_poll_res pres;
    uint8_t decode_buffer[HEATSHRINK_DECODE_BUFFER_SIZE];
    do {
        size_t poll_sz = 0;
        pres = heatshrink_decoder_poll(ctx->hsd, decode_buffer, sizeof(decode_buffer), &poll_sz);
        if (pres < 0) {
            GLTH_LOGE(TAG, "poll error: %d", pres);
            return GOLIOTH_ERR_FAIL;
        }

        GOLIOTH_STATUS_RETURN_IF_ERROR(fw_update_handle_block(
                decode_buffer,
                poll_sz,
                ctx->bytes_out,  // offset
                _main_component->size));

        ctx->bytes_out += poll_sz;
    } while (pres == HSDR_POLL_MORE);

    // TODO - compare decompressed size with size from the manifest
    // TODO - compute sha256 of decompressed image and verify it matches manifest

    return GOLIOTH_OK;
}

static void decompress_deinit(decompress_ctx_t* ctx) {
    if (ctx->hsd) {
        heatshrink_decoder_free(ctx->hsd);
    }
}

static golioth_status_t download_and_write_flash(void) {
    assert(_main_component);

    GLTH_LOGI(TAG, "Image size = %" PRIu32, _main_component->size);

    download_ctx_t download_ctx;
    download_init(&download_ctx, _main_component);

    decompress_ctx_t decompress_ctx;
    decompress_init(&decompress_ctx, _main_component->is_compressed);

    download_ctx.output_fn = decompress_and_handle;
    download_ctx.output_fn_arg = &decompress_ctx;

    // Note: total_num_blocks is an estimate of the number of blocks required to download,
    // based on the size of the main component reported in the manifest.
    //
    // Due to the way this size is populated in the manifest on the server, the actual
    // number of blocks may be different.
    //
    // Therefore, the loop below will terminate when the last block is received, not
    // when total_num_blocks is reached.
    size_t total_num_blocks = golioth_ota_size_to_nblocks(_main_component->size);

    // Handle blocks one at a time
    const uint64_t start_time_ms = golioth_sys_now_ms();
    for (download_ctx.block_index = 0; !download_ctx.is_last_block; download_ctx.block_index++) {
        GLTH_LOGI(
                TAG,
                "Getting block index %" PRIu32 " (%" PRIu32 "/%" PRIu32 ")",
                (uint32_t)download_ctx.block_index,
                (uint32_t)download_ctx.block_index + 1,
                (uint32_t)total_num_blocks);

        golioth_status_t status = download_block(&download_ctx, _ota_block_buffer);
        if (status != GOLIOTH_OK) {
            break;
        }
    }

    GLTH_LOGI(TAG, "Download took %" PRIu64 " ms", golioth_sys_now_ms() - start_time_ms);
    GLTH_LOGI(TAG, "Block Latency Stats:");
    GLTH_LOGI(TAG, "   Min: %" PRIu32 " ms", download_ctx.block_stats.block_min_ms);
    GLTH_LOGI(TAG, "   Ave: %.3f ms", download_ctx.block_stats.block_ema_ms);
    GLTH_LOGI(TAG, "   Max: %" PRIu32 " ms", download_ctx.block_stats.block_max_ms);
    GLTH_LOGI(TAG, "Total bytes written: %" PRIu32, (uint32_t)decompress_ctx.bytes_out);

    int32_t decompress_delta = decompress_ctx.bytes_out - decompress_ctx.bytes_in;
    if (decompress_delta > 0) {
        GLTH_LOGI(TAG, "Compression saved %" PRId32 " bytes", decompress_delta);
    }

    decompress_deinit(&decompress_ctx);
    fw_update_post_download();

    return GOLIOTH_OK;
}

static golioth_status_t golioth_fw_update_report_state_sync(
        golioth_client_t client,
        golioth_ota_state_t state,
        golioth_ota_reason_t reason,
        const char* package,
        const char* current_version,
        const char* target_version,
        int32_t timeout_s) {
    if (_state_callback) {
        _state_callback(state, reason, _state_callback_arg);
    }

    return golioth_ota_report_state_sync(
            client, state, reason, package, current_version, target_version, timeout_s);
}

static void on_ota_manifest(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        return;
    }

    GLTH_LOGD(TAG, "Received OTA manifest: %.*s", (int)payload_size, payload);

    golioth_ota_state_t state = golioth_ota_get_state();
    if (state == GOLIOTH_OTA_STATE_DOWNLOADING) {
        GLTH_LOGW(TAG, "Ignoring manifest while download in progress");
        return;
    }

    golioth_status_t status =
            golioth_ota_payload_as_manifest(payload, payload_size, &_ota_manifest);
    if (status != GOLIOTH_OK) {
        GLTH_LOGE(TAG, "Failed to parse manifest: %s", golioth_status_to_str(status));
        return;
    }
    golioth_sys_sem_give(_manifest_rcvd);
}

static bool manifest_version_is_different(const golioth_ota_manifest_t* manifest) {
    _main_component = golioth_ota_find_component(manifest, _config.fw_package_name);
    if (_main_component) {
        if (0 != strcmp(_config.current_version, _main_component->version)) {
            GLTH_LOGI(
                    TAG,
                    "Current version = %s, Target version = %s",
                    _config.current_version,
                    _main_component->version);
            return true;
        }
    }
    return false;
}

static void fw_update_thread(void* arg) {
    // If it's the first time booting a new OTA image,
    // wait for successful connection to Golioth.
    //
    // If we don't connect after 60 seconds, roll back to the old image.
    if (fw_update_is_pending_verify()) {
        GLTH_LOGI(TAG, "Waiting for golioth client to connect before cancelling rollback");
        int seconds_elapsed = 0;
        while (seconds_elapsed < 60) {
            if (golioth_client_is_connected(_client)) {
                break;
            }
            golioth_sys_msleep(1000);
            seconds_elapsed++;
        }

        if (seconds_elapsed == 60) {
            // We didn't connect to Golioth cloud, so something might be wrong with
            // this firmware. Roll back and reboot.
            GLTH_LOGW(TAG, "Failed to connect to Golioth");
            GLTH_LOGW(TAG, "!!!");
            GLTH_LOGW(TAG, "!!! Rolling back and rebooting now!");
            GLTH_LOGW(TAG, "!!!");
            fw_update_rollback();
            fw_update_reboot();
        } else {
            GLTH_LOGI(TAG, "Firmware updated successfully!");
            fw_update_cancel_rollback();

            GLTH_LOGI(TAG, "State = Idle");
            golioth_fw_update_report_state_sync(
                    _client,
                    GOLIOTH_OTA_STATE_UPDATING,
                    GOLIOTH_OTA_REASON_FIRMWARE_UPDATED_SUCCESSFULLY,
                    _config.fw_package_name,
                    _config.current_version,
                    NULL,
                    GOLIOTH_WAIT_FOREVER);
        }
    }

    golioth_fw_update_report_state_sync(
            _client,
            GOLIOTH_OTA_STATE_IDLE,
            GOLIOTH_OTA_REASON_READY,
            _config.fw_package_name,
            _config.current_version,
            NULL,
            GOLIOTH_WAIT_FOREVER);

    golioth_ota_observe_manifest_async(_client, on_ota_manifest, NULL);

    while (1) {
        GLTH_LOGI(TAG, "Waiting to receive OTA manifest");
        golioth_sys_sem_take(_manifest_rcvd, GOLIOTH_SYS_WAIT_FOREVER);
        GLTH_LOGI(TAG, "Received OTA manifest");
        if (!manifest_version_is_different(&_ota_manifest)) {
            GLTH_LOGI(TAG, "Manifest does not contain different firmware version. Nothing to do.");
            continue;
        }

        GLTH_LOGI(TAG, "State = Downloading");
        golioth_fw_update_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_DOWNLOADING,
                GOLIOTH_OTA_REASON_READY,
                _config.fw_package_name,
                _config.current_version,
                _main_component->version,
                GOLIOTH_WAIT_FOREVER);

        if (download_and_write_flash() != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Firmware download failed");
            fw_update_end();

            GLTH_LOGI(TAG, "State = Idle");
            golioth_fw_update_report_state_sync(
                    _client,
                    GOLIOTH_OTA_STATE_IDLE,
                    GOLIOTH_OTA_REASON_FIRMWARE_UPDATE_FAILED,
                    _config.fw_package_name,
                    _config.current_version,
                    _main_component->version,
                    GOLIOTH_WAIT_FOREVER);

            continue;
        }

        if (fw_update_validate() != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Firmware validate failed");
            fw_update_end();

            GLTH_LOGI(TAG, "State = Idle");
            golioth_fw_update_report_state_sync(
                    _client,
                    GOLIOTH_OTA_STATE_IDLE,
                    GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE,
                    _config.fw_package_name,
                    _config.current_version,
                    _main_component->version,
                    GOLIOTH_WAIT_FOREVER);

            continue;
        }

        GLTH_LOGI(TAG, "State = Downloaded");
        golioth_fw_update_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_DOWNLOADED,
                GOLIOTH_OTA_REASON_READY,
                _config.fw_package_name,
                _config.current_version,
                _main_component->version,
                GOLIOTH_WAIT_FOREVER);

        GLTH_LOGI(TAG, "State = Updating");
        golioth_fw_update_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_UPDATING,
                GOLIOTH_OTA_REASON_READY,
                _config.fw_package_name,
                _config.current_version,
                NULL,
                GOLIOTH_WAIT_FOREVER);

        if (fw_update_change_boot_image() != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Firmware change boot image failed");
            fw_update_end();
            continue;
        }

        int countdown = 5;
        while (countdown > 0) {
            GLTH_LOGI(TAG, "Rebooting into new image in %d seconds", countdown);
            golioth_sys_msleep(1000);
            countdown--;
        }
        fw_update_reboot();
    }
}

void golioth_fw_update_init(golioth_client_t client, const char* current_version) {
    golioth_fw_update_config_t config = {
            .current_version = current_version,
            .fw_package_name = GOLIOTH_FW_UPDATE_DEFAULT_PACKAGE_NAME,
    };
    golioth_fw_update_init_with_config(client, &config);
}

void golioth_fw_update_init_with_config(
        golioth_client_t client,
        const golioth_fw_update_config_t* config) {
    static bool initialized = false;

    _client = client;
    _config = *config;
    _manifest_rcvd = golioth_sys_sem_create(1, 0);  // never destroyed

    GLTH_LOGI(
            TAG,
            "Current firmware version: %s - %s",
            _config.fw_package_name,
            _config.current_version);

    if (!initialized) {
        golioth_sys_thread_t thread = golioth_sys_thread_create((golioth_sys_thread_config_t){
                .name = "fw_update",
                .fn = fw_update_thread,
                .user_arg = NULL,
                .stack_size = 4096,
                .prio = 3});
        if (!thread) {
            GLTH_LOGE(TAG, "Failed to create shell thread");
        } else {
            initialized = true;
        }
    }
}

void golioth_fw_update_register_state_change_callback(
        golioth_fw_update_state_change_callback callback,
        void* user_arg) {
    _state_callback = callback;
    _state_callback_arg = user_arg;
}
