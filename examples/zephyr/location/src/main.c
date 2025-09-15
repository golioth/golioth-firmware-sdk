/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(location_main, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <golioth/net_info/cellular.h>
#include <golioth/net_info/wifi.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/net/wifi_mgmt.h>

#include <samples/common/net_connect.h>

#include "cellular.h"

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

static enum golioth_status golioth_net_info_wifi_append_zephyr(
    struct golioth_net_info *info,
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

    return golioth_net_info_wifi_append(info, &golioth_result);
}

static struct golioth_net_info *info;

static int cellular_get_and_encode_info(void)
{
    struct golioth_cellular_info cellular_infos[4];
    enum golioth_status status;
    size_t num_infos;
    int err;

    if (!IS_ENABLED(CONFIG_GOLIOTH_NET_INFO_CELLULAR))
    {
        return 0;
    }

    err = cellular_info_get(cellular_infos, ARRAY_SIZE(cellular_infos), &num_infos);
    if (err)
    {
        LOG_ERR("Failed to get cellular network info: %d", err);
        return err;
    }

    for (size_t i = 0; i < num_infos; i++)
    {
        status = golioth_net_info_cellular_append(info, &cellular_infos[i]);
        if (status != GOLIOTH_OK)
        {
            LOG_ERR("Failed to append cellular info: %d", status);
            return -ENOMEM;
        }
    }

    return 0;
}

static void wifi_scan_result_process(const struct wifi_scan_result *entry)
{
    if (entry->mac_length != WIFI_MAC_ADDR_LEN)
    {
        return;
    }

    golioth_net_info_wifi_append_zephyr(info, entry);
}

static K_SEM_DEFINE(scan_sem, 0, 1);

static void wifi_scan_done_process(void)
{
    k_sem_give(&scan_sem);
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event,
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

    if (!IS_ENABLED(CONFIG_GOLIOTH_NET_INFO_WIFI))
    {
        return 0;
    }

    err = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
    if (err)
    {
        LOG_ERR("Failed to scan: %d", err);
        return err;
    }

    k_sem_take(&scan_sem, K_FOREVER);

    return 0;
}

struct block_upload_source
{
    const uint8_t *buf;
    size_t len;
};

static enum golioth_status block_upload_read_chunk(uint32_t block_idx,
                                                   uint8_t *block_buffer,
                                                   size_t *block_size,
                                                   bool *is_last,
                                                   void *arg)
{
    size_t bu_max_block_size = *block_size;
    const struct block_upload_source *bu_source = arg;
    size_t bu_offset = block_idx * bu_max_block_size;
    size_t bu_size = bu_source->len - bu_offset;

    if (bu_offset >= bu_source->len)
    {
        GLTH_LOGE(TAG, "Calculated offset is past end of buffer: %zu", bu_offset);
        goto bu_error;
    }

    if (bu_size <= bu_max_block_size)
    {
        *block_size = bu_size;
        *is_last = true;
    }

    GLTH_LOGI(TAG,
              "block-idx: %" PRIu32 " bu_offset: %zu bytes_remaining: %zu",
              block_idx,
              bu_offset,
              bu_size);

    memcpy(block_buffer, bu_source->buf + bu_offset, *block_size);
    return GOLIOTH_OK;

bu_error:
    *block_size = 0;
    *is_last = true;

    return GOLIOTH_ERR_NO_MORE_DATA;
}

static struct net_mgmt_event_callback wifi_mgmt_cb;

int main(void)
{
    struct net_if *wifi_iface = net_if_get_first_wifi();
    enum golioth_status status;
    int err;

    LOG_DBG("Start location sample");

    if (IS_ENABLED(CONFIG_WIFI))
    {
        net_mgmt_init_event_callback(&wifi_mgmt_cb,
                                     wifi_mgmt_event_handler,
                                     (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE));
        net_mgmt_add_event_callback(&wifi_mgmt_cb);
    }

    net_connect();

    /* Note: In production, credentials should be saved in secure storage. For
     * simplicity, we provide a utility that stores credentials as plaintext
     * settings.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    info = golioth_net_info_create();

    while (true)
    {
        err = cellular_get_and_encode_info();
        if (err)
        {
            LOG_ERR("Failed to encode cellular network information data: %d", err);
            continue;
        }

        err = wifi_scan_and_encode_info(wifi_iface);
        if (err)
        {
            LOG_ERR("Failed to encode wifi network information data: %d", err);
            continue;
        }

        status = golioth_net_info_finish(info);
        if (status != GOLIOTH_OK)
        {
            if (status == GOLIOTH_ERR_NULL)
            {
                LOG_WRN("No network information data provided");
                return 0;
            }

            LOG_ERR("Failed to encode network information data");
            return -ENOMEM;
        }

        /* Always use blockwise upload to ensure large payloads are delivered successfully */
        struct block_upload_source bu_source = {.buf = golioth_net_info_get_buf(info),
                                                .len = golioth_net_info_get_buf_len(info)};

        int err = golioth_stream_set_blockwise_sync(client,
                                                    "loc/net",
                                                    GOLIOTH_CONTENT_TYPE_CBOR,
                                                    block_upload_read_chunk,
                                                    (void *) &bu_source);
        if (err != GOLIOTH_OK)
        {
            LOG_ERR("Failed to stream network data: %d", err);
        }
        else
        {
            LOG_INF("Successfully streamed network data");
        }

        k_msleep(CONFIG_APP_LOCATION_GET_INTERVAL);
    }

    return 0;
}
