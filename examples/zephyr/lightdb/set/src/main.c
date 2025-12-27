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

static void counter_set_handler(struct golioth_client *client,
                                enum golioth_status status,
                                const struct golioth_coap_rsp_code *coap_rsp_code,
                                const char *path,
                                void *arg)
{
    if (status != GOLIOTH_OK)
    {
        LOG_WRN("Failed to set counter: %d (%s)", status, golioth_status_to_str(status));
        return;
    }

    LOG_DBG("Counter successfully set");

    return;
}

static void counter_set(int counter)
{
    int err;

    err = golioth_lightdb_set_int(client, "counter", counter, counter_set_handler, NULL);
    if (err)
    {
        LOG_WRN("Failed to set counter: %d (%s)", err, golioth_status_to_str(err));
        return;
    }
}

static void counter_set_json(int counter)
{
    char sbuf[sizeof("{\"counter\":4294967295}")];
    int err;

    snprintk(sbuf, sizeof(sbuf), "{\"counter\":%d}", counter);

    err = golioth_lightdb_set(client,
                              "",
                              GOLIOTH_CONTENT_TYPE_JSON,
                              sbuf,
                              strlen(sbuf),
                              counter_set_handler,
                              NULL);
    if (err)
    {
        LOG_WRN("Failed to set counter: %d (%s)", err, golioth_status_to_str(err));
        return;
    }
}

static void counter_set_cbor(int counter)
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

    int err = golioth_lightdb_set(client,
                                  "",
                                  GOLIOTH_CONTENT_TYPE_CBOR,
                                  buf,
                                  payload_size,
                                  counter_set_handler,
                                  NULL);
    if (err)
    {
        LOG_WRN("Failed to set counter: %d (%s)", err, golioth_status_to_str(err));
    }
}

int main(void)
{
    int counter = 0;

    LOG_DBG("Start LightDB set sample");

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
        /* Using int */
        LOG_DBG("Setting counter to %d", counter);

        LOG_DBG("Before request");
        counter_set(counter);
        LOG_DBG("After request");

        counter++;
        k_sleep(K_SECONDS(5));

        /* Using JSON object */
        LOG_DBG("Setting counter to %d", counter);

        LOG_DBG("Before request (json)");
        counter_set_json(counter);
        LOG_DBG("After request (json)");

        counter++;
        k_sleep(K_SECONDS(5));

        /* Using CBOR object */
        LOG_DBG("Setting counter to %d", counter);


        LOG_DBG("Before request (cbor)");
        counter_set_cbor(counter);
        LOG_DBG("After request (cbor)");

        counter++;
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
