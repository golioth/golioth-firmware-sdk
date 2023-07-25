/*
 * copyright (c) 2023 golioth, inc.
 *
 * spdx-license-identifier: apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightdb_get, LOG_LEVEL_DBG);

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

static void counter_get_handler(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        LOG_WRN("Failed to set counter: %d", response->status);
        return;
    }

    LOG_INF("Counter (async): %d", golioth_payload_as_int(payload, payload_size));

    return;
}

static void counter_get_async(golioth_client_t* client) {
    int err;

    err = golioth_lightdb_get_async(client, "counter", counter_get_handler, NULL);
    if (err) {
        LOG_WRN("failed to get data from LightDB: %d", err);
    }
}

static void counter_get_sync(golioth_client_t* client) {
    uint32_t value;
    int err;

    err = golioth_lightdb_get_int_sync(client, "counter", &value, APP_TIMEOUT_S);
    if (err) {
        LOG_WRN("failed to get data from LightDB: %d", err);
    }

    LOG_INF("Counter (sync): %d", value);
}

static void counter_get_json_sync(golioth_client_t* client) {
    uint8_t sbuf[128];
    size_t len = sizeof(sbuf);
    int err;

    /* Get root of LightDB State, but JSON can be returned for any path */
    err = golioth_lightdb_get_json_sync(client, "", sbuf, len, APP_TIMEOUT_S);
    if (err) {
        LOG_WRN("failed to get JSON data from LightDB: %d", err);
        return;
    }

    LOG_HEXDUMP_INF(sbuf, strlen(sbuf), "LightDB JSON (sync)");
}

int main(void) {
    LOG_DBG("Start LightDB get sample");

    IF_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON, (net_connect();))

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    golioth_client_config_t* client_config = hardcoded_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true) {
        LOG_INF("Before request (async)");
        counter_get_async(client);
        LOG_INF("After request (async)");

        k_sleep(K_SECONDS(5));

        LOG_INF("Before request (sync)");
        counter_get_sync(client);
        LOG_INF("After request (sync)");

        k_sleep(K_SECONDS(5));

        LOG_INF("Before JSON request (sync)");
        counter_get_json_sync(client);
        LOG_INF("After JSON request (sync)");

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
