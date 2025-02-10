/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

#include <golioth/golioth_status.h>
#include <golioth/golioth_sys.h>
#include <golioth/fw_update.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_fw_zephyr);

/*
 * @note This can be replaced with flash_img_get_upload_slot() on update to new
 * Zephyr release.
 * https://github.com/zephyrproject-rtos/zephyr/commit/beffba6d874dc13fcec04adbcc66ab672d5740f8
 */
#include <zephyr/devicetree.h>
#ifdef CONFIG_TRUSTED_EXECUTION_NONSECURE
#define UPLOAD_FLASH_AREA_LABEL slot1_ns_partition
#else
#if FIXED_PARTITION_EXISTS(slot1_partition)
#define UPLOAD_FLASH_AREA_LABEL slot1_partition
#else
#define UPLOAD_FLASH_AREA_LABEL slot0_partition
#endif
#endif

/* FIXED_PARTITION_ID() values used below are auto-generated by DT */
#define UPLOAD_FLASH_AREA_ID FIXED_PARTITION_ID(UPLOAD_FLASH_AREA_LABEL)

struct flash_img_context _flash_img_context;

bool fw_update_is_pending_verify(void)
{
    if (!IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT))
    {
        return false;
    }

    return !boot_is_img_confirmed();
}

void fw_update_rollback(void) {}

void fw_update_reboot(void)
{
    if (!IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT))
    {
        return;
    }

    sys_reboot(SYS_REBOOT_WARM);
}

/*
 * @note This is a copy of ERASED_VAL_32() from mcumgr.
 */
#define ERASED_VAL_32(x) (((x) << 24) | ((x) << 16) | ((x) << 8) | (x))

/**
 * Determines if the specified area of flash is completely unwritten.
 *
 * @note This is a copy of zephyr_img_mgmt_flash_check_empty() from mcumgr.
 */
static int flash_area_check_empty(const struct flash_area *fa, bool *out_empty)
{
    uint32_t data[16];
    off_t addr;
    off_t end;
    int bytes_to_read;
    int rc;
    int i;
    uint8_t erased_val;
    uint32_t erased_val_32;

    __ASSERT_NO_MSG(fa->fa_size % 4 == 0);

    erased_val = flash_area_erased_val(fa);
    erased_val_32 = ERASED_VAL_32(erased_val);

    end = fa->fa_size;
    for (addr = 0; addr < end; addr += sizeof(data))
    {
        if (end - addr < sizeof(data))
        {
            bytes_to_read = end - addr;
        }
        else
        {
            bytes_to_read = sizeof(data);
        }

        rc = flash_area_read(fa, addr, data, bytes_to_read);
        if (rc != 0)
        {
            flash_area_close(fa);
            return rc;
        }

        for (i = 0; i < bytes_to_read / 4; i++)
        {
            if (data[i] != erased_val_32)
            {
                *out_empty = false;
                flash_area_close(fa);
                return 0;
            }
        }
    }

    *out_empty = true;

    return 0;
}

static int flash_img_erase_if_needed(struct flash_img_context *ctx)
{
    bool empty;
    int err;

    if (IS_ENABLED(CONFIG_IMG_ERASE_PROGRESSIVELY))
    {
        return 0;
    }

    err = flash_area_check_empty(ctx->flash_area, &empty);
    if (err)
    {
        return err;
    }

    if (empty)
    {
        return 0;
    }

    err = flash_area_erase(ctx->flash_area, 0, ctx->flash_area->fa_size);
    if (err)
    {
        return err;
    }

    return 0;
}

static const char *swap_type_str(int swap_type)
{
    switch (swap_type)
    {
        case BOOT_SWAP_TYPE_NONE:
            return "none";
        case BOOT_SWAP_TYPE_TEST:
            return "test";
        case BOOT_SWAP_TYPE_PERM:
            return "perm";
        case BOOT_SWAP_TYPE_REVERT:
            return "revert";
        case BOOT_SWAP_TYPE_FAIL:
            return "fail";
    }

    return "unknown";
}

