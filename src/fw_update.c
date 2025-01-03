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
    golioth_sys_sha256_t sha;
};

static struct golioth_client *_client;
static golioth_sys_mutex_t _manifest_update_mut;
static golioth_sys_sem_t _manifest_rcvd;
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
                                             uint8_t *block_buffer,
                                             size_t block_buffer_len,
                                             bool is_last,
                                             size_t negotiated_block_size,
                                             void *arg)
{
    assert(arg);
    struct download_progress_context *ctx = (struct download_progress_context *) arg;

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
        ctx->bytes_downloaded += block_buffer_len;
        golioth_sys_sha256_update(ctx->sha, block_buffer, block_buffer_len);

        if (is_last)
        {
            fw_update_post_download();
        }
    }

    return status;
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

    GLTH_LOGE(TAG, "Failed to report fw status: %u", status);
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
        GLTH_LOGE(TAG, "Error in OTA manifest observation: %d", status);
        return;
    }

    GLTH_LOGD(TAG, "Received OTA manifest: %.*s", (int) payload_size, payload);

    golioth_sys_mutex_lock(_manifest_update_mut, GOLIOTH_SYS_WAIT_FOREVER);
    enum golioth_status parse_status =
        golioth_ota_payload_as_manifest(payload, payload_size, &_ota_manifest);
    golioth_sys_mutex_unlock(_manifest_update_mut);

    if (parse_status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Failed to parse manifest: %s", golioth_status_to_str(parse_status));
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
        status = golioth_ota_observe_manifest_async(_client, on_ota_manifest, NULL);

        if (status == GOLIOTH_OK)
        {
            break;
        }

        GLTH_LOGW(TAG,
                  "Failed to observe manifest, retry in %" PRIu32 "s: %d",
                  retry_delay_s,
                  status);

        golioth_sys_msleep(retry_delay_s * 1000);

        retry_delay_s = retry_delay_s * 2;

        if (retry_delay_s > CONFIG_GOLIOTH_FW_UPDATE_OBSERVATION_RETRY_MAX_DELAY_S)
        {
            retry_delay_s = CONFIG_GOLIOTH_FW_UPDATE_OBSERVATION_RETRY_MAX_DELAY_S;
        }
    }
}

