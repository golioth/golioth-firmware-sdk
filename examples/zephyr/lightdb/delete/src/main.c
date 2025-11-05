/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightdb_delete, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

#define APP_TIMEOUT_S 5

struct golioth_client *client;
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

static void counter_handler(struct golioth_client *client,
                            enum golioth_status status,
                            const struct golioth_coap_rsp_code *coap_rsp_code,
                            const char *path,
                            void *arg)
{
    if (status != GOLIOTH_OK)
    {
        LOG_WRN("Failed to deleted counter: %d (%s)", status, golioth_status_to_str(status));
        return;
    }

    LOG_DBG("Counter deleted successfully");

    return;
}

static void counter_delete_async(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_delete_async(client, "counter", counter_handler, NULL);
    if (err)
    {
        LOG_WRN("failed to delete data from LightDB: %d (%s)", err, golioth_status_to_str(err));
    }
}

static void counter_delete_sync(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_delete_sync(client, "counter", APP_TIMEOUT_S);
    if (err)
    {
        LOG_WRN("failed to delete data from LightDB: %d (%s)", err, golioth_status_to_str(err));
    }

    LOG_DBG("Counter deleted successfully");
}

int main(void)
{
    LOG_DBG("Start LightDB delete sample");

    net_connect();

    /* Note: In production, credentials should be saved in secure storage. For
     * simplicity, we provide a utility that stores credentials as plaintext
     * settings.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true)
    {
        LOG_DBG("Before request (async)");
        counter_delete_async(client);
        LOG_DBG("After request (async)");

        k_sleep(K_SECONDS(5));

        LOG_DBG("Before request (sync)");
        counter_delete_sync(client);
        LOG_DBG("After request (sync)");

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
