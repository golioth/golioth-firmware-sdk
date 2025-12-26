/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

#include <golioth/golioth_sys.h>
#include <golioth/ota.h>

#include "fw_update.h"

_Static_assert(sizeof(CONFIG_GOLIOTH_FW_UPDATE_PACKAGE_NAME)
                   <= CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN + 1,
               "GOLIOTH_FW_UPDATE_PACKAGE_NAME may be no longer than "
               "GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN");

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fw_update);

#define FW_MAX_BLOCK_RESUME_BEFORE_FAIL 15
#define FW_UPDATE_RESUME_DELAY_S 15
#define FW_REPORT_BACKOFF_MAX_S 180
#define FW_REPORT_MAX_RETRIES 5
#define FW_REPORT_RETRIES_INITAL_DELAY_S 5
#define BACKOFF_DURATION_INITIAL_MS 60 * 1000
#define BACKOFF_DURATION_MAX_MS 24 * 60 * 60 * 1000

#define FW_REPORT_COMPONENT_NAME 1 << 0
#define FW_REPORT_TARGET_VERSION 1 << 1
#define FW_REPORT_CURRENT_VERSION 1 << 2

struct golioth_fw_update_config
{
    /// The current firmware version, NULL-terminated, shallow-copied from user. (e.g. "1.2.3")
    const char *current_version;
    /// The name of the package in the manifest for the main firmware, NULL-terminated,
    /// shallow-copied from user (e.g. "main").
    const char *fw_package_name;
};

struct fw_update_component_context
{
    struct golioth_fw_update_config config;
    struct golioth_ota_component target_component;
    uint32_t backoff_duration_ms;
    uint32_t last_fail_ts;
};

struct download_progress_context
{
    size_t bytes_downloaded;
    uint32_t block_idx;
    uint8_t retries;
    enum golioth_status result;
    golioth_sys_sha256_t sha;
    golioth_sys_timer_t block_retry_timer;
};

struct block_retry_context
{
    struct fw_update_component_context *component_ctx;
    struct download_progress_context *download_ctx;
};

static struct golioth_client *_client;
static K_MUTEX_DEFINE(_manifest_update_mut);
static K_SEM_DEFINE(_manifest_rcvd, 0, 1);
static K_SEM_DEFINE(_download_complete, 0, 1);
static struct golioth_ota_manifest _ota_manifest;
static golioth_fw_update_state_change_callback _state_callback;
static void *_state_callback_arg;
static struct fw_update_component_context _component_ctx;

struct flash_img_context _flash_img_context;

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

/*
 * @note This is similar to Zephyr's flash_img_check() but uses the Golioth
 * port's sha256 API.
 */