static enum golioth_status fw_verify_component_hash(
    struct download_progress_context *ctx,
    const uint8_t server_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN])
{
    uint8_t calc_sha256[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN];
    golioth_sys_sha256_finish(ctx->sha, calc_sha256);

    if (memcmp(server_sha256, calc_sha256, sizeof(calc_sha256)) == 0)
    {
        return GOLIOTH_OK;
    }
    else
    {
        GLTH_LOGE(TAG,
                  "Firmware download failed; Recieved %u bytes but sha256 doesn't match",
                  ctx->bytes_downloaded);

        GLTH_LOG_BUFFER_HEXDUMP(TAG,
                                calc_sha256,
                                sizeof(calc_sha256),
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

static void fw_update_thread(void *arg)
{
    // If it's the first time booting a new OTA image,
    // wait for successful connection to Golioth.
    //
    // If we don't connect after 60 seconds, roll back to the old image.
    if (fw_update_is_pending_verify())
    {
        GLTH_LOGI(TAG, "Waiting for golioth client to connect before cancelling rollback");
        int seconds_elapsed = 0;
        while (seconds_elapsed < 60)
        {
            if (golioth_client_is_connected(_client))
            {
                break;
            }
            golioth_sys_msleep(1000);
            seconds_elapsed++;
        }

        if (seconds_elapsed == 60)
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
        uint32_t next_block = 0;
        download_ctx.sha = golioth_sys_sha256_create();

        int err;

        struct block_retry_cnt
        {
            uint32_t idx;
            int count;
        } block_retries;

        block_retries.idx = 0;
        block_retries.count = 0;
        bool block_retries_reported = false;

        while (1)
        {
            err = golioth_ota_download_component(_client,
                                                 &_component_ctx.target_component,
                                                 &next_block,
                                                 fw_write_block_cb,
                                                 (void *) &download_ctx);

            if (err == GOLIOTH_OK)
            {
                break;
            }

            if (err == GOLIOTH_ERR_IO)
            {
                GLTH_LOGE(TAG, "Failed to store OTA component");
                break;
            }

            if (block_retries.idx == next_block)
            {
                block_retries.count++;
            }
            else
            {
                block_retries.idx = next_block;
                block_retries.count = 1;
            }

            if (block_retries.count >= FW_MAX_BLOCK_RESUME_BEFORE_FAIL)
            {
                GLTH_LOGE(TAG,
                          "Max resume count of %i for single block reached; aborting download.",
                          block_retries.count);
                break;
            }
            else
            {
                GLTH_LOGI(TAG,
                          "Block download failed at block idx: %" PRIu32 "; status: %s; resuming",
                          next_block,
                          golioth_status_to_str(err));

                if (!block_retries_reported)
                {
                    golioth_fw_update_report_state_sync(&_component_ctx,
                                                        GOLIOTH_OTA_STATE_DOWNLOADING,
                                                        GOLIOTH_OTA_REASON_CONNECTION_LOST,
                                                        FW_REPORT_COMPONENT_NAME
                                                            | FW_REPORT_CURRENT_VERSION
                                                            | FW_REPORT_TARGET_VERSION);
                }

                golioth_sys_msleep(FW_UPDATE_RESUME_DELAY_S * 1000);

                if (!block_retries_reported)
                {
                    golioth_fw_update_report_state_sync(&_component_ctx,
                                                        GOLIOTH_OTA_STATE_DOWNLOADING,
                                                        GOLIOTH_OTA_REASON_READY,
                                                        FW_REPORT_COMPONENT_NAME
                                                            | FW_REPORT_CURRENT_VERSION
                                                            | FW_REPORT_TARGET_VERSION);

                    block_retries_reported = true;
                }
            }
        }

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

        if (GOLIOTH_OK
            != fw_verify_component_hash(&download_ctx, _component_ctx.target_component.hash))
        {
            fw_download_failed(GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE);
            golioth_sys_sha256_destroy(download_ctx.sha);
            continue;
        }
        else
        {
            golioth_sys_sha256_destroy(download_ctx.sha);
            GLTH_LOGD(TAG, "SHA256 matches server");
        }

        GLTH_LOGI(TAG,
                  "Successfully downloaded %zu bytes in %" PRIu64 " ms",
                  download_ctx.bytes_downloaded,
                  golioth_sys_now_ms() - start_time_ms);

        if (fw_update_validate() != GOLIOTH_OK)
        {
            GLTH_LOGE(TAG, "Firmware validate failed");
            fw_update_end();

            GLTH_LOGI(TAG, "State = Idle");
            golioth_fw_update_report_state_sync(&_component_ctx,
                                                GOLIOTH_OTA_STATE_IDLE,
                                                GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE,
                                                FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                    | FW_REPORT_TARGET_VERSION);

            continue;
        }

        GLTH_LOGI(TAG, "State = Downloaded");
        golioth_fw_update_report_state_sync(&_component_ctx,
                                            GOLIOTH_OTA_STATE_DOWNLOADED,
                                            GOLIOTH_OTA_REASON_READY,
                                            FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                | FW_REPORT_TARGET_VERSION);

        GLTH_LOGI(TAG, "State = Updating");
        golioth_fw_update_report_state_sync(&_component_ctx,
                                            GOLIOTH_OTA_STATE_UPDATING,
                                            GOLIOTH_OTA_REASON_READY,
                                            FW_REPORT_COMPONENT_NAME | FW_REPORT_CURRENT_VERSION
                                                | FW_REPORT_TARGET_VERSION);

        if (fw_update_change_boot_image() != GOLIOTH_OK)
        {
            GLTH_LOGE(TAG, "Firmware change boot image failed");
            fw_update_end();
            continue;
        }

        /* Download successful. Reset backoff */
        backoff_reset(&_component_ctx);

        int countdown = 5;
        while (countdown > 0)
        {
            GLTH_LOGI(TAG, "Rebooting into new image in %d seconds", countdown);
            golioth_sys_msleep(1000);
            countdown--;
        }
        fw_update_reboot();
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
