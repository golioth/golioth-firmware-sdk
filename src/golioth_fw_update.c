/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "golioth_fw_update.h"
#include "golioth_statistics.h"

#define TAG "golioth_fw_update"

static golioth_client_t _client;
static const char* _current_version;
static SemaphoreHandle_t _manifest_rcvd;
static golioth_ota_manifest_t _ota_manifest;
static uint8_t _ota_block_buffer[GOLIOTH_OTA_BLOCKSIZE + 1];
static const golioth_ota_component_t* _main_component;

static golioth_status_t download_and_write_flash(void) {
    assert(_main_component);

    int32_t main_size = _main_component->size;

    // Handle blocks one at a time
    GLTH_LOGI(TAG, "Image size = %"PRIu32, main_size);
    size_t nblocks = golioth_ota_size_to_nblocks(main_size);
    size_t bytes_written = 0;
    for (size_t i = 0; /* empty */; i++) {
        size_t block_nbytes = 0;

        GLTH_LOGI(TAG, "Getting block index %d (%d/%d)", i, i + 1, nblocks);

        bool is_last_block = false;
        golioth_status_t status = golioth_ota_get_block_sync(
                _client,
                _main_component->package,
                _main_component->version,
                i,
                _ota_block_buffer,
                &block_nbytes,
                &is_last_block,
                GOLIOTH_WAIT_FOREVER);
        if (status != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Failed to get block index %d (%s)", i, golioth_status_to_str(status));
            break;
        }

        assert(block_nbytes <= GOLIOTH_OTA_BLOCKSIZE);

        status = fw_update_handle_block(
                _ota_block_buffer,
                block_nbytes,
                bytes_written,
                main_size);
        if (status != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Failed to handle block index %d", i);
            return status;
        }

        bytes_written += block_nbytes;

        if (is_last_block) {
            break;
        }
    }

    GLTH_LOGI(TAG, "Total bytes written: %"PRIu32, (uint32_t)bytes_written);
    fw_update_post_download();

    return GOLIOTH_OK;
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

    GLTH_LOGD(TAG, "Received OTA manifest: %.*s", payload_size, payload);

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
    xSemaphoreGive(_manifest_rcvd);
}

static bool manifest_version_is_different(const golioth_ota_manifest_t* manifest) {
    _main_component = golioth_ota_find_component(manifest, "main");
    if (_main_component) {
        if (0 != strcmp(_current_version, _main_component->version)) {
            GLTH_LOGI(
                    TAG,
                    "Current version = %s, Target version = %s",
                    _current_version,
                    _main_component->version);
            return true;
        }
    }
    return false;
}

static void fw_update_task(void* arg) {
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
            vTaskDelay(1000 / portTICK_PERIOD_MS);
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
            golioth_ota_report_state_sync(
                    _client,
                    GOLIOTH_OTA_STATE_UPDATING,
                    GOLIOTH_OTA_REASON_FIRMWARE_UPDATED_SUCCESSFULLY,
                    "main",
                    _current_version,
                    NULL,
                    GOLIOTH_WAIT_FOREVER);
        }
    }

    golioth_ota_report_state_sync(
            _client,
            GOLIOTH_OTA_STATE_IDLE,
            GOLIOTH_OTA_REASON_READY,
            "main",
            _current_version,
            NULL,
            GOLIOTH_WAIT_FOREVER);

    golioth_ota_observe_manifest_async(_client, on_ota_manifest, NULL);

    while (1) {
        GLTH_LOGI(TAG, "Waiting to receive OTA manifest");
        xSemaphoreTake(_manifest_rcvd, portMAX_DELAY);
        GLTH_LOGI(TAG, "Received OTA manifest");
        if (!manifest_version_is_different(&_ota_manifest)) {
            GLTH_LOGI(TAG, "Manifest does not contain different firmware version. Nothing to do.");
            continue;
        }

        GLTH_LOGI(TAG, "State = Downloading");
        golioth_ota_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_DOWNLOADING,
                GOLIOTH_OTA_REASON_READY,
                "main",
                _current_version,
                _main_component->version,
                GOLIOTH_WAIT_FOREVER);

        if (download_and_write_flash() != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Firmware download failed");
            fw_update_end();

            GLTH_LOGI(TAG, "State = Idle");
            golioth_ota_report_state_sync(
                    _client,
                    GOLIOTH_OTA_STATE_IDLE,
                    GOLIOTH_OTA_REASON_FIRMWARE_UPDATE_FAILED,
                    "main",
                    _current_version,
                    _main_component->version,
                    GOLIOTH_WAIT_FOREVER);

            continue;
        }

        if (fw_update_validate() != GOLIOTH_OK) {
            GLTH_LOGE(TAG, "Firmware validate failed");
            fw_update_end();

            GLTH_LOGI(TAG, "State = Idle");
            golioth_ota_report_state_sync(
                    _client,
                    GOLIOTH_OTA_STATE_IDLE,
                    GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE,
                    "main",
                    _current_version,
                    _main_component->version,
                    GOLIOTH_WAIT_FOREVER);

            continue;
        }

        GLTH_LOGI(TAG, "State = Downloaded");
        golioth_ota_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_DOWNLOADED,
                GOLIOTH_OTA_REASON_READY,
                "main",
                _current_version,
                _main_component->version,
                GOLIOTH_WAIT_FOREVER);

        GLTH_LOGI(TAG, "State = Updating");
        golioth_ota_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_UPDATING,
                GOLIOTH_OTA_REASON_READY,
                "main",
                _current_version,
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
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            countdown--;
        }
        fw_update_reboot();
    }
}

void golioth_fw_update_init(golioth_client_t client, const char* current_version) {
    static bool initialized = false;

    GLTH_LOGI(TAG, "Current firmware version: %s", current_version);

    _client = client;
    _current_version = current_version;
    _manifest_rcvd = xSemaphoreCreateBinary();  // never freed

    if (!initialized) {
        bool task_created = xTaskCreate(  // never freed
                fw_update_task,
                "fw_update",
                4096,
                NULL,  // task arg
                3,     // pri
                NULL);
        if (!task_created) {
            GLTH_LOGE(TAG, "Failed to create shell task");
        } else {
            initialized = true;
        }
    }
}
