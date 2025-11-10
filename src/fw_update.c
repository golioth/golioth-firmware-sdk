/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// TODO - fw_update_destroy, to clean up resources and stop thread

#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <golioth/golioth_sys.h>
#include <golioth/fw_update.h>
#include "golioth/ota.h"

_Static_assert(sizeof(CONFIG_GOLIOTH_FW_UPDATE_PACKAGE_NAME)
                   <= CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN + 1,
               "GOLIOTH_FW_UPDATE_PACKAGE_NAME may be no longer than "
               "GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN");

#if defined(CONFIG_GOLIOTH_FW_UPDATE)

LOG_TAG_DEFINE(golioth_fw_update);

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
static golioth_sys_mutex_t _manifest_update_mut;
static golioth_sys_sem_t _manifest_rcvd;
static golioth_sys_sem_t _download_complete;
static struct golioth_ota_manifest _ota_manifest;
static golioth_fw_update_state_change_callback _state_callback;
static void *_state_callback_arg;
static struct fw_update_component_context _component_ctx;

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

    GLTH_LOGI(TAG,
              "Received block %" PRIu32 "/%zu",
              block_idx,
              (size_t) (component->size / negotiated_block_size));

    enum golioth_status status = fw_update_handle_block(block_buffer,
                                                        block_buffer_len,
                                                        negotiated_block_size * block_idx,
                                                        component->size);

    if (status == GOLIOTH_OK)
    {
        ctx->retries = 0;
        ctx->bytes_downloaded += block_buffer_len;
        golioth_sys_sha256_update(ctx->sha, block_buffer, block_buffer_len);
    }

    return status;
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
        golioth_sys_sem_give(_download_complete);
    }
    else
    {
        ctx->retries++;
        ctx->block_idx = block_idx;

        GLTH_LOGW(TAG,
                  "Block (%" PRIu32 ") download failed, retrying (%" PRIu8 ")",
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
            GLTH_LOGD(TAG, "State/Reason sent to server");
            return status;
        }

        GLTH_LOGE(TAG, "Failed to report fw status: %d; retry in %" PRIu32 "s", status, backoff_s);

        golioth_sys_msleep(backoff_s * 1000);

        backoff_s *= 2;
        if (backoff_s > FW_REPORT_BACKOFF_MAX_S)
        {
            backoff_s = FW_REPORT_BACKOFF_MAX_S;
        }
    }

    GLTH_LOGE(TAG, "Failed to report fw status: %d (%s)", status, golioth_status_to_str(status));
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
        GLTH_LOGE(TAG,
                  "Error in OTA manifest observation: %d (%s)",
                  status,
                  golioth_status_to_str(status));
        return;
    }

    char manifest_str[payload_size + 1];
    memcpy(manifest_str, payload, payload_size);
    manifest_str[payload_size] = '\0';
    GLTH_LOGD(TAG, "Received OTA manifest: %s", manifest_str);

    golioth_sys_mutex_lock(_manifest_update_mut, GOLIOTH_SYS_WAIT_FOREVER);
    enum golioth_status parse_status =
        golioth_ota_payload_as_manifest(payload, payload_size, &_ota_manifest);
    golioth_sys_mutex_unlock(_manifest_update_mut);

    if (parse_status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Failed to parse manifest: %d (%s)", status, golioth_status_to_str(status));
        return;
    }
    golioth_sys_sem_give(_manifest_rcvd);
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
    golioth_sys_mutex_lock(_manifest_update_mut, GOLIOTH_SYS_WAIT_FOREVER);

    bool found_new = false;

    const struct golioth_ota_component *new_component =
        golioth_ota_find_component(manifest, ctx->config.fw_package_name);
    if (new_component)
    {
        GLTH_LOGI(TAG,
                  "Current version = %s, Target version = %s",
                  ctx->config.current_version,
                  new_component->version);

        if (0 == strcmp(ctx->config.current_version, new_component->version))
        {
            GLTH_LOGI(TAG, "Current version matches target version.");
        }
        else if (ctx->backoff_duration_ms
                 && 0 == strcmp(ctx->target_component.version, new_component->version))
        {
            GLTH_LOGI(TAG, "Update to target version already in progress.");
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
        GLTH_LOGI(TAG,
                  "Manifest does not contain target component: %s",
                  ctx->config.fw_package_name);
        /* TODO: Report state/reason here.
         *  This can't be done directly because it would call a sync func from a callback
         *  Consider adding a new reason code: GOLIOTH_OTA_REASON_COMPONENT_NOT_FOUND
         */
    }

    golioth_sys_mutex_unlock(_manifest_update_mut);

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

        GLTH_LOGW(TAG,
                  "Failed to observe manifest, retry in %" PRIu32 "s: %d (%s)",
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
        GLTH_LOGE(TAG, "Firmware download failed; sha256 doesn't match");

        GLTH_LOG_BUFFER_HEXDUMP(TAG,
                                calc_sha256,
                                GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN,
                                GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);
    }

    return GOLIOTH_ERR_FAIL;
}

