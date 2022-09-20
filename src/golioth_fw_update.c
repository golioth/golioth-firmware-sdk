#include "golioth_fw_update.h"

#if 0
/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "golioth_fw_update.h"
#include "golioth_statistics.h"

#define TAG "golioth_fw_update"

static golioth_client_t _client;
static const char* _current_version;
static SemaphoreHandle_t _manifest_rcvd;
static golioth_ota_manifest_t _ota_manifest;
static uint8_t _ota_block_buffer[GOLIOTH_OTA_BLOCKSIZE + 1];
static const golioth_ota_component_t* _main_component;
static esp_ota_handle_t _update_handle;
static const esp_partition_t* _update_partition;

static bool header_valid(const uint8_t* bytes, size_t nbytes) {
    size_t header_size = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)
            + sizeof(esp_app_desc_t);
    assert(nbytes >= header_size);

    esp_app_desc_t new_app_info;
    memcpy(&new_app_info,
           &bytes[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
           sizeof(esp_app_desc_t));

    esp_app_desc_t running_app_info;
    esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app_info);

    esp_app_desc_t invalid_app_info;
    const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
    esp_ota_get_partition_description(last_invalid_app, &invalid_app_info);

    // check current version with last invalid partition
    if (last_invalid_app) {
        if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version))
            == 0) {
            ESP_LOGW(TAG, "New version is the same as invalid version.");
            ESP_LOGW(
                    TAG,
                    "Previously, there was an attempt to launch the firmware with %s version, but it failed.",
                    invalid_app_info.version);
            ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
            return false;
        }
    }

    return true;
}

static bool fw_update_is_pending_verify(void) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            return true;
        }
    }
    return false;
}

static void fw_update_rollback_and_reboot(void) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

static void fw_update_cancel_rollback(void) {
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "State = Idle");
    golioth_ota_report_state_sync(
            _client,
            GOLIOTH_OTA_STATE_UPDATING,
            GOLIOTH_OTA_REASON_FIRMWARE_UPDATED_SUCCESSFULLY,
            "main",
            _current_version,
            NULL,
            GOLIOTH_WAIT_FOREVER);
}

static bool fw_update_manifest_version_is_different(const golioth_ota_manifest_t* manifest) {
    _main_component = golioth_ota_find_component(manifest, "main");
    if (_main_component) {
        if (0 != strcmp(_current_version, _main_component->version)) {
            ESP_LOGI(
                    TAG,
                    "Current version = %s, Target version = %s",
                    _current_version,
                    _main_component->version);
            return true;
        }
    }
    return false;
}

static golioth_status_t fw_update_download_and_write_flash(void) {
    assert(_main_component);

    esp_err_t err = ESP_OK;

    ESP_LOGI(TAG, "State = Downloading");
    golioth_ota_report_state_sync(
            _client,
            GOLIOTH_OTA_STATE_DOWNLOADING,
            GOLIOTH_OTA_REASON_READY,
            "main",
            _current_version,
            _main_component->version,
            GOLIOTH_WAIT_FOREVER);

    _update_partition = esp_ota_get_next_update_partition(NULL);
    assert(_update_partition);
    ESP_LOGI(
            TAG,
            "Writing to partition subtype %d at offset 0x%x",
            _update_partition->subtype,
            _update_partition->address);

    // Handle blocks one at a time
    ESP_LOGI(TAG, "Image size = %zu", _main_component->size);
    size_t nblocks = golioth_ota_size_to_nblocks(_main_component->size);
    size_t bytes_written = 0;
    for (size_t i = 0; i < nblocks; i++) {
        size_t block_nbytes = 0;

        ESP_LOGI(TAG, "Getting block index %d (%d/%d)", i, i + 1, nblocks);

        golioth_status_t status = golioth_ota_get_block_sync(
                _client,
                _main_component->package,
                _main_component->version,
                i,
                _ota_block_buffer,
                &block_nbytes,
                GOLIOTH_WAIT_FOREVER);
        if (status != GOLIOTH_OK) {
            ESP_LOGE(TAG, "Failed to get block index %d (%s)", i, golioth_status_to_str(status));
            break;
        }

        assert(block_nbytes <= GOLIOTH_OTA_BLOCKSIZE);

        if (i == 0) {
            if (!header_valid(_ota_block_buffer, block_nbytes)) {
                break;
            }
            ESP_LOGI(TAG, "Erasing flash");
            err = esp_ota_begin(_update_partition, _main_component->size, &_update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                esp_ota_abort(_update_handle);
                break;
            }
        }

        err = esp_ota_write(_update_handle, (const void*)_ota_block_buffer, block_nbytes);
        if (err != ESP_OK) {
            esp_ota_abort(_update_handle);
            break;
        }

        bytes_written += block_nbytes;
    }

    ESP_LOGI(TAG, "Total bytes written: %zu", bytes_written);
    if (bytes_written != _main_component->size) {
        ESP_LOGE(
                TAG,
                "Download failed, downloaded size %zu does not match manifest size %zu",
                bytes_written,
                _main_component->size);
        ESP_LOGI(TAG, "State = Idle");
        golioth_ota_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_IDLE,
                GOLIOTH_OTA_REASON_FIRMWARE_UPDATE_FAILED,
                "main",
                _current_version,
                _main_component->version,
                GOLIOTH_WAIT_FOREVER);
        esp_ota_abort(_update_handle);
        return GOLIOTH_ERR_FAIL;
    }
    return GOLIOTH_OK;
}

