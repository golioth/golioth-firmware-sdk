/*
 * copyright (c) 2023 golioth, inc.
 *
 * spdx-license-identifier: apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightdb_delete, LOG_LEVEL_DBG);

#include "golioth.h"
#include <samples/common/hardcoded_credentials.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

#define APP_TIMEOUT_S 1

golioth_client_t client;
static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(golioth_client_t client, golioth_client_event_t event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void counter_handler(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        LOG_WRN("Failed to deleted counter: %d", response->status);
        return;
    }

    LOG_DBG("Counter deleted successfully");

    return;
}

static void counter_delete_async(golioth_client_t* client) {
    int err;

    err = golioth_lightdb_delete_async(client, "counter", counter_handler, NULL);
    if (err) {
        LOG_WRN("failed to delete data from LightDB: %d", err);
    }
}

static void counter_delete_sync(golioth_client_t* client) {
    int err;

    err = golioth_lightdb_delete_sync(client, "counter", APP_TIMEOUT_S);
    if (err) {
        LOG_WRN("failed to delete data from LightDB: %d", err);
    }

    LOG_DBG("Counter deleted successfully");
}

int main(void) {
    LOG_DBG("Start LightDB delete sample");

    IF_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON, (net_connect();))

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const golioth_client_config_t* client_config = hardcoded_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true) {
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
