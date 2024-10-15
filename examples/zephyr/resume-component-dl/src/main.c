/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(resume_component_dl, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/fw_update.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>
#include <mbedtls/sha256.h>


#define MODEL_PACKAGE_NAME "image"

struct golioth_ota_component *stored_component = NULL;
bool _dl_in_progress = false;
bool _fail_pending = true;


static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void on_manifest(struct golioth_client *client,
                        const struct golioth_response *response,
                        const char *path,
                        const uint8_t *payload,
                        size_t payload_size,
                        void *arg)
{
    struct golioth_ota_manifest man;

    int err = golioth_ota_payload_as_manifest(payload, payload_size, &man);

    if (err)
    {
        LOG_ERR("Error converting payload to manifest: %d (check GOLIOTH_OTA_MAX_NUM_COMPONENTS)",
                err);
        return;
    }

    for (int i = 0; i < man.num_components; i++)
    {
        LOG_INF("Package found: %s", man.components[i].package);

        size_t pn_len = strlen(man.components[i].package);
        if (pn_len > CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN)
        {
            LOG_ERR("Package name length limited to %d but got %zu",
                    CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN,
                    pn_len);
        }
        else if (strcmp(MODEL_PACKAGE_NAME, man.components[i].package) != 0)
        {
            LOG_INF("Skipping download for package name: %s", man.components[i].package);
        }
        else
        {
            LOG_INF("Queueing for download: %s", man.components[i].package);

            stored_component =
                (struct golioth_ota_component *) malloc(sizeof(struct golioth_ota_component));

            if (!stored_component)
            {
                LOG_ERR("Unable to allocate memory to store component");
                return;
            }

            memcpy(stored_component, &man.components[i], sizeof(struct golioth_ota_component));
            return;
        }
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
        GLTH_LOGE(TAG, "arg is NULL but should be a SHA256 context");
        return GOLIOTH_ERR_INVALID_FORMAT;
    }

    mbedtls_sha256_context *sha = (mbedtls_sha256_context *) arg;

    if (block_idx == 0)
    {
        mbedtls_sha256_starts(sha, 0);
    }

    if (_fail_pending && block_idx == 2)
    {
        _fail_pending = false;
        return GOLIOTH_ERR_FAIL;
    }

    LOG_INF("Downloaded block: %d", block_idx);
    mbedtls_sha256_update(sha, block_buffer, block_buffer_len);

    if (is_last)
    {
        GLTH_LOGI(TAG, "Block download complete!");

        unsigned char sha_output[32];
        mbedtls_sha256_finish(sha, sha_output);

        unsigned char sha_server[32];
        hex2bin(component->hash, strlen(component->hash), sha_server, sizeof(sha_server));

        LOG_HEXDUMP_WRN(sha_output, sizeof(sha_output), "Downloaded hash");
        LOG_HEXDUMP_WRN(sha_server, sizeof(sha_server), "Server hash");

        if (memcmp(sha_output, sha_server, sizeof(sha_output)) == 0)
        {
            LOG_WRN("SHA256 matches as expected!!");
        }
        else
        {
            LOG_ERR("SHA256 doesn't match expected.");
        }
    }
    return GOLIOTH_OK;
}

int main(void)
{
    LOG_DBG("Start Resumable Component Download sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    struct golioth_client *client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    /* Listen for OTA manifest */
    golioth_ota_observe_manifest_async(client, on_manifest, NULL);

    k_sem_take(&connected, K_FOREVER);

    uint32_t next_block_idx = 0;
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);

    int counter = 0;
    while(1)
    {
        LOG_INF("Loopstart: %i", counter);
        counter++;

        if (stored_component)
        {
            if (!_dl_in_progress) {
                _dl_in_progress = true;
                _fail_pending = true;
                next_block_idx = 0;
            }

            enum golioth_status status =
                golioth_ota_download_component(client,
                                               stored_component,
                                               &next_block_idx,
                                               write_artifact_block_cb,
                                               (void *) &sha_ctx);

            if (status == GOLIOTH_OK)
            {
                _dl_in_progress = false;
                free(stored_component);
                stored_component = NULL;
            }
        }

        k_msleep(5000);
    }
}