static golioth_status_t fw_update_validate(void) {
    assert(_update_handle);
    esp_err_t err = esp_ota_end(_update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }

        ESP_LOGI(TAG, "State = Idle");
        golioth_ota_report_state_sync(
                _client,
                GOLIOTH_OTA_STATE_IDLE,
                GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE,
                "main",
                _current_version,
                _main_component->version,
                GOLIOTH_WAIT_FOREVER);
        return GOLIOTH_ERR_FAIL;
    }

    ESP_LOGI(TAG, "State = Downloaded");
    golioth_ota_report_state_sync(
            _client,
            GOLIOTH_OTA_STATE_DOWNLOADED,
            GOLIOTH_OTA_REASON_READY,
            "main",
            _current_version,
            _main_component->version,
            GOLIOTH_WAIT_FOREVER);
    return GOLIOTH_OK;
}

static golioth_status_t fw_update_change_boot_image(void) {
    assert(_update_partition);

    ESP_LOGI(TAG, "State = Updating");
    golioth_ota_report_state_sync(
            _client,
            GOLIOTH_OTA_STATE_UPDATING,
            GOLIOTH_OTA_REASON_READY,
            "main",
            _current_version,
            NULL,
            GOLIOTH_WAIT_FOREVER);

    ESP_LOGI(TAG, "Setting boot partition");
    esp_err_t err = esp_ota_set_boot_partition(_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return GOLIOTH_ERR_FAIL;
    }
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

    ESP_LOGD(TAG, "Received OTA manifest: %.*s", payload_size, payload);

    golioth_ota_state_t state = golioth_ota_get_state();
    if (state == GOLIOTH_OTA_STATE_DOWNLOADING) {
        ESP_LOGW(TAG, "Ignoring manifest while download in progress");
        return;
    }

    golioth_status_t status =
            golioth_ota_payload_as_manifest(payload, payload_size, &_ota_manifest);
    if (status != GOLIOTH_OK) {
        ESP_LOGE(TAG, "Failed to parse manifest: %s", golioth_status_to_str(status));
        return;
    }
    xSemaphoreGive(_manifest_rcvd);
}

static void fw_update_end(void) {
    if (_update_handle) {
        esp_ota_end(_update_handle);
    }
}

static void fw_update_task(void* arg) {
    // If it's the first time booting a new OTA image,
    // wait for successful connection to Golioth.
    //
    // If we don't connect after 30 seconds, roll back to the old image.
    if (fw_update_is_pending_verify()) {
        ESP_LOGI(TAG, "Waiting for golioth client to connect before cancelling rollback");
        int seconds_elapsed = 0;
        while (seconds_elapsed < 30) {
            if (golioth_client_is_connected(_client)) {
                break;
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            seconds_elapsed++;
        }

        if (seconds_elapsed == 30) {
            // We didn't connect to Golioth cloud, so something might be wrong with
            // this firmware. Roll back and reboot.
            ESP_LOGW(TAG, "Failed to connect to Golioth");
            ESP_LOGW(TAG, "!!!");
            ESP_LOGW(TAG, "!!! Rolling back and rebooting now!");
            ESP_LOGW(TAG, "!!!");
            fw_update_rollback_and_reboot();
        } else {
            ESP_LOGI(TAG, "Firmware updated successfully!");
            fw_update_cancel_rollback();
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
        ESP_LOGI(TAG, "Waiting to receive OTA manifest");
        xSemaphoreTake(_manifest_rcvd, portMAX_DELAY);
        ESP_LOGI(TAG, "Received OTA manifest");
        if (!fw_update_manifest_version_is_different(&_ota_manifest)) {
            ESP_LOGI(TAG, "Manifest does not contain different firmware version. Nothing to do.");
            continue;
        }

        if (fw_update_download_and_write_flash() != GOLIOTH_OK) {
            ESP_LOGE(TAG, "Firmware download failed");
            fw_update_end();
            continue;
        }

        if (fw_update_validate() != GOLIOTH_OK) {
            ESP_LOGE(TAG, "Firmware validate failed");
            fw_update_end();
            continue;
        }

        if (fw_update_change_boot_image() != GOLIOTH_OK) {
            ESP_LOGE(TAG, "Firmware change boot image failed");
            fw_update_end();
            continue;
        }

        int countdown = 5;
        while (countdown > 0) {
            ESP_LOGI(TAG, "Rebooting into new image in %d seconds", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            countdown--;
        }
        esp_restart();
    }
}
#endif

void golioth_fw_update_init(golioth_client_t client, const char* current_version) {
#if 0
    static bool initialized = false;

    ESP_LOGI(TAG, "Current firmware version: %s", current_version);

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
            ESP_LOGE(TAG, "Failed to create shell task");
        } else {
            initialized = true;
        }
    }
#endif
}