static enum golioth_status fw_update_check_candidate(const uint8_t *hash, size_t img_size)
{
    enum golioth_status status = GOLIOTH_OK;
    int err;
    int buf_size;
    int pos;
    uint8_t calc_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN];

    err = flash_area_open(flash_img_get_upload_slot(),
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

static enum golioth_status fw_write_block_cb(const struct golioth_ota_component *component,
                                             uint32_t block_idx,
                                             const uint8_t *block_buffer,
                                             size_t block_buffer_len,
                                             bool is_last,
                                             size_t negotiated_block_size,
                                             void *arg)
{
    assert(arg);
    struct download_progress_context *ctx = arg;

    LOG_INF("Received block %" PRIu32 "/%zu",
            block_idx,
            (size_t) (component->size / negotiated_block_size));

    int err = 0;

    if (block_idx == 0)
    {
        err = flash_img_prepare(&_flash_img_context);
        if (err)
        {
            return GOLIOTH_ERR_IO;
        }
    }

    err = flash_img_buffered_write(&_flash_img_context, block_buffer, block_buffer_len, is_last);
    if (err)
    {
        LOG_ERR("Failed to write to flash: %d", err);
        return GOLIOTH_ERR_IO;
    }

    ctx->retries = 0;
    ctx->bytes_downloaded += block_buffer_len;
    golioth_sys_sha256_update(ctx->sha, block_buffer, block_buffer_len);

    return GOLIOTH_OK;
}

static void fw_download_end_cb(enum golioth_status status,
                               const struct golioth_coap_rsp_code *rsp_code,
                               const struct golioth_ota_component *component,
                               uint32_t block_idx,
                               void *arg)
{
    struct download_progress_context *ctx = arg;

    if (GOLIOTH_OK == status || GOLIOTH_ERR_IO == status
        || ctx->retries >= FW_MAX_BLOCK_RESUME_BEFORE_FAIL)
    {
        ctx->result = status;
        k_sem_give(&_download_complete);
    }
    else
    {
        ctx->retries++;
        ctx->block_idx = block_idx;

        LOG_WRN("Block (%" PRIu32 ") download failed, retrying (%" PRIu8 ")",
                ctx->block_idx,
                ctx->retries);

        golioth_sys_timer_start(ctx->block_retry_timer);
    }
}

static void block_retry_timer_expiry(golioth_sys_timer_t timer, void *arg)
{
    struct block_retry_context *ctx = arg;

    golioth_ota_download_component(_client,
                                   &ctx->component_ctx->target_component,
                                   ctx->download_ctx->block_idx,
                                   fw_write_block_cb,
                                   fw_download_end_cb,
                                   ctx->download_ctx);
}

enum golioth_status golioth_fw_update_report_state_sync(struct fw_update_component_context *ctx,
                                                        enum golioth_ota_state state,
                                                        enum golioth_ota_reason reason,
                                                        const int report_flags)
{
    if (!ctx)
    {
        return GOLIOTH_ERR_NULL;
    }

    if (_state_callback)
    {
        _state_callback(state, reason, _state_callback_arg);
    }

    enum golioth_status status = GOLIOTH_ERR_FAIL;
    uint32_t backoff_s = FW_REPORT_RETRIES_INITAL_DELAY_S;
    int retries_remaining = FW_REPORT_MAX_RETRIES;

    while (retries_remaining--)
    {
        status = golioth_ota_report_state_sync(
            _client,
            state,
            reason,
            (report_flags & FW_REPORT_COMPONENT_NAME) ? ctx->config.fw_package_name : NULL,
            (report_flags & FW_REPORT_CURRENT_VERSION) ? _component_ctx.config.current_version
                                                       : NULL,
            (report_flags & FW_REPORT_TARGET_VERSION) ? _component_ctx.target_component.version
                                                      : NULL,
            GOLIOTH_SYS_WAIT_FOREVER);

        if (status == GOLIOTH_OK)
        {
            LOG_DBG("State/Reason sent to server");
            return status;
        }

        LOG_ERR("Failed to report fw status: %d; retry in %" PRIu32 "s", status, backoff_s);

        golioth_sys_msleep(backoff_s * 1000);

        backoff_s *= 2;
        if (backoff_s > FW_REPORT_BACKOFF_MAX_S)
        {
            backoff_s = FW_REPORT_BACKOFF_MAX_S;
        }
    }

    LOG_ERR("Failed to report fw status: %d (%s)", status, golioth_status_to_str(status));
    return status;
}

static void on_ota_manifest(struct golioth_client *client,
                            enum golioth_status status,
                            const struct golioth_coap_rsp_code *coap_rsp_code,
                            const char *path,
                            const uint8_t *payload,
                            size_t payload_size,
                            void *arg)
{
    if (status != GOLIOTH_OK)
    {
        LOG_ERR("Error in OTA manifest observation: %d (%s)",
                status,
                golioth_status_to_str(status));
        return;
    }

    char manifest_str[payload_size + 1];
    memcpy(manifest_str, payload, payload_size);
    manifest_str[payload_size] = '\0';
    LOG_DBG("Received OTA manifest: %s", manifest_str);

    k_mutex_lock(&_manifest_update_mut, K_FOREVER);
    enum golioth_status parse_status =
        golioth_ota_payload_as_manifest(payload, payload_size, &_ota_manifest);
    k_mutex_unlock(&_manifest_update_mut);

    if (parse_status != GOLIOTH_OK)
    {
        LOG_ERR("Failed to parse manifest: %d (%s)", status, golioth_status_to_str(status));
        return;
    }
    k_sem_give(&_manifest_rcvd);
}

static void backoff_reset(struct fw_update_component_context *ctx)
{
    ctx->backoff_duration_ms = 0;
    ctx->last_fail_ts = 0;
}

static void backoff_increment(struct fw_update_component_context *ctx)
{
    if (ctx->backoff_duration_ms == 0)
    {
        ctx->backoff_duration_ms = BACKOFF_DURATION_INITIAL_MS;
    }
    else
    {
        ctx->backoff_duration_ms = ctx->backoff_duration_ms * 2;

        if (ctx->backoff_duration_ms > BACKOFF_DURATION_MAX_MS)
        {
            ctx->backoff_duration_ms = BACKOFF_DURATION_MAX_MS;
        }
    }

    ctx->last_fail_ts = (uint32_t) golioth_sys_now_ms();
}

static int32_t backoff_ms_before_expiration(struct fw_update_component_context *ctx)
{
    uint32_t actual_duration = ((uint32_t) golioth_sys_now_ms()) - ctx->last_fail_ts;

    if (actual_duration < ctx->backoff_duration_ms)
    {
        return (int32_t) (ctx->backoff_duration_ms - actual_duration);
    }

    return 0;
}

static bool received_new_target_component(const struct golioth_ota_manifest *manifest,
                                          struct fw_update_component_context *ctx)
{
    k_mutex_lock(&_manifest_update_mut, K_FOREVER);

    bool found_new = false;

    const struct golioth_ota_component *new_component =
        golioth_ota_find_component(manifest, ctx->config.fw_package_name);
    if (new_component)
    {
        LOG_INF("Current version = %s, Target version = %s",
                ctx->config.current_version,
                new_component->version);

        if (0 == strcmp(ctx->config.current_version, new_component->version))
        {
            LOG_INF("Current version matches target version.");
        }
        else if (ctx->backoff_duration_ms
                 && 0 == strcmp(ctx->target_component.version, new_component->version))
        {
            LOG_INF("Update to target version already in progress.");
        }
        else
        {
            memcpy(&ctx->target_component, new_component, sizeof(struct golioth_ota_component));
            backoff_reset(ctx);
            found_new = true;
        }
    }
    else
    {
        LOG_INF("Manifest does not contain target component: %s", ctx->config.fw_package_name);
        /* TODO: Report state/reason here.
         *  This can't be done directly because it would call a sync func from a callback
         *  Consider adding a new reason code: GOLIOTH_OTA_REASON_COMPONENT_NOT_FOUND
         */
    }

    k_mutex_unlock(&_manifest_update_mut);

    return found_new;
}

static void fw_observe_manifest(void)
{
    enum golioth_status status = GOLIOTH_ERR_NULL;
    uint32_t retry_delay_s = 5;

    while (status != GOLIOTH_OK)
    {
        status = golioth_ota_manifest_subscribe(_client, on_ota_manifest, NULL);

        if (status == GOLIOTH_OK)
        {
            break;
        }

        LOG_WRN("Failed to observe manifest, retry in %" PRIu32 "s: %d (%s)",
                retry_delay_s,
                status,
                golioth_status_to_str(status));

        golioth_sys_msleep(retry_delay_s * 1000);

        retry_delay_s = retry_delay_s * 2;

        if (retry_delay_s > CONFIG_GOLIOTH_FW_UPDATE_OBSERVATION_RETRY_MAX_DELAY_S)
        {
            retry_delay_s = CONFIG_GOLIOTH_FW_UPDATE_OBSERVATION_RETRY_MAX_DELAY_S;
        }
    }
}

static enum golioth_status fw_verify_component_hash(
    const uint8_t calc_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN],
    const uint8_t server_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN])
{
    if (memcmp(server_sha256, calc_sha256, GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN) == 0)
    {
        return GOLIOTH_OK;
    }
    else
    {
        LOG_ERR("Firmware download failed; sha256 doesn't match");

        /* TODO */
        // GLTH_LOG_BUFFER_HEXDUMP(TAG,
        //                         calc_sha256,
        //                         GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN,
        //                         GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);
    }

    return GOLIOTH_ERR_FAIL;
}

