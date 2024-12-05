/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(location_wifi, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/payload_utils.h>
#include <golioth/zcbor_utils.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
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

#include "../src/coap_client.h"

static void send_cb(struct golioth_client *client,
                    enum golioth_status status,
                    const struct golioth_coap_rsp_code *coap_rsp_code,
                    const char *path,
                    const uint8_t *payload,
                    size_t payload_size,
                    void *arg)
{
    ZCBOR_STATE_D(zsd, 1, payload, payload_size, 1, 0);
    double lat;
    double lon;
    int64_t acc;
    struct zcbor_map_entry map_entries[] = {
        ZCBOR_TSTR_LIT_MAP_ENTRY("lat", zcbor_map_float_decode, &lat),
        ZCBOR_TSTR_LIT_MAP_ENTRY("lon", zcbor_map_float_decode, &lon),
        ZCBOR_TSTR_LIT_MAP_ENTRY("acc", zcbor_map_int64_decode, &acc),
    };
    int err;

    if (status != GOLIOTH_OK) {
        LOG_ERR("Error status: %d (%s)", status, golioth_status_to_str(status));
        return;
    }

    err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
    if (err) {
        LOG_ERR("Failed to parse position");
        return;
    }

    LOG_INF("%lf %lf (%lld)", lat, lon, (long long int) acc);
}

struct wifi_visible {
    const char *mac;
    int rssi;
};

struct wifi_scan_results {
    int64_t ts;
    struct wifi_visible *results;
    size_t num_results;
};

static const struct wifi_scan_results scan_results[] = {
#include "location.h"
};

uint8_t buf[4096];

static int send_location(struct wifi_visible *visible, size_t num_visible)
{
    ZCBOR_STATE_E(zse, 1, buf, sizeof(buf), 1);
    bool ok;
    enum golioth_status status;

    ok = zcbor_map_start_encode(zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to %s %s encoding", "start", "map");
        return -ENOMEM;
    }

    ok = zcbor_tstr_put_lit(zse, "wifi");
    if (!ok)
    {
        LOG_ERR("Failed to encode %s %s", "wifi", "key");
        return -ENOMEM;
    }

    ok = zcbor_list_start_encode(zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to %s %s encoding", "start", "list");
        return -ENOMEM;
    }

    for (size_t i = 0; i < num_visible; i++) {
        ok = zcbor_map_start_encode(zse, 1);
        if (!ok)
        {
            LOG_ERR("Failed to %s %s encoding", "start", "map");
            return -ENOMEM;
        }

        ok = zcbor_tstr_put_lit(zse, "mac");
        if (!ok)
        {
            LOG_ERR("Failed to encode %s %s", "mac", "key");
            return -ENOMEM;
        }

        ok = zcbor_tstr_put_term(zse, visible[i].mac, 6 * 2 + 5);
        if (!ok) {
            LOG_ERR("Failed to encode %s %s", "mac", "val");
            return -ENOMEM;
        }

        ok = zcbor_tstr_put_lit(zse, "rss") &&
            zcbor_int32_put(zse, visible[i].rssi);
        if (!ok) {
            LOG_ERR("Failed to encode RSSI");
            return -ENOMEM;
        }

        ok = zcbor_map_end_encode(zse, 1);
        if (!ok) {
            LOG_ERR("Failed to %s %s encoding", "end", "map");
            return -ENOMEM;
        }

        LOG_INF("Encoded successfully %s", visible[i].mac);
    }

    ok = zcbor_list_end_encode(zse, 1);
    if (!ok) {
        LOG_ERR("Failed to %s %s encoding", "end", "list");
        return -ENOMEM;
    }

    ok = zcbor_map_end_encode(zse, 1);
    if (!ok) {
        LOG_ERR("Failed to %s %s encoding", "end", "map");
        return -ENOMEM;
    }

    LOG_INF("CBOR length: %d", (int) (zse->payload - buf));

    status = golioth_coap_client_set(client,
                                     ".l/",
                                     "v1/net",
                                     GOLIOTH_CONTENT_TYPE_CBOR,
                                     buf,
                                     zse->payload - buf,
                                     send_cb,
                                     NULL,
                                     true,
                                     5);

    return 0;
}

int main(void)
{
    int err;

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

    for (size_t i = 0; i < ARRAY_SIZE(scan_results); i++) {
        const struct wifi_scan_results *results = &scan_results[i];

        k_sleep(K_TIMEOUT_ABS_MS(results->ts / 10));

        LOG_INF("sending location at %lld", (long long) results->ts);
        err = send_location(results->results, results->num_results);
        LOG_INF("sent location (%lld): %d", results->ts, err);
    }

    return 0;
}
