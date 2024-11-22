/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/client.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include <golioth/payload_utils.h>
#include <golioth/ota.h>
#include <string.h>

LOG_TAG_DEFINE(test_ota);

static golioth_sys_sem_t connected_sem;
static golioth_sys_sem_t reason_test_sem;
static golioth_sys_sem_t block_test_sem;
static golioth_sys_sem_t resume_test_sem;

#define GOLIOTH_OTA_REASON_CNT 10
#define GOLIOTH_OTA_STATE_CNT 4
#define TEST_BLOCK_CNT 42
#define DUMMY_VER_OLDER "1.2.2"
#define DUMMY_VER_SAME "1.2.3"
#define DUMMY_VER_UPDATE "1.2.4"
#define DUMMY_VER_EXTRA "1.2.5"

static uint8_t callback_arg = 17;
static uint8_t block_buf[CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE];

struct golioth_client *client;

static struct golioth_ota_component stored_component;
struct resume_ctx
{
    bool in_progress;
    bool fail_pending;
    golioth_sys_sha256_t sha;
};

static void log_component_members(const struct golioth_ota_component *component)
{
    char hash_string[GOLIOTH_OTA_COMPONENT_HEX_HASH_LEN + 1];
    for (int i = 0; i < sizeof(component->hash); i++)
    {
        int write_idx = i * 2;
        if (write_idx > (GOLIOTH_OTA_COMPONENT_HEX_HASH_LEN - 2))
        {
            GLTH_LOGE(TAG, "Error converting component hash to string");
            return;
        }

        sprintf(hash_string + write_idx, "%02x", component->hash[i]);
    }

    GLTH_LOGI(TAG, "component.package: %s", component->package);
    GLTH_LOGI(TAG, "component.version: %s", component->version);
    GLTH_LOGI(TAG, "component.size: %u", (unsigned int) component->size);
    GLTH_LOGI(TAG, "component.hash: %s", hash_string);
    GLTH_LOGI(TAG, "component.uri: %s", component->uri);
    GLTH_LOGI(TAG, "component.bootloader: %s", component->bootloader);
}

static void test_manifest_decoding(struct golioth_ota_manifest *manifest,
                                   const struct golioth_ota_component *main)
{
    const struct golioth_ota_component *absent;

    absent = golioth_ota_find_component(manifest, "absent");

    if (absent != NULL)
    {
        GLTH_LOGE(TAG, "Found absent component, but we expected NULL");
    }
    else
    {
        GLTH_LOGI(TAG, "No absent component found");
    }

    log_component_members(main);
}

static void test_multiple_artifacts(struct golioth_ota_manifest *manifest,
                                    const struct golioth_ota_component *main)
{
    const struct golioth_ota_component *walrus;

    walrus = golioth_ota_find_component(manifest, "walrus");

    if (walrus == NULL)
    {
        GLTH_LOGE(TAG, "No walrus component found");
    }
    else
    {
        GLTH_LOGI(TAG, "Found walrus component");
    }

    log_component_members(walrus);
}

static void test_reason_and_state(void)
{
    enum golioth_status status;

    for (uint8_t i = 0; i < GOLIOTH_OTA_REASON_CNT; ++i)
    {
        enum golioth_ota_state state = i % GOLIOTH_OTA_STATE_CNT;
        enum golioth_ota_reason reason = i;
        GLTH_LOGI(TAG, "Updating status. Reason: %d State: %d", reason, state);
        status =
            golioth_ota_report_state_sync(client, state, reason, "lobster", "2.3.4", "5.6.7", 2);

        if (status != GOLIOTH_OK)
        {
            GLTH_LOGE(TAG, "Unable to report ota status: %d", status);
        }
        else
        {
            GLTH_LOGI(TAG, "OTA status reported successfully");
        }
        golioth_sys_msleep(4000);
    }

    GLTH_LOGI(TAG, "golioth_ota_get_state: %d", golioth_ota_get_state());

    golioth_sys_msleep(5000);

    status = golioth_ota_report_state_sync(NULL, 0, 0, "lobster", "2.3.4", "5.6.7", 2);
    GLTH_LOGE(TAG, "GOLIOTH_ERR_NULL: %d", status);
}

static void test_block_ops(void)
{
    enum golioth_status status;
    size_t block_index = 0;
    size_t block_nbytes;
    bool is_last = false;

    GLTH_LOGI(TAG,
              "golioth_ota_size_to_nblocks: %zu",
              golioth_ota_size_to_nblocks(
                  (TEST_BLOCK_CNT * CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE) + 1));

    // Test NULL client
    status = golioth_ota_get_block_sync(NULL,
                                        "main",
                                        DUMMY_VER_SAME,
                                        block_index,
                                        block_buf,
                                        &block_nbytes,
                                        &is_last,
                                        10);
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Block sync failed: %d", status);
    }
    else
    {
        GLTH_LOGW(TAG, "We expected to receive an error but got a block instead");
    }

    // Test Download block

    status = GOLIOTH_OK;
    block_index = 0;

    while ((is_last == false) && (status == GOLIOTH_OK))
    {
        status = golioth_ota_get_block_sync(client,
                                            "main",
                                            DUMMY_VER_SAME,
                                            block_index,
                                            block_buf,
                                            &block_nbytes,
                                            &is_last,
                                            10);
        if (status != GOLIOTH_OK)
        {
            GLTH_LOGE(TAG, "Block sync failed: %d", status);
        }
        else
        {
            GLTH_LOGI(TAG, "Received block %zu", block_index);
            GLTH_LOGI(TAG, "is_last: %d", is_last);
        }

        ++block_index;
    }
}