static void fw_download_failed(enum golioth_ota_reason reason)
{
    golioth_fw_update_report_state_sync(&_component_ctx,
                                        GOLIOTH_OTA_STATE_DOWNLOADING,
                                        reason,
                                        FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                            | FW_REPORT_TARGET_VERSION);
}

static enum golioth_status fw_change_image_and_reboot()
{
    LOG_INF("State = Updating");
    golioth_fw_update_report_state_sync(&_component_ctx,
                                        GOLIOTH_OTA_STATE_UPDATING,
                                        GOLIOTH_OTA_REASON_READY,
                                        FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                            | FW_REPORT_TARGET_VERSION);
    int err = boot_request_upgrade(BOOT_UPGRADE_TEST);
    if (err)
    {
        LOG_ERR("Firmware change boot image failed %d", err);
        return GOLIOTH_ERR_FAIL;
    }

    int countdown = 5;
    while (countdown > 0)
    {
        LOG_INF("Rebooting into new image in %d seconds", countdown);
        golioth_sys_msleep(1000);
        countdown--;
    }
    sys_reboot(SYS_REBOOT_WARM);

    // Should never be reached.
    return GOLIOTH_OK;
}


void golioth_fw_update_run(struct golioth_client *client, const char *version)
{
    _client = client;

    _component_ctx.config.fw_package_name = CONFIG_GOLIOTH_FW_UPDATE_PACKAGE_NAME;
    _component_ctx.config.current_version = version;

    backoff_reset(&_component_ctx);

    LOG_INF("Current firmware version: %s - %s",
            _component_ctx.config.fw_package_name,
            _component_ctx.config.current_version);

    // If it's the first time booting a new OTA image,
    // wait for successful connection to Golioth.
    //
    // If we don't connect after the configured period, roll back to the old image.
    if (!boot_is_img_confirmed())  // TODO: In port, guarded by CONFIG_BOOTLOADER_MCUBOOT
    {
        LOG_INF("Waiting for golioth client to connect before cancelling rollback");
        int seconds_elapsed = 0;
        while (seconds_elapsed < CONFIG_GOLIOTH_FW_UPDATE_ROLLBACK_TIMER_S)
        {
            if (golioth_client_is_connected(_client))
            {
                break;
            }
            golioth_sys_msleep(1000);
            seconds_elapsed++;
        }

        if (seconds_elapsed == CONFIG_GOLIOTH_FW_UPDATE_ROLLBACK_TIMER_S)
        {
            // We didn't connect to Golioth cloud, so something might be wrong with
            // this firmware. Roll back and reboot.
            LOG_WRN("Failed to connect to Golioth");
            LOG_WRN("!!!");
            LOG_WRN("!!! Rolling back and rebooting now!");
            LOG_WRN("!!!");
            if (!IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT))
            {
                return;
            }

            sys_reboot(SYS_REBOOT_WARM);
        }
        else
        {
            LOG_INF("Firmware updated successfully!");
            boot_write_img_confirmed();

            golioth_fw_update_report_state_sync(&_component_ctx,
                                                GOLIOTH_OTA_STATE_UPDATING,
                                                GOLIOTH_OTA_REASON_FIRMWARE_UPDATED_SUCCESSFULLY,
                                                FW_REPORT_COMPONENT_NAME
                                                    | FW_REPORT_CURRENT_VERSION);
        }
    }

    fw_observe_manifest();

    struct download_progress_context download_ctx;
    uint8_t calc_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN];

    while (1)
    {
        LOG_INF("State = Idle");
        golioth_fw_update_report_state_sync(&_component_ctx,
                                            GOLIOTH_OTA_STATE_IDLE,
                                            GOLIOTH_OTA_REASON_READY,
                                            FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                | FW_REPORT_TARGET_VERSION);

        LOG_INF("Waiting to receive OTA manifest");

        while (1)
        {
            k_timeout_t manifest_timeout = (_component_ctx.backoff_duration_ms == 0)
                ? K_FOREVER
                : K_MSEC(backoff_ms_before_expiration(&_component_ctx));

            LOG_INF("State = Idle");

            if (K_TIMEOUT_EQ(manifest_timeout, K_FOREVER))
            {
                golioth_fw_update_report_state_sync(&_component_ctx,
                                                    GOLIOTH_OTA_STATE_IDLE,
                                                    GOLIOTH_OTA_REASON_READY,
                                                    FW_REPORT_COMPONENT_NAME
                                                        | FW_REPORT_CURRENT_VERSION);
            }
            else
            {
                golioth_fw_update_report_state_sync(&_component_ctx,
                                                    GOLIOTH_OTA_STATE_IDLE,
                                                    GOLIOTH_OTA_REASON_AWAIT_RETRY,
                                                    FW_REPORT_COMPONENT_NAME
                                                        | FW_REPORT_CURRENT_VERSION
                                                        | FW_REPORT_TARGET_VERSION);
            }

            if (0 != k_sem_take(&_manifest_rcvd, manifest_timeout))
            {
                LOG_INF("Retry component download: %s", _component_ctx.config.fw_package_name);
                break;
            }

            LOG_INF("Received OTA manifest");

            bool new_component_received =
                received_new_target_component(&_ota_manifest, &_component_ctx);

            if (!new_component_received)
            {
                LOG_INF("Manifest does not contain different firmware version. Nothing to do.");
                continue;
            }

            // clang-format off
            if (fw_update_check_candidate(_component_ctx.target_component.hash,
                                          _component_ctx.target_component.size) == GOLIOTH_OK)
            // clang-format on
            {
                LOG_INF("Target component already downloaded. Attempting to update.");
                if (fw_change_image_and_reboot() != GOLIOTH_OK)
                {
                    LOG_ERR("Failed to reboot into new image. Attempting to download again.");
                }
            }

            break;
        }

        LOG_INF("State = Downloading");
        golioth_fw_update_report_state_sync(&_component_ctx,
                                            GOLIOTH_OTA_STATE_DOWNLOADING,
                                            GOLIOTH_OTA_REASON_READY,
                                            FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                | FW_REPORT_TARGET_VERSION);

        uint64_t start_time_ms = golioth_sys_now_ms();
        download_ctx.bytes_downloaded = 0;
        download_ctx.retries = 0;
        download_ctx.sha = golioth_sys_sha256_create();

        int err;

        struct block_retry_context retry_context = {
            .download_ctx = &download_ctx,
            .component_ctx = &_component_ctx,
        };

        struct golioth_timer_config retry_timer_config = {
            .name = "",
            .expiration_ms = FW_UPDATE_RESUME_DELAY_S * 1000,
            .fn = block_retry_timer_expiry,
            .user_arg = &retry_context,
        };
        download_ctx.block_retry_timer = golioth_sys_timer_create(&retry_timer_config);

        err = golioth_ota_download_component(_client,
                                             &_component_ctx.target_component,
                                             0,
                                             fw_write_block_cb,
                                             fw_download_end_cb,
                                             &download_ctx);

        if (GOLIOTH_OK == err)
        {
            k_sem_take(&_download_complete, K_FOREVER);
            err = download_ctx.result;
        }
        else
        {
            LOG_ERR("Failed to start OTA component download: %d (%s)",
                    err,
                    golioth_status_to_str(err));
        }

        golioth_sys_timer_destroy(download_ctx.block_retry_timer);

        /* Download finished, prepare backoff in case needed */
        backoff_increment(&_component_ctx);

        if (err != GOLIOTH_OK)
        {
            switch (err)
            {
                case GOLIOTH_ERR_IO:
                    fw_download_failed(GOLIOTH_OTA_REASON_IO);
                    break;
                default:
                    fw_download_failed(GOLIOTH_OTA_REASON_FIRMWARE_UPDATE_FAILED);
                    break;
            }

            golioth_sys_sha256_destroy(download_ctx.sha);
            continue;
        }

        golioth_sys_sha256_finish(download_ctx.sha, calc_sha256);
        golioth_sys_sha256_destroy(download_ctx.sha);

        if (GOLIOTH_OK
            != fw_verify_component_hash(calc_sha256, _component_ctx.target_component.hash))
        {
            fw_download_failed(GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE);
            continue;
        }
        else
        {
            LOG_DBG("SHA256 matches server");
        }

        LOG_INF("Successfully downloaded %zu bytes in %" PRIu64 " ms",
                download_ctx.bytes_downloaded,
                golioth_sys_now_ms() - start_time_ms);
        (void) start_time_ms;

        LOG_INF("State = Downloaded");
        golioth_fw_update_report_state_sync(&_component_ctx,
                                            GOLIOTH_OTA_STATE_DOWNLOADED,
                                            GOLIOTH_OTA_REASON_READY,
                                            FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                | FW_REPORT_TARGET_VERSION);

        /* Download successful. Reset backoff */
        backoff_reset(&_component_ctx);

        if (fw_change_image_and_reboot() != GOLIOTH_OK)
        {
            LOG_ERR("Failed to reboot into new image.");
        }
    }
}

void golioth_fw_update_register_state_change_callback(
    golioth_fw_update_state_change_callback callback,
    void *user_arg)
{
    _state_callback = callback;
    _state_callback_arg = user_arg;
}
