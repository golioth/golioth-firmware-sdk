/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/fw_update.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"
#include "esp_app_format.h"
#include "esp_system.h"
#include "bootloader_common.h"
#include "golioth/golioth_status.h"
#include "golioth/ota.h"
#include <string.h>  // memcpy

#define TAG "fw_update_esp_idf"

static esp_ota_handle_t _update_handle;
static const esp_partition_t *_update_partition;

bool fw_update_is_pending_verify(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            return true;
        }
    }
    return false;
}

void fw_update_rollback(void)
{
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

void fw_update_reboot(void)
{
    esp_restart();
}

void fw_update_cancel_rollback(void)
{
    esp_ota_mark_app_valid_cancel_rollback();
}

enum golioth_status fw_update_handle_block(const uint8_t *block,
                                           size_t block_size,
                                           size_t offset,
                                           size_t total_size)
{
    esp_err_t err = ESP_OK;

    if (offset == 0)
    {
        _update_partition = esp_ota_get_next_update_partition(NULL);
        assert(_update_partition);
        GLTH_LOGI(TAG,
                  "Writing to partition subtype %d at offset 0x%" PRIx32,
                  _update_partition->subtype,
                  _update_partition->address);

        GLTH_LOGI(TAG, "Erasing flash");
        err = esp_ota_begin(_update_partition, OTA_SIZE_UNKNOWN, &_update_handle);
        if (err != ESP_OK)
        {
            GLTH_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            esp_ota_abort(_update_handle);
            return GOLIOTH_ERR_IO;
        }
    }

    err = esp_ota_write(_update_handle, (const void *) block, block_size);
    if (err != ESP_OK)
    {
        GLTH_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
        esp_ota_abort(_update_handle);
        return GOLIOTH_ERR_IO;
    }

    return GOLIOTH_OK;
}

enum golioth_status fw_update_post_download(void)
{
    assert(_update_handle);
    esp_err_t err = esp_ota_end(_update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            GLTH_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        else
        {
            GLTH_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }

        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

enum golioth_status fw_update_check_candidate(const uint8_t *hash, size_t img_size)
{
    _update_partition = esp_ota_get_next_update_partition(NULL);
    assert(_update_partition);

    if (img_size > _update_partition->size)
    {
        return GOLIOTH_ERR_FAIL;
    }

    uint8_t calc_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN];

    // Manually set size and type of partition. When partition is PART_TYPE_APP, the appended digest
    // in the image will be used, rather than the hash of the full contents. Overriding to
    // PART_TYPE_DATA requires that we use the image size rather than the partition size to ensure
    // that only the hash of the image contents is calculated.
    if (bootloader_common_get_sha256_of_partition(_update_partition->address,
                                                  img_size,
                                                  PART_TYPE_DATA,
                                                  calc_sha256)
        != ESP_OK)
    {
        return GOLIOTH_ERR_FAIL;
    }

    if (memcmp(hash, calc_sha256, sizeof(calc_sha256)) != 0)
    {
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

enum golioth_status fw_update_change_boot_image(void)
{
    assert(_update_partition);

    GLTH_LOGI(TAG, "Setting boot partition");
    esp_err_t err = esp_ota_set_boot_partition(_update_partition);
    if (err != ESP_OK)
    {
        GLTH_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return GOLIOTH_ERR_FAIL;
    }
    return GOLIOTH_OK;
}

void fw_update_end(void)
{
    if (_update_handle)
    {
        esp_ota_end(_update_handle);
    }
}
