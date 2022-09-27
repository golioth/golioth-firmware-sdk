#include "golioth_fw_update.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"

static esp_ota_handle_t _update_handle;
static const esp_partition_t* _update_partition;

static bool fw_update_header_valid(const uint8_t* bytes, size_t nbytes) {
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
            GLTH_LOGW(TAG, "New version is the same as invalid version.");
            GLTH_LOGW(
                    TAG,
                    "Previously, there was an attempt to launch the firmware with %s version, but it failed.",
                    invalid_app_info.version);
            GLTH_LOGW(TAG, "The firmware has been rolled back to the previous version.");
            return false;
        }
    }

    return true;
}

bool fw_update_is_pending_verify(void) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            return true;
        }
    }
    return false;
}

void fw_update_rollback(void) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

void fw_update_reboot(void) {
    esp_restart();
}

void fw_update_cancel_rollback(void) {
    esp_ota_mark_app_valid_cancel_rollback();
}

golioth_status_t fw_update_handle_block(
        const uint8_t* block,
        size_t block_size,
        size_t offset,
        size_t total_size) {
    esp_err_t err = ESP_OK;

    if (offset == 0) {
        if (!fw_update_header_valid(block, block_size)) {
            return GOLIOTH_ERR_FAIL;
        }

        _update_partition = esp_ota_get_next_update_partition(NULL);
        assert(_update_partition);
        GLTH_LOGI(
                TAG,
                "Writing to partition subtype %d at offset 0x%x",
                _update_partition->subtype,
                _update_partition->address);

        GLTH_LOGI(TAG, "Erasing flash");
        err = esp_ota_begin(_update_partition, total_size, &_update_handle);
        if (err != ESP_OK) {
            GLTH_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            esp_ota_abort(_update_handle);
            return GOLIOTH_ERR_FAIL;
        }
    }

    err = esp_ota_write(_update_handle, (const void*)block, block_size);
    if (err != ESP_OK) {
        GLTH_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
        esp_ota_abort(_update_handle);
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

void fw_update_post_download(void) {
    // Nothing to do
}

golioth_status_t fw_update_validate(void) {
    assert(_update_handle);
    esp_err_t err = esp_ota_end(_update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            GLTH_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            GLTH_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }

        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

golioth_status_t fw_update_change_boot_image(void) {
    assert(_update_partition);

    GLTH_LOGI(TAG, "Setting boot partition");
    esp_err_t err = esp_ota_set_boot_partition(_update_partition);
    if (err != ESP_OK) {
        GLTH_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return GOLIOTH_ERR_FAIL;
    }
    return GOLIOTH_OK;
}

void fw_update_end(void) {
    if (_update_handle) {
        esp_ota_end(_update_handle);
    }
}
