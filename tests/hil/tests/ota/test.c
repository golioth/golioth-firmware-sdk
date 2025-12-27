/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "golioth/golioth_status.h"
#include "golioth_port_config.h"
#include <golioth/client.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include <golioth/payload_utils.h>
#include <golioth/ota.h>
#include <string.h>

LOG_TAG_DEFINE(test_ota);

static golioth_sys_sem_t connected_sem;
static golioth_sys_sem_t reason_test_sem;
static golioth_sys_sem_t resume_test_sem;
static golioth_sys_sem_t manifest_get_test_sem;
static golioth_sys_sem_t manifest_get_cb_sem;

#define GOLIOTH_OTA_REASON_CNT 10
#define GOLIOTH_OTA_STATE_CNT 4
#define DUMMY_VER_OLDER "1.2.2"
#define DUMMY_VER_SAME "1.2.3"
#define DUMMY_VER_UPDATE "1.2.4"
#define DUMMY_VER_EXTRA "1.2.5"

static uint8_t callback_arg = 17;

#define SHA256_LEN 32
static uint8_t _saved_manifest_sha256[SHA256_LEN];

/* Arbitrarily large memory area as there's no way to know actual CBOR manifest size */
#define MANIFEST_CACHE 4096
static uint8_t manifest_cache[MANIFEST_CACHE];

struct golioth_client *client;

static struct golioth_ota_component stored_component;
struct resume_ctx
{
    bool in_progress;
    bool fail_pending;
    uint32_t block_idx;
    enum golioth_status status;
    golioth_sys_sem_t sem;
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
}

