/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_playback, LOG_LEVEL_DBG);

#define DT_DRV_COMPAT golioth_wifi_playback

#include <zephyr/net/net_offload.h>
#include <zephyr/net/wifi_mgmt.h>

struct wifi_playback_scan_result
{
    uint8_t mac[WIFI_MAC_ADDR_LEN];
    int rssi;
};

struct wifi_playback_scan
{
    int64_t ts;
    const struct wifi_playback_scan_result *results;
    size_t num_results;
};

struct wifi_playback_config
{
    struct net_if *net_iface;
    const struct wifi_playback_scan *scans;
    size_t num_scans;
};

struct wifi_playback_data
{
    const struct wifi_playback_config *config;
    scan_result_cb_t scan_cb;
    struct k_work_delayable scan_work;
    size_t scan_idx;
};

#define SCAN_WORK_QUEUE_STACK_SIZE 2048

K_THREAD_STACK_DEFINE(scan_work_queue_stack_area, SCAN_WORK_QUEUE_STACK_SIZE);

static struct k_work_q scan_work_queue;

static void wifi_playback_scan_handle(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct wifi_playback_data *data = CONTAINER_OF(dwork, struct wifi_playback_data, scan_work);
    const struct wifi_playback_config *config = data->config;
    const struct wifi_playback_scan *scan = &config->scans[data->scan_idx];

    for (size_t i = 0; i < scan->num_results; i++)
    {
        struct wifi_scan_result scan_result = {};

        memcpy(scan_result.mac, scan->results[i].mac, WIFI_MAC_ADDR_LEN);
        scan_result.mac_length = WIFI_MAC_ADDR_LEN;

        scan_result.rssi = scan->results[i].rssi;

        data->scan_cb(config->net_iface, 0, &scan_result);
    }

    data->scan_cb(config->net_iface, 0, NULL);
    data->scan_cb = NULL;

    data->scan_idx++;
}

static int wifi_playback_mgmt_scan(const struct device *dev,
                                   struct wifi_scan_params *params,
                                   scan_result_cb_t cb)
{
    const struct wifi_playback_config *config = dev->config;
    struct net_if *net_iface = config->net_iface;
    struct wifi_playback_data *data = dev->data;
    const struct wifi_playback_scan *scan = &config->scans[data->scan_idx];

    ARG_UNUSED(params);

    if (!net_if_is_carrier_ok(net_iface))
    {
        return -EIO;
    }

    data->scan_cb = cb;

    if (scan->ts == SYS_FOREVER_MS)
    {
        LOG_INF("No more WiFi scans");
        return 0;
    }

    k_work_schedule_for_queue(
        &scan_work_queue,
        &data->scan_work,
        K_TIMEOUT_ABS_MS(scan->ts * 1000 / CONFIG_GOLIOTH_PLAYBACK_SPEED_FACTOR));

    return 0;
}

static int wifi_playback_mgmt_iface_status(const struct device *dev,
                                           struct wifi_iface_status *status)
{
    const struct wifi_playback_config *config = dev->config;
    struct net_if *net_iface = config->net_iface;

    memset(status, 0x0, sizeof(*status));

    status->state = WIFI_STATE_INACTIVE;
    status->band = WIFI_FREQ_BAND_UNKNOWN;
    status->iface_mode = WIFI_MODE_UNKNOWN;
    status->link_mode = WIFI_LINK_MODE_UNKNOWN;
    status->security = WIFI_SECURITY_TYPE_UNKNOWN;
    status->mfp = WIFI_MFP_UNKNOWN;

    if (!net_if_is_carrier_ok(net_iface))
    {
        status->state = WIFI_STATE_INTERFACE_DISABLED;
        return 0;
    }

    return 0;
}

static const struct wifi_mgmt_ops wifi_playback_mgmt_ops = {
    .scan = wifi_playback_mgmt_scan,
    .iface_status = wifi_playback_mgmt_iface_status,
};

static struct net_offload wifi_playback_net_offload;

static void wifi_playback_iface_init(struct net_if *iface)
{
    iface->if_dev->offload = &wifi_playback_net_offload;

    net_if_flag_set(iface, NET_IF_NO_AUTO_START);

    /* Not currently connected to a network */
    net_if_dormant_on(iface);
}

static enum offloaded_net_if_types wifi_playback_offload_get_type(void)
{
    return L2_OFFLOADED_NET_IF_TYPE_WIFI;
}

static const struct net_wifi_mgmt_offload wifi_playback_api = {
    .wifi_iface.iface_api.init = wifi_playback_iface_init,
    .wifi_iface.get_type = wifi_playback_offload_get_type,
    .wifi_mgmt_api = &wifi_playback_mgmt_ops,
};

static int wifi_playback_init(const struct device *dev)
{
    const struct wifi_playback_config *config = dev->config;
    struct wifi_playback_data *data = dev->data;

    data->config = config;
    k_work_init_delayable(&data->scan_work, wifi_playback_scan_handle);

    k_work_queue_init(&scan_work_queue);
    k_work_queue_start(&scan_work_queue,
                       scan_work_queue_stack_area,
                       K_THREAD_STACK_SIZEOF(scan_work_queue_stack_area),
                       K_HIGHEST_THREAD_PRIO,
                       NULL);

    return 0;
}

static const struct wifi_playback_config wifi_playback_config_0;

static struct wifi_playback_data wifi_playback_data_0;

NET_DEVICE_DT_INST_OFFLOAD_DEFINE(0,
                                  wifi_playback_init,
                                  NULL,
                                  &wifi_playback_data_0,
                                  &wifi_playback_config_0,
                                  CONFIG_WIFI_INIT_PRIORITY,
                                  &wifi_playback_api,
                                  NET_ETH_MTU);

#include "wifi_playback_input.c"

static const struct wifi_playback_config wifi_playback_config_0 = {
    .net_iface = NET_IF_GET(Z_DEVICE_DT_DEV_ID(DT_DRV_INST(0)), 0),
    .scans = scan_results,
    .num_scans = ARRAY_SIZE(scan_results),
};