static void fw_download_failed(enum golioth_ota_reason reason)
{
    fw_update_end();
    golioth_fw_update_report_state_sync(&_component_ctx,
                                        GOLIOTH_OTA_STATE_DOWNLOADING,
                                        reason,
                                        FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                            | FW_REPORT_TARGET_VERSION);
}

static enum golioth_status fw_change_image_and_reboot()
{
    GLTH_LOGI(TAG, "State = Updating");
    golioth_fw_update_report_state_sync(&_component_ctx,
                                        GOLIOTH_OTA_STATE_UPDATING,
                                        GOLIOTH_OTA_REASON_READY,
                                        FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                            | FW_REPORT_TARGET_VERSION);
    enum golioth_status status = fw_update_change_boot_image();
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Firmware change boot image failed");
        return status;
    }

    int countdown = 5;
    while (countdown > 0)
    {
        GLTH_LOGI(TAG, "Rebooting into new image in %d seconds", countdown);
        golioth_sys_msleep(1000);
        countdown--;
    }
    fw_update_reboot();

    // Should never be reached.
    return GOLIOTH_OK;
}


static void fw_update_thread(void *arg)
{
    // If it's the first time booting a new OTA image,
    // wait for successful connection to Golioth.
    //
    // If we don't connect after the configured period, roll back to the old image.
    if (fw_update_is_pending_verify())
    {
        GLTH_LOGI(TAG, "Waiting for golioth client to connect before cancelling rollback");
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
            GLTH_LOGW(TAG, "Failed to connect to Golioth");
            GLTH_LOGW(TAG, "!!!");
            GLTH_LOGW(TAG, "!!! Rolling back and rebooting now!");
            GLTH_LOGW(TAG, "!!!");
            fw_update_rollback();
            fw_update_reboot();
        }
        else
        {
            GLTH_LOGI(TAG, "Firmware updated successfully!");
            fw_update_cancel_rollback();

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
        GLTH_LOGI(TAG, "State = Idle");
        golioth_fw_update_report_state_sync(&_component_ctx,
                                            GOLIOTH_OTA_STATE_IDLE,
                                            GOLIOTH_OTA_REASON_READY,
                                            FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                | FW_REPORT_TARGET_VERSION);

        GLTH_LOGI(TAG, "Waiting to receive OTA manifest");

        while (1)
        {
            int32_t manifest_timeout = (_component_ctx.backoff_duration_ms == 0)
                ? GOLIOTH_SYS_WAIT_FOREVER
                : backoff_ms_before_expiration(&_component_ctx);

            GLTH_LOGI(TAG, "State = Idle");

            if (manifest_timeout == GOLIOTH_SYS_WAIT_FOREVER)
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

            if (!golioth_sys_sem_take(_manifest_rcvd, manifest_timeout))
            {
                GLTH_LOGI(TAG,
                          "Retry component download: %s",
                          _component_ctx.config.fw_package_name);
                break;
            }

            GLTH_LOGI(TAG, "Received OTA manifest");

            bool new_component_received =
                received_new_target_component(&_ota_manifest, &_component_ctx);

            if (!new_component_received)
            {
                GLTH_LOGI(TAG,
                          "Manifest does not contain different firmware version. Nothing to do.");
                continue;
            }

            // clang-format off
            if (fw_update_check_candidate(_component_ctx.target_component.hash,
                                          _component_ctx.target_component.size) == GOLIOTH_OK)
            // clang-format on
            {
                GLTH_LOGI(TAG, "Target component already downloaded. Attempting to update.");
                if (fw_change_image_and_reboot() != GOLIOTH_OK)
                {
                    GLTH_LOGE(TAG,
                              "Failed to reboot into new image. Attempting to download again.");
                }
            }

            break;
        }

        GLTH_LOGI(TAG, "State = Downloading");
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
            golioth_sys_sem_take(_download_complete, GOLIOTH_SYS_WAIT_FOREVER);
            err = download_ctx.result;
        }
        else
        {
            GLTH_LOGE(TAG,
                      "Failed to start OTA component download: %d (%s)",
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

        if (GOLIOTH_OK != fw_update_post_download())
        {
            GLTH_LOGE(TAG, "Failed to perform post download operations");
            fw_download_failed(GOLIOTH_OTA_REASON_FIRMWARE_UPDATE_FAILED);
            continue;
        };

        if (GOLIOTH_OK
            != fw_verify_component_hash(calc_sha256, _component_ctx.target_component.hash))
        {
            fw_download_failed(GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE);
            continue;
        }
        else
        {
            GLTH_LOGD(TAG, "SHA256 matches server");
        }

        GLTH_LOGI(TAG,
                  "Successfully downloaded %zu bytes in %" PRIu64 " ms",
                  download_ctx.bytes_downloaded,
                  golioth_sys_now_ms() - start_time_ms);
        (void) start_time_ms;

        GLTH_LOGI(TAG, "State = Downloaded");
        golioth_fw_update_report_state_sync(&_component_ctx,
                                            GOLIOTH_OTA_STATE_DOWNLOADED,
                                            GOLIOTH_OTA_REASON_READY,
                                            FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                | FW_REPORT_TARGET_VERSION);

        /* Download successful. Reset backoff */
        backoff_reset(&_component_ctx);

        if (fw_change_image_and_reboot() != GOLIOTH_OK)
        {
            GLTH_LOGE(TAG, "Failed to reboot into new image.");
            fw_update_end();
        }
    }
}

void golioth_fw_update_init(struct golioth_client *client, const char *current_version)
{
    struct golioth_fw_update_config config = {
        .current_version = current_version,
        .fw_package_name = CONFIG_GOLIOTH_FW_UPDATE_PACKAGE_NAME,
    };
    golioth_fw_update_init_with_config(client, &config);
}

void golioth_fw_update_init_with_config(struct golioth_client *client,
                                        const struct golioth_fw_update_config *config)
{
    static bool initialized = false;

    _client = client;

    _component_ctx.config = *config;
    backoff_reset(&_component_ctx);

    _manifest_update_mut = golioth_sys_mutex_create();  // never destroyed
    _manifest_rcvd = golioth_sys_sem_create(1, 0);      // never destroyed
    _download_complete = golioth_sys_sem_create(1, 0);  // never destroyed

    GLTH_LOGI(TAG,
              "Current firmware version: %s - %s",
              _component_ctx.config.fw_package_name,
              _component_ctx.config.current_version);

    if (!initialized)
    {
        struct golioth_thread_config thread_cfg = {
            .name = "fw_update",
            .fn = fw_update_thread,
            .user_arg = NULL,
            .stack_size = CONFIG_GOLIOTH_FW_UPDATE_THREAD_STACK_SIZE,
            .prio = 3,
        };

        golioth_sys_thread_t thread = golioth_sys_thread_create(&thread_cfg);
        if (!thread)
        {
            GLTH_LOGE(TAG, "Failed to create firmware update thread");
        }
        else
        {
            initialized = true;
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

#endif  // CONFIG_GOLIOTH_FW_UPDATE
