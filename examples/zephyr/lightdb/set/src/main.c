/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightdb_set, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zcbor_encode.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

#define APP_TIMEOUT_S 1

struct golioth_client* client;
static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client* client, enum golioth_client_event event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void counter_set_handler(
        struct golioth_client* client,
        const golioth_response_t* response,
        const char* path,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        LOG_WRN("Failed to set counter: %d", response->status);
        return;
    }

    LOG_DBG("Counter successfully set");

    return;
}

static void counter_set_async(int counter) {
    int err;

    err = golioth_lightdb_set_int_async(client, "counter", counter, counter_set_handler, NULL);
    if (err) {
        LOG_WRN("Failed to set counter: %d", err);
        return;
    }
}

static void counter_set_sync(int counter) {
    int err;

    err = golioth_lightdb_set_int_sync(client, "counter", counter, APP_TIMEOUT_S);
    if (err) {
        LOG_WRN("Failed to set counter: %d", err);
        return;
    }

    LOG_DBG("Counter successfully set");
}

static void counter_set_json_async(int counter) {
    char sbuf[sizeof("{\"counter\":4294967295}")];
    int err;

    snprintk(sbuf, sizeof(sbuf), "{\"counter\":%d}", counter);

    err = golioth_lightdb_set_async(client, "", GOLIOTH_CONTENT_TYPE_JSON,
                                    sbuf, strlen(sbuf), counter_set_handler, NULL);
    if (err) {
        LOG_WRN("Failed to set counter: %d", err);
        return;
    }
}

static void counter_set_cbor_sync(int counter)
{
    uint8_t buf[32];
    ZCBOR_STATE_E(zse, 1, buf, sizeof(buf), 1);

    bool ok = zcbor_map_start_encode(zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to start CBOR encoding");
        return;
    }

    ok = zcbor_tstr_put_lit(zse, "counter");
    if (!ok)
    {
        LOG_ERR("CBOR: Failed to encode counter name");
        return;
    }

    ok = zcbor_int32_put(zse, counter);
    if (!ok)
    {
        LOG_ERR("CBOR: failed to encode counter value");
        return;
    }

    ok = zcbor_map_end_encode(zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to close CBOR map object");
        return;
    }

    size_t payload_size = (intptr_t) zse->payload - (intptr_t) buf;

    int err = golioth_lightdb_set_sync(client, "", GOLIOTH_CONTENT_TYPE_CBOR,
                             buf, payload_size, APP_TIMEOUT_S);
    if (err != 0)
    {
        LOG_WRN("Failed to set counter: %d", err);
    }
    else
    {
        LOG_DBG("Counter successfully set");
    }

}

int main(void) {
    int counter = 0;

    LOG_DBG("Start LightDB set sample");

    IF_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON, (net_connect();))

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const golioth_client_config_t* client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true) {
        /* Callback-based using int */
        LOG_DBG("Setting counter to %d", counter);

        LOG_DBG("Before request (async)");
        counter_set_async(counter);
        LOG_DBG("After request (async)");

        counter++;
        k_sleep(K_SECONDS(5));

        /* Synchrnous using int */
        LOG_DBG("Setting counter to %d", counter);

        LOG_DBG("Before request (sync)");
        counter_set_sync(counter);
        LOG_DBG("After request (sync)");

        counter++;
        k_sleep(K_SECONDS(5));

        /* Callback-based using JSON object */
        LOG_DBG("Setting counter to %d", counter);

        LOG_DBG("Before request (json async)");
        counter_set_json_async(counter);
        LOG_DBG("After request (json async)");

        counter++;
        k_sleep(K_SECONDS(5));

        /* Synchronous using CBOR object */
        LOG_DBG("Setting counter to %d", counter);


        LOG_DBG("Before request (cbor sync)");
        counter_set_cbor_sync(counter);
        LOG_DBG("After request (cbor sync)");

        counter++;
        k_sleep(K_SECONDS(5));

    }

    return 0;
}
