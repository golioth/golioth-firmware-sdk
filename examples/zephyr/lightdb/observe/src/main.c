/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightdb_observe, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

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

static void counter_observe_handler(struct golioth_client *client,
                                    enum golioth_status status,
                                    const struct golioth_coap_rsp_code *coap_rsp_code,
                                    const char *path,
                                    const uint8_t *payload,
                                    size_t payload_size,
                                    void *arg)
{
    if (status != GOLIOTH_OK)
    {
        LOG_WRN("Failed to receive observed payload: %d", status);
        return;
    }

    LOG_HEXDUMP_INF(payload, payload_size, "Counter (async)");

    return;
}

int main(void)
{
    LOG_DBG("Start LightDB observe sample");

    net_connect();

    /* Note: In production, credentials should be saved in secure storage. For
     * simplicity, we provide a utility that stores credentials as plaintext
     * settings.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    /* Observe LightDB State Path */
    golioth_lightdb_observe_async(client,
                                  "counter",
                                  GOLIOTH_CONTENT_TYPE_JSON,
                                  counter_observe_handler,
                                  NULL);

    while (true)
    {
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
