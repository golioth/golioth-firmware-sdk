/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lightdb_get, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <golioth/payload_utils.h>
#include <golioth/zcbor_utils.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zcbor_decode.h>
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

static void counter_get_handler(struct golioth_client *client,
                                enum golioth_status status,
                                const struct golioth_coap_rsp_code *coap_rsp_code,
                                const char *path,
                                const uint8_t *payload,
                                size_t payload_size,
                                void *arg)
{
    if ((status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        LOG_WRN("Failed to get counter (async): %d", status);
    }
    else
    {
        LOG_INF("Counter (async): %d", golioth_payload_as_int(payload, payload_size));
    }
}

static void counter_get_async(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_get_async(client,
                                    "counter",
                                    GOLIOTH_CONTENT_TYPE_JSON,
                                    counter_get_handler,
                                    NULL);
    if (err)
    {
        LOG_WRN("failed to get data from LightDB: %d", err);
    }
}

static void counter_get_sync(struct golioth_client *client)
{
    int32_t value;
    int err;

    err = golioth_lightdb_get_int_sync(client, "counter", &value, APP_TIMEOUT_S);
    if (err)
    {
        LOG_WRN("failed to get data from LightDB: %d", err);
    }
    else
    {
        LOG_INF("Counter (sync): %d", value);
    }
}

static void counter_get_json_sync(struct golioth_client *client)
{
    uint8_t sbuf[128];
    size_t len = sizeof(sbuf);
    int err;

    /* Get root of LightDB State, but JSON can be returned for any path */
    err =
        golioth_lightdb_get_sync(client, "", GOLIOTH_CONTENT_TYPE_JSON, sbuf, &len, APP_TIMEOUT_S);
    if (err || (0 == strlen(sbuf)))
    {
        LOG_WRN("failed to get JSON data from LightDB: %d", err);
    }
    else
    {
        LOG_HEXDUMP_INF(sbuf, len, "LightDB JSON (sync)");
    }
}

static void counter_get_cbor_handler(struct golioth_client *client,
                                     enum golioth_status status,
                                     const struct golioth_coap_rsp_code *coap_rsp_code,
                                     const char *path,
                                     const uint8_t *payload,
                                     size_t payload_size,
                                     void *arg)
{
    if ((status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        LOG_WRN("Failed to get counter (async): %d", status);
        return;
    }

    ZCBOR_STATE_D(zsd, 1, payload, payload_size, 1, 0);
    int64_t counter;
    struct zcbor_map_entry map_entry =
        ZCBOR_TSTR_LIT_MAP_ENTRY("counter", zcbor_map_int64_decode, &counter);

    int err = zcbor_map_decode(zsd, &map_entry, 1);
    if (err)
    {
        LOG_WRN("Failed to decode CBOR data: %d", err);
    }

    LOG_INF("Counter (CBOR async): %d", (uint32_t) counter);
}

static void counter_get_cbor_async(struct golioth_client *client)
{
    int err = golioth_lightdb_get_async(client,
                                        "",
                                        GOLIOTH_CONTENT_TYPE_CBOR,
                                        counter_get_cbor_handler,
                                        NULL);
    if (err)
    {
        LOG_WRN("Failed to get data from LightDB: %d", err);
    }
}

int main(void)
{
    LOG_DBG("Start LightDB get sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true)
    {
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

        LOG_INF("Before CBOR request (async)");
        counter_get_cbor_async(client);
        LOG_INF("After CBOR request (async)");

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