static int flash_img_prepare(struct flash_img_context *flash)
{
    int swap_type;
    int err;

    swap_type = mcuboot_swap_type();
    switch (swap_type)
    {
        case BOOT_SWAP_TYPE_REVERT:
            LOG_WRN("'revert' swap type detected, it is not safe to continue");
            return -EBUSY;
        default:
            LOG_INF("swap type: %s", swap_type_str(swap_type));
            break;
    }

    err = flash_img_init(flash);
    if (err)
    {
        LOG_ERR("failed to init: %d", err);
        return err;
    }

    err = flash_img_erase_if_needed(flash);
    if (err)
    {
        LOG_ERR("failed to erase: %d", err);
        return err;
    }

    return 0;
}

void fw_update_cancel_rollback(void)
{
    if (!IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT))
    {
        return;
    }

    boot_write_img_confirmed();
}

enum golioth_status fw_update_handle_block(const uint8_t *block,
                                           size_t block_size,
                                           size_t offset,
                                           size_t total_size)
{
    int err;

    if (!IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT))
    {
        return GOLIOTH_OK;
    }

    if (offset == 0)
    {
        err = flash_img_prepare(&_flash_img_context);
        if (err)
        {
            return GOLIOTH_ERR_IO;
        }
    }

    err = flash_img_buffered_write(&_flash_img_context, block, block_size, false);
    if (err)
    {
        LOG_ERR("Failed to write to flash: %d", err);
        return GOLIOTH_ERR_IO;
    }

    return GOLIOTH_OK;
}

enum golioth_status fw_update_post_download(void)
{
    int err;

    if (!_flash_img_context.stream.buf_bytes)
    {
        return GOLIOTH_OK;
    }

    err = flash_img_buffered_write(&_flash_img_context, NULL, 0, true);
    if (err)
    {
        LOG_ERR("Failed to write to flash: %d", err);
        return GOLIOTH_ERR_IO;
    }

    return GOLIOTH_OK;
}

/*
 * @note This is similar to Zephyr's flash_img_check() but uses the Golioth
 * port's sha256 API.
 */
enum golioth_status fw_update_check_candidate(const uint8_t *hash, size_t img_size)
{
    enum golioth_status status = GOLIOTH_OK;
    int err;
    int buf_size;
    int pos;
    uint8_t calc_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN];

    err = flash_area_open(UPLOAD_FLASH_AREA_ID,
                          (const struct flash_area **) &(_flash_img_context.flash_area));
    if (err)
    {
        return GOLIOTH_ERR_IO;
    };

    golioth_sys_sha256_t hash_ctx = golioth_sys_sha256_create();

    buf_size = sizeof(_flash_img_context.buf);

    for (pos = 0; pos < img_size; pos += buf_size)
    {
        if (pos + buf_size > img_size)
        {
            buf_size = img_size - pos;
        }

        err = flash_read(_flash_img_context.flash_area->fa_dev,
                         (_flash_img_context.flash_area->fa_off + pos),
                         _flash_img_context.buf,
                         buf_size);
        if (err != 0)
        {
            status = GOLIOTH_ERR_IO;
            goto error;
        }

        err = golioth_sys_sha256_update(hash_ctx, _flash_img_context.buf, buf_size);
        if (err != GOLIOTH_OK)
        {
            status = GOLIOTH_ERR_FAIL;
            goto error;
        }
    }

    err = golioth_sys_sha256_finish(hash_ctx, calc_sha256);
    if (err != GOLIOTH_OK)
    {
        status = GOLIOTH_ERR_FAIL;
        goto error;
    }

    if (memcmp(hash, calc_sha256, sizeof(calc_sha256)))
    {
        status = GOLIOTH_ERR_FAIL;
        goto error;
    }

error:
    golioth_sys_sha256_destroy(hash_ctx);

    return status;
}

enum golioth_status fw_update_change_boot_image(void)
{
    int err;

    if (!IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT))
    {
        return GOLIOTH_ERR_NOT_IMPLEMENTED;
    }

    err = boot_request_upgrade(BOOT_UPGRADE_TEST);
    if (err)
    {
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

void fw_update_end(void) {}