enum golioth_status write_artifact_block_cb(const struct golioth_ota_component *component,
                                            uint32_t block_idx,
                                            uint8_t *block_buffer,
                                            size_t block_buffer_len,
                                            bool is_last,
                                            size_t negotiated_block_size,
                                            void *arg)
{
    if (!arg)
    {
        GLTH_LOGE(TAG, "arg is NULL but should be a resume_ctx");
        return GOLIOTH_ERR_INVALID_FORMAT;
    }

    struct resume_ctx *ctx = (struct resume_ctx *) arg;

    if (ctx->fail_pending && block_idx == 2)
    {
        ctx->fail_pending = false;
        return GOLIOTH_ERR_FAIL;
    }

    GLTH_LOGI(TAG, "Downloaded block: %" PRIu32, block_idx);
    golioth_sys_sha256_update(ctx->sha, block_buffer, block_buffer_len);

    if (is_last)
    {
        GLTH_LOGI(TAG, "Block download complete!");

        unsigned char sha_output[32];
        golioth_sys_sha256_finish(ctx->sha, sha_output);

        if (sizeof(sha_output) == sizeof(component->hash)
            && memcmp(sha_output, component->hash, sizeof(component->hash)) == 0)
        {
            GLTH_LOGW(TAG, "SHA256 matches as expected!!");

            /* Small sleep to help ensure logs are not out of order */
            golioth_sys_msleep(100);
        }
        else
        {
            GLTH_LOGE(TAG, "SHA256 doesn't match expected.");
        }
    }
    return GOLIOTH_OK;
}

static void test_resume(struct golioth_ota_component *walrus)
{
    GLTH_LOGI(TAG, "Start resumable blockwise download test");

    if (!walrus)
    {
        GLTH_LOGE(TAG, "OTA component is NULL");
    }
    enum golioth_status status;

    struct resume_ctx ctx = {
        .in_progress = false,
        .fail_pending = true,
    };
    ctx.sha = golioth_sys_sha256_create();

    uint32_t block_idx = 0;
    int counter = 0;

    while (counter < 10)
    {
        GLTH_LOGI(TAG, "Starting block download from %" PRIu32, block_idx);

        status = golioth_ota_download_component(client,
                                                walrus,
                                                &block_idx,
                                                write_artifact_block_cb,
                                                (void *) &ctx);

        if (status)
        {
            GLTH_LOGE(TAG, "Block download failed (will resume): %d", status);
        }
        else
        {
            GLTH_LOGI(TAG, "Block download successful!");

            golioth_sys_sha256_destroy(ctx.sha);
            ctx.sha = NULL;
            return;
        }

        counter++;
        golioth_sys_msleep(3000);
    }

    golioth_sys_sha256_destroy(ctx.sha);
    GLTH_LOGE(TAG, "Blockwise resume test failed.");
}

static void on_manifest(struct golioth_client *client,
                        enum golioth_status status,
                        const struct golioth_coap_rsp_code *coap_rsp_code,
                        const char *path,
                        const uint8_t *payload,
                        size_t payload_size,
                        void *arg)
{
    enum golioth_status parse_status;
    struct golioth_ota_manifest manifest;
    const struct golioth_ota_component *main;

    if ((status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        GLTH_LOGE(TAG, "Failed to get manifest (async): %d", status);
    }
    else
    {
        GLTH_LOGI(TAG, "Manifest received");
    }

    parse_status = golioth_ota_payload_as_manifest(payload, payload_size, &manifest);

    if (parse_status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Failed to decode manifest");
    }
    else
    {
        GLTH_LOGI(TAG, "Manifest successfully decoded");
    }

    main = golioth_ota_find_component(&manifest, "main");

    if (main == NULL)
    {
        const struct golioth_ota_component *walrus =
            golioth_ota_find_component(&manifest, "walrus");
        if (!walrus)
        {
            GLTH_LOGI(TAG, "Manifest doesn't have main or walrus components, skipping this one.");
            return;
        }
        else
        {
            memcpy(&stored_component, walrus, sizeof(struct golioth_ota_component));

            golioth_sys_sem_give(resume_test_sem);
        }
    }
    else
    {
        GLTH_LOGI(TAG, "Found main component");
        if (strcmp(main->version, DUMMY_VER_OLDER) == 0)
        {
            test_manifest_decoding(&manifest, main);
        }
        else if (strcmp(main->version, DUMMY_VER_EXTRA) == 0)
        {
            test_multiple_artifacts(&manifest, main);
        }
        else if (strcmp(main->version, DUMMY_VER_SAME) == 0)
        {
            golioth_sys_sem_give(block_test_sem);
        }
        else if (strcmp(main->version, DUMMY_VER_UPDATE) == 0)
        {
            golioth_sys_sem_give(reason_test_sem);
        }
    }
}

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    if (event == GOLIOTH_CLIENT_EVENT_CONNECTED)
    {
        golioth_sys_sem_give(connected_sem);
    }
}

void hil_test_entry(const struct golioth_client_config *config)
{
    connected_sem = golioth_sys_sem_create(1, 0);
    reason_test_sem = golioth_sys_sem_create(1, 0);
    block_test_sem = golioth_sys_sem_create(1, 0);
    resume_test_sem = golioth_sys_sem_create(1, 0);

    golioth_debug_set_cloud_log_enabled(false);

    client = golioth_client_create(config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_sys_sem_take(connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    golioth_ota_observe_manifest_async(client, on_manifest, &callback_arg);

    while (1)
    {
        if (golioth_sys_sem_take(reason_test_sem, 0))
        {
            test_reason_and_state();
        }
        if (golioth_sys_sem_take(block_test_sem, 0))
        {
            test_block_ops();
        }
        if (golioth_sys_sem_take(resume_test_sem, 0))
        {
            test_resume(&stored_component);
        }

        golioth_sys_msleep(100);
    }
}
