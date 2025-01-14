/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(location_main, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/location/wifi.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/net/wifi_mgmt.h>

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

static enum golioth_status golioth_location_wifi_append_zephyr(
    struct golioth_location_req *req,
    const struct wifi_scan_result *result)
{
    struct golioth_wifi_scan_result golioth_result = {
        .mac = {result->mac[0],
                result->mac[1],
                result->mac[2],
                result->mac[3],
                result->mac[4],
                result->mac[5]},
        .rssi = result->rssi,
    };

    return golioth_location_wifi_append(req, &golioth_result);
}

static struct golioth_location_req location_req;

static void wifi_scan_result_process(const struct wifi_scan_result *entry)
{
    if (entry->mac_length != WIFI_MAC_ADDR_LEN)
    {
        return;
    }

    golioth_location_wifi_append_zephyr(&location_req, entry);
}

static K_SEM_DEFINE(scan_sem, 0, 1);

static void wifi_scan_done_process(void)
{
    k_sem_give(&scan_sem);
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint32_t mgmt_event,
                                    struct net_if *iface)
{
    switch (mgmt_event)
    {
        case NET_EVENT_WIFI_SCAN_RESULT:
            wifi_scan_result_process(cb->info);
            break;
        case NET_EVENT_WIFI_SCAN_DONE:
            wifi_scan_done_process();
            break;
        default:
            break;
    }
}

static int wifi_scan_and_encode_info(struct net_if *iface)
{
    int err;

    err = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
    if (err)
    {
        LOG_ERR("Failed to scan: %d", err);
        return err;
    }

    k_sem_take(&scan_sem, K_FOREVER);

    return 0;
}

static struct net_mgmt_event_callback wifi_mgmt_cb;

int main(void)
{
    struct net_if *wifi_iface = net_if_get_first_wifi();
    struct golioth_location_rsp location_rsp;
    enum golioth_status status;
    int err;

    LOG_DBG("Start location sample");

    net_mgmt_init_event_callback(&wifi_mgmt_cb,
                                 wifi_mgmt_event_handler,
                                 (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE));
    net_mgmt_add_event_callback(&wifi_mgmt_cb);

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
        golioth_location_init(&location_req);

        err = wifi_scan_and_encode_info(wifi_iface);
        if (err)
        {
            continue;
        }

        status = golioth_location_finish(&location_req);
        if (status != GOLIOTH_OK)
        {
            if (status == GOLIOTH_ERR_NULL)
            {
                LOG_WRN("No location data to be provided");
                return 0;
            }

            LOG_ERR("Failed to encode location data");
            return -ENOMEM;
        }

        status = golioth_location_get_sync(client, &location_req, &location_rsp, APP_TIMEOUT_S);
        if (status == GOLIOTH_OK)
        {
            LOG_INF("%s%lld.%09lld %s%lld.%09lld (%lld)",
                    location_rsp.latitude < 0 ? "-" : "",
                    llabs(location_rsp.latitude) / 1000000000,
                    llabs(location_rsp.latitude) % 1000000000,
                    location_rsp.longitude < 0 ? "-" : "",
                    llabs(location_rsp.longitude) / 1000000000,
                    llabs(location_rsp.longitude) % 1000000000,
                    (long long int) location_rsp.accuracy);
        }

        k_msleep(CONFIG_APP_LOCATION_GET_INTERVAL);
    }

    return 0;
}
