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
        LOG_WRN("Failed to get counter: %d (%s)", status, golioth_status_to_str(status));
    }
    else
    {
        LOG_INF("Counter: %d", golioth_payload_as_int(payload, payload_size));
    }
}

static void counter_get(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_get(client,
                              "counter",
                              GOLIOTH_CONTENT_TYPE_JSON,
                              counter_get_handler,
                              NULL);
    if (err)
    {
        LOG_WRN("failed to get data from LightDB: %d (%s)", err, golioth_status_to_str(err));
    }
}

static void counter_get_json_handler(struct golioth_client *client,
                                     enum golioth_status status,
                                     const struct golioth_coap_rsp_code *coap_rsp_code,
                                     const char *path,
                                     const uint8_t *payload,
                                     size_t payload_size,
                                     void *arg)
{
    if ((GOLIOTH_OK != status) || golioth_payload_is_null(payload, payload_size))
    {
        LOG_ERR("Error fetching LightDB JSON: %d (%s)", status, golioth_status_to_str(status));
    }
    else
    {
        char sbuf[128];
        snprintf(sbuf, sizeof(sbuf), "LightDB JSON: %.*s", (int) payload_size, payload);
        LOG_INF("LightDB JSON: %s", payload);
    }
}

static void counter_get_json(struct golioth_client *client)
{
    /* Get root of LightDB State, but JSON can be returned for any path */
    enum golioth_status status =
        golioth_lightdb_get(client, "", GOLIOTH_CONTENT_TYPE_JSON, counter_get_json_handler, NULL);
    if (GOLIOTH_OK != status)
    {
        LOG_ERR("failed to get JSON data from LightDB: %d (%s)",
                status,
                golioth_status_to_str(status));
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
        LOG_WRN("Failed to get counter: %d (%s)", status, golioth_status_to_str(status));
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

    LOG_INF("Counter (CBOR): %d", (uint32_t) counter);
}

static void counter_get_cbor(struct golioth_client *client)
{
    int err =
        golioth_lightdb_get(client, "", GOLIOTH_CONTENT_TYPE_CBOR, counter_get_cbor_handler, NULL);
    if (err)
    {
        LOG_WRN("Failed to get data from LightDB: %d (%s)", err, golioth_status_to_str(err));
    }
}

int main(void)
{
    LOG_DBG("Start LightDB get sample");

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
        LOG_INF("Before request");
        counter_get(client);
        LOG_INF("After request");

        k_sleep(K_SECONDS(5));

        LOG_INF("Before JSON request");
        counter_get_json(client);
        LOG_INF("After JSON request");

        k_sleep(K_SECONDS(5));

        LOG_INF("Before CBOR request");
        counter_get_cbor(client);
        LOG_INF("After CBOR request");

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
