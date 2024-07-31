/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/fw_update.h>
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "sysflash/sysflash.h"

#define TAG "fw_update_mcuboot"

static const struct flash_area* _primary_flash_area;
static const struct flash_area* _secondary_flash_area;

bool fw_update_is_pending_verify(void) {
    struct boot_swap_state primary_swap_state;
    int primary_id = flash_area_id_from_image_slot(0);
    int status = boot_read_swap_state_by_id(primary_id, &primary_swap_state);
    if (status != 0) {
        GLTH_LOGE(TAG, "Failed to read swap state: %d", status);
        return false;
    }

    GLTH_LOGD(TAG, "Primary swap state = %d", primary_swap_state.swap_type);
    return (primary_swap_state.swap_type == BOOT_SWAP_TYPE_TEST);
}

void fw_update_rollback(void) {
    // Nothing special to do, just need to reboot and the rollback will happen
}

void fw_update_reboot(void) {
    NVIC_SystemReset();
}

void fw_update_cancel_rollback(void) {
    // Mark primary image as "ok"
    boot_set_confirmed();
}

enum golioth_status fw_update_handle_block(
        const uint8_t* block,
        size_t block_size,
        size_t offset,
        size_t total_size) {
    int status = 0;

    if (offset == 0) {
        // Validate it's an MCUboot header
        const struct image_header* header = (const struct image_header*)block;
        if (header->ih_magic != IMAGE_MAGIC) {
            GLTH_LOGE(TAG, "Image header invalid, IMAGE_MAGIC not found");
            return GOLIOTH_ERR_FAIL;
        }

        // Open secondary flash area
        int secondary_id = flash_area_id_from_image_slot(1);
        status = flash_area_open(secondary_id, &_secondary_flash_area);
        if (status != 0) {
            GLTH_LOGE(TAG, "flash_area_open error: %d", status);
            return GOLIOTH_ERR_FAIL;
        }

        GLTH_LOGD(TAG, "Secondary flash area:");
        GLTH_LOGD(TAG, "   fa_id        = %d", _secondary_flash_area->fa_id);
        GLTH_LOGD(TAG, "   fa_device_id = %d", _secondary_flash_area->fa_device_id);
        GLTH_LOGD(TAG, "   fa_off       = 0x%08" PRIx32, _secondary_flash_area->fa_off);
        GLTH_LOGD(TAG, "   fa_size      = %" PRIu32, (uint32_t)_secondary_flash_area->fa_size);

        // Erase secondary flash area
        uint32_t erase_size = _secondary_flash_area->fa_size;
        GLTH_LOGI(TAG, "Erasing %" PRIu32 " bytes of flash in secondary area", erase_size);
        status = flash_area_erase(_secondary_flash_area, 0, erase_size);
        GLTH_LOGI(TAG, "Done erasing flash");
        if (status != 0) {
            GLTH_LOGE(TAG, "flash_area_erase error: %d", status);
            return GOLIOTH_ERR_FAIL;
        }
    }

    // Write secondary flash area at offset
    status = flash_area_write(_secondary_flash_area, offset, block, block_size);
    if (status != 0) {
        GLTH_LOGE(TAG, "flash_area_write error: %d", status);
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

void fw_update_post_download(void) {
    if (_primary_flash_area) {
        flash_area_close(_primary_flash_area);
        _primary_flash_area = NULL;
    }

    if (_secondary_flash_area) {
        flash_area_close(_secondary_flash_area);
        _secondary_flash_area = NULL;
    }
}

enum golioth_status fw_update_validate(void) {
    // Nothing to do
    return GOLIOTH_OK;
}

enum golioth_status fw_update_change_boot_image(void) {
    GLTH_LOGD(TAG, "boot_set_pending");
    int status = boot_set_pending(0);
    if (status != 0) {
        GLTH_LOGE(TAG, "boot_set_pending failed, error %d", status);
        // Note: We are purposely ignoring this error, as it is expected
        // with the current version of MCUboot, which does not have
        // good support for PSOC6 and the 512-byte erase/write alignment
        // requirement.
        //
        // boot_write_trailer fails the alignment check, which prevents
        // the image trailer from being written.
        //
        // See https://github.com/mcu-tools/mcuboot/issues/713
        //
        // TODO - treat as an error if/when MCUboot supports 512-byte alignment.
    }
    return GOLIOTH_OK;
}

void fw_update_end(void) {
    // Nothing to do
}
