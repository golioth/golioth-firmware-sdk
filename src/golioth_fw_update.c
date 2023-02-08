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

static golioth_client_t _client;
static golioth_sys_sem_t _manifest_rcvd;
static golioth_ota_manifest_t _ota_manifest;
static uint8_t _ota_block_buffer[GOLIOTH_OTA_BLOCKSIZE + 1];
static const golioth_ota_component_t* _main_component;
static golioth_fw_update_state_change_callback _state_callback;
static void* _state_callback_arg;
static golioth_fw_update_config_t _config;
#if CONFIG_GOLIOTH_OTA_DECOMPRESSION_ENABLE
static heatshrink_decoder _hsd;
#endif

static golioth_status_t download_and_write_flash(void) {
    assert(_main_component);

    int32_t main_size = _main_component->size;
    GLTH_LOGI(TAG, "Image size = %" PRIu32, main_size);

#if CONFIG_GOLIOTH_OTA_DECOMPRESSION_ENABLE
    GLTH_LOGI(TAG, "Compression enabled");
#endif

    block_latency_stats_t stats = {
            .block_min_ms = UINT32_MAX,
            .block_ema_ms = 0.0f,
            .block_max_ms = 0,
    };
    uint64_t start_time_ms = golioth_sys_now_ms();

    // Handle blocks one at a time
    size_t nblocks = golioth_ota_size_to_nblocks(main_size);
    size_t bytes_written = 0;
    for (size_t i = 0; /* empty */; i++) {
        size_t block_nbytes = 0;

        GLTH_LOGI(
                TAG,
                "Getting block index %" PRIu32 " (%" PRIu32 "/%" PRIu32 ")",
                (uint32_t)i,
                (uint32_t)i + 1,
                (uint32_t)nblocks);

        bool is_last_block = false;

        uint64_t block_start_ms = golioth_sys_now_ms();
        golioth_status_t status = golioth_ota_get_block_sync(
                _client,
                _main_component->package,
                _main_component->version,
                i,
                _ota_block_buffer,
                &block_nbytes,
                &is_last_block,
                GOLIOTH_WAIT_FOREVER);
        uint64_t block_latency_ms = golioth_sys_now_ms() - block_start_ms;

        // Update block latency statistics
        const float alpha = 0.01f;
        stats.block_min_ms = min(stats.block_min_ms, block_latency_ms);
        stats.block_ema_ms = alpha * (float)block_latency_ms + (1.0f - alpha) * stats.block_ema_ms;
        stats.block_max_ms = max(stats.block_max_ms, block_latency_ms);

        if (status != GOLIOTH_OK) {
            GLTH_LOGE(
                    TAG,
                    "Failed to get block index %" PRIu32 " (%s)",
                    (uint32_t)i,
                    golioth_status_to_str(status));
            break;
        }

        assert(block_nbytes <= GOLIOTH_OTA_BLOCKSIZE);

#if CONFIG_GOLIOTH_OTA_DECOMPRESSION_ENABLE
        // Sink the compressed data
        size_t sunk = 0;
        HSD_sink_res sink_res =
                heatshrink_decoder_sink(&_hsd, _ota_block_buffer, block_nbytes, &sunk);
        if (sink_res != HSDR_SINK_OK) {
            GLTH_LOGE(TAG, "sink error: %d, sunk = %" PRIu32, sink_res, (uint32_t)sunk);
        }

        // Pull the uncompressed data out of the decoder
        HSD_poll_res pres;
        uint8_t decode_buffer[HEATSHRINK_DECODE_BUFFER_SIZE];
        do {
            size_t poll_sz = 0;
            pres = heatshrink_decoder_poll(&_hsd, decode_buffer, sizeof(decode_buffer), &poll_sz);
            if (pres < 0) {
                GLTH_LOGE(TAG, "poll error: %d", pres);
            }

            status = fw_update_handle_block(decode_buffer, poll_sz, bytes_written, main_size);
            if (status != GOLIOTH_OK) {
                GLTH_LOGE(TAG, "Failed to handle block index %" PRIu32, (uint32_t)i);
                return status;
            }

            bytes_written += poll_sz;
        } while (pres == HSDR_POLL_MORE);

        if (is_last_block) {
            HSD_finish_res hsd_finish_res = heatshrink_decoder_finish(&_hsd);
            if (hsd_finish_res != HSDR_FINISH_DONE) {
                GLTH_LOGE(TAG, "finish error: %d", hsd_finish_res);
            }
        }
#else
        status = fw_update_handle_block(_ota_block_buffer, block_nbytes, bytes_written, main_size);
        if (status != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Failed to handle block index %" PRIu32, (uint32_t)i);
            return status;
        }

        bytes_written += block_nbytes;
#endif

        if (is_last_block) {
            break;
        }
    }

    uint64_t elapsed_ms = golioth_sys_now_ms() - start_time_ms;
    GLTH_LOGI(TAG, "Download took %" PRIu64 " ms", elapsed_ms);
    GLTH_LOGI(TAG, "Block Latency Stats:");
    GLTH_LOGI(TAG, "   Min: %" PRIu32 " ms", stats.block_min_ms);
    GLTH_LOGI(TAG, "   Ave: %.3f ms", stats.block_ema_ms);
    GLTH_LOGI(TAG, "   Max: %" PRIu32 " ms", stats.block_max_ms);
    GLTH_LOGI(TAG, "Total bytes written: %" PRIu32, (uint32_t)bytes_written);

#if CONFIG_GOLIOTH_OTA_DECOMPRESSION_ENABLE
    // TODO - compare decompressed size with size from the manifest
    // TODO - compute sha256 of decompressed image and verify it matches manifest
    GLTH_LOGI(
            TAG,
            "Compression saved %" PRId32 " bytes",
            (int32_t)bytes_written - (int32_t)main_size);
#endif

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