static enum golioth_status calc_hash(const uint8_t *data, size_t data_size, uint8_t *output)
{
    golioth_sys_sha256_t hash = golioth_sys_sha256_create();
    if (NULL == hash)
    {
        GLTH_LOGE(TAG, "Failed to allocate sha256 handle");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    enum golioth_status err = golioth_sys_sha256_update(hash, data, data_size);
    if (GOLIOTH_OK != err)
    {
        GLTH_LOGE(TAG, "Failed to calculate hash: %d", err);
        goto finish;
    }

    err = golioth_sys_sha256_finish(hash, output);
    if (GOLIOTH_OK != err)
    {
        GLTH_LOGE(TAG, "Failed to finish hash: %d", err);
        goto finish;
    }

finish:
    golioth_sys_sha256_destroy(hash);
    return err;
}

static void on_manifest_get(struct golioth_client *client,
                            enum golioth_status status,
                            const struct golioth_coap_rsp_code *coap_rsp_code,
                            const char *path,
                            const uint8_t *payload,
                            size_t payload_size,
                            void *arg)
{
    if ((status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        GLTH_LOGE(TAG, "Failed to get manifest (async): %d", status);
        goto finish;
    }
    else
    {
        GLTH_LOGI(TAG, "Manifest get successful");
    }

    uint8_t manifest_sha256[SHA256_LEN];

    enum golioth_status err = calc_hash(payload, payload_size, manifest_sha256);
    if (GOLIOTH_OK != err)
    {
        GLTH_LOGE(TAG, "Failed to calculate sha256: %d", err);
        goto finish;
    }

    if (memcmp(manifest_sha256, _saved_manifest_sha256, SHA256_LEN) != 0)
    {
        GLTH_LOGE(TAG, "Manifest SHAs do not match");
    }
    else
    {
        GLTH_LOGW(TAG, "Manifest get SHA matches stored SHA as expected");
    }

finish:
    golioth_sys_sem_give(manifest_get_cb_sem);
}

static enum golioth_status on_block(struct golioth_client *client,
                                    const char *path,
                                    uint32_t block_idx,
                                    const uint8_t *block_buffer,
                                    size_t block_buffer_len,
                                    bool is_last,
                                    size_t negotiated_block_size,
                                    void *arg)
{
    if (NULL == arg)
    {
        GLTH_LOGE(TAG, "arg cannot be NULL");
        return GOLIOTH_ERR_NULL;
    }

    size_t *bytes_cached_p = (size_t *) arg;

    size_t offset = negotiated_block_size * block_idx;

    if (MANIFEST_CACHE < (offset + block_buffer_len))
    {
        GLTH_LOGE(TAG, "Block %" PRIu32 " overflows manifest_cache", block_idx);
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    memcpy(manifest_cache + offset, block_buffer, block_buffer_len);
    *bytes_cached_p += block_buffer_len;

    GLTH_LOGI(TAG, "Cached manifest block: %" PRIu32, block_idx);

    return GOLIOTH_OK;
}

static void on_end(struct golioth_client *client,
                   enum golioth_status status,
                   const struct golioth_coap_rsp_code *coap_rsp_code,
                   const char *path,
                   uint32_t block_idx,
                   void *arg)
{
    if (NULL == arg)
    {
        GLTH_LOGE(TAG, "arg cannot be NULL");
        golioth_sys_sem_give(manifest_get_cb_sem);
        return;
    }

    size_t *bytes_cached_p = (size_t *) arg;

    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Blockwise manifest get failed: %d", status);
    }
    else
    {
        GLTH_LOGI(TAG, "Blockwise manifest get received %zu bytes", *bytes_cached_p);
    }

    uint8_t manifest_sha256[SHA256_LEN];

    enum golioth_status err = calc_hash(manifest_cache, *bytes_cached_p, manifest_sha256);
    if (GOLIOTH_OK != err)
    {
        GLTH_LOGE(TAG, "Failed to calculate sha256: %d", err);
        goto finish;
    }

    if (memcmp(manifest_sha256, _saved_manifest_sha256, SHA256_LEN) != 0)
    {
        GLTH_LOGE(TAG, "Manifest SHAs do not match");
    }
    else
    {
        GLTH_LOGW(TAG, "Manifest blockwise SHA matches stored SHA as expected");
    }

finish:
    free(bytes_cached_p);
    golioth_sys_sem_give(manifest_get_cb_sem);
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

static void test_manifest_get(void)
{
    enum golioth_status err;

    err = golioth_ota_get_manifest(client, on_manifest_get, (void *) GOLIOTH_OTA_REASON_CNT);
    if (GOLIOTH_OK != err)
    {
        GLTH_LOGE(TAG, "Failed to get manifest: %d", err);
    }
    else
    {
        golioth_sys_sem_take(manifest_get_cb_sem, 5 * 1000);
    }


    size_t *bytes_cached_p = (size_t *) golioth_sys_malloc(sizeof(size_t));
    if (NULL == bytes_cached_p)
    {
        GLTH_LOGE(TAG, "Failed to allocate bytes_cached");
        return;
    }

    *bytes_cached_p = 0;

    err = golioth_ota_blockwise_manifest(client, 0, on_block, on_end, bytes_cached_p);
    if (GOLIOTH_OK != err)
    {
        GLTH_LOGE(TAG, "Failed to get manifest: %d", err);
    }
    else
    {
        golioth_sys_sem_take(manifest_get_cb_sem, 5 * 1000);
    }
}

static void test_reason_and_state(void)
{
    enum golioth_status status;

    for (uint8_t i = 0; i < GOLIOTH_OTA_REASON_CNT; ++i)
    {
        enum golioth_ota_state state = i % GOLIOTH_OTA_STATE_CNT;
        enum golioth_ota_reason reason = i;
        GLTH_LOGI(TAG, "Updating status. Reason: %d State: %d", reason, state);
        status = golioth_ota_report_state(client,
                                          state,
                                          reason,
                                          "lobster",
                                          "2.3.4",
                                          "5.6.7",
                                          NULL,
                                          NULL);

        if (status != GOLIOTH_OK)
        {
            GLTH_LOGE(TAG, "Unable to report ota status: %d", status);
            break;
        }
        golioth_sys_msleep(500);
    }

    /* The log of this state check used as trigger by pytest and must come before null test */
    GLTH_LOGI(TAG, "golioth_ota_get_state: %d", golioth_ota_get_state());

    golioth_sys_msleep(1000);

    /* The log of this null test is used as a trigger by pytest and must always be last */
    status = golioth_ota_report_state(NULL, 0, 0, "lobster", "2.3.4", "5.6.7", NULL, NULL);
    GLTH_LOGE(TAG, "GOLIOTH_ERR_NULL: %d", status);
}

static enum golioth_status write_artifact_block_cb(const struct golioth_ota_component *component,
                                                   uint32_t block_idx,
                                                   const uint8_t *block_buffer,
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

    struct resume_ctx *ctx = arg;

    if (ctx->fail_pending && block_idx == 2)
    {
        ctx->fail_pending = false;
        return GOLIOTH_ERR_FAIL;
    }

    GLTH_LOGI(TAG, "Downloaded block: %" PRIu32, block_idx);
    golioth_sys_sha256_update(ctx->sha, block_buffer, block_buffer_len);

    return GOLIOTH_OK;
}

static void download_complete_cb(enum golioth_status status,
                                 const struct golioth_coap_rsp_code *coap_rsp_code,
                                 const struct golioth_ota_component *component,
                                 uint32_t block_idx,
                                 void *arg)
{
    struct resume_ctx *ctx = arg;

    ctx->status = status;
    ctx->block_idx = block_idx;

    if (GOLIOTH_OK == status)
    {
        GLTH_LOGI(TAG, "Block download complete!");

        unsigned char sha_output[SHA256_LEN];
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

    golioth_sys_sem_give(ctx->sem);
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
        .block_idx = 0,
        .status = GOLIOTH_OK,
        .sem = golioth_sys_sem_create(1, 0),
    };
    ctx.sha = golioth_sys_sha256_create();

    int counter = 0;

    while (counter < 10)
    {
        GLTH_LOGI(TAG, "Starting block download from %" PRIu32, ctx.block_idx);

        status = golioth_ota_download_component(client,
                                                walrus,
                                                ctx.block_idx,
                                                write_artifact_block_cb,
                                                download_complete_cb,
                                                (void *) &ctx);

        golioth_sys_sem_take(ctx.sem, GOLIOTH_SYS_WAIT_FOREVER);

        if (GOLIOTH_OK != ctx.status)
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

            enum golioth_status err = calc_hash(payload, payload_size, _saved_manifest_sha256);
            if (GOLIOTH_OK != err)
            {
                GLTH_LOGE(TAG, "Failed to store manifest hash: %d", err);
            }
            else
            {
                golioth_sys_sem_give(manifest_get_test_sem);
            }
        }
        else if (strcmp(main->version, DUMMY_VER_SAME) == 0)
        {
            // TODO: replace?
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
    resume_test_sem = golioth_sys_sem_create(1, 0);
    manifest_get_test_sem = golioth_sys_sem_create(1, 0);
    manifest_get_cb_sem = golioth_sys_sem_create(1, 0);

    golioth_debug_set_cloud_log_enabled(false);

    client = golioth_client_create(config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_sys_sem_take(connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    golioth_ota_manifest_subscribe(client, on_manifest, &callback_arg);

    while (1)
    {
        if (golioth_sys_sem_take(reason_test_sem, 0))
        {
            test_reason_and_state();
        }
        if (golioth_sys_sem_take(resume_test_sem, 0))
        {
            test_resume(&stored_component);
        }
        if (golioth_sys_sem_take(manifest_get_test_sem, 0))
        {
            test_manifest_get();
        }

        golioth_sys_msleep(100);
    }
}
