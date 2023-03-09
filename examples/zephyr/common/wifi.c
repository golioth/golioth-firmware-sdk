/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_wifi, LOG_LEVEL_DBG);

#include <samples/common/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/settings/settings.h>

struct wifi_data {
    struct net_mgmt_event_callback wifi_mgmt_cb;
    struct k_sem connect_sem;
    int connect_status;
};

static void wifi_mgmt_event_handler(
        struct net_mgmt_event_callback* cb,
        uint32_t mgmt_event,
        struct net_if* iface) {
    struct wifi_data* wifi = CONTAINER_OF(cb, struct wifi_data, wifi_mgmt_cb);
    const struct wifi_status* status = cb->info;

    switch (mgmt_event) {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            LOG_INF("%s with status: %d", "Connected", status->status);
            wifi->connect_status = status->status;
            k_sem_give(&wifi->connect_sem);
            break;
        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            LOG_INF("%s with status: %d", "Disconnected", status->status);
            wifi->connect_status = -EFAULT;
            k_sem_give(&wifi->connect_sem);
            break;
        default:
            break;
    }
}

#if defined(CONFIG_GOLIOTH_SAMPLE_WIFI_SETTINGS)

static uint8_t wifi_ssid[WIFI_SSID_MAX_LEN];
static size_t wifi_ssid_len;
static uint8_t wifi_psk[WIFI_PSK_MAX_LEN];
static size_t wifi_psk_len;

static int wifi_settings_get(const char* name, char* dst, int val_len_max) {
    uint8_t* val;
    size_t val_len;

    if (!strcmp(name, "ssid")) {
        val = wifi_ssid;
        val_len = wifi_ssid_len;
    } else if (!strcmp(name, "psk")) {
        val = wifi_psk;
        val_len = wifi_psk_len;
    } else {
        LOG_WRN("Unsupported key '%s'", name);
        return -ENOENT;
    }

    if (val_len > val_len_max) {
        LOG_ERR("Not enough space (%zu %d)", val_len, val_len_max);
        return -ENOMEM;
    }

    memcpy(dst, val, val_len);

    return val_len;
}

static int wifi_settings_set(
        const char* name,
        size_t len_rd,
        settings_read_cb read_cb,
        void* cb_arg) {
    uint8_t* buffer;
    size_t buffer_len;
    size_t* ret_len;
    ssize_t ret;

    if (!strcmp(name, "ssid")) {
        buffer = wifi_ssid;
        buffer_len = sizeof(wifi_ssid);
        ret_len = &wifi_ssid_len;
    } else if (!strcmp(name, "psk")) {
        buffer = wifi_psk;
        buffer_len = sizeof(wifi_psk);
        ret_len = &wifi_psk_len;
    } else {
        LOG_WRN("Unsupported key '%s'", name);
        return -ENOENT;
    }

    ret = read_cb(cb_arg, buffer, buffer_len);
    if (ret < 0) {
        LOG_ERR("Failed to read value: %d", (int)ret);
        return ret;
    }

    *ret_len = ret;

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(
        wifi,
        "wifi",
        IS_ENABLED(CONFIG_SETTINGS_RUNTIME) ? wifi_settings_get : NULL,
        wifi_settings_set,
        NULL,
        NULL);

#else /* defined(CONFIG_GOLIOTH_SAMPLE_WIFI_SETTINGS) */

static uint8_t wifi_ssid[] = CONFIG_GOLIOTH_SAMPLE_WIFI_SSID;
static size_t wifi_ssid_len = sizeof(CONFIG_GOLIOTH_SAMPLE_WIFI_SSID) - 1;
static uint8_t wifi_psk[] = CONFIG_GOLIOTH_SAMPLE_WIFI_PSK;
static size_t wifi_psk_len = sizeof(CONFIG_GOLIOTH_SAMPLE_WIFI_PSK) - 1;

#endif /* defined(CONFIG_GOLIOTH_SAMPLE_WIFI_SETTINGS) */

void wifi_connect(struct net_if* iface) {
    struct wifi_connect_req_params params = {
            .ssid = wifi_ssid,
            .ssid_length = wifi_ssid_len,
            .psk = wifi_psk,
            .psk_length = wifi_psk_len,
            .channel = WIFI_CHANNEL_ANY,
            .security = wifi_psk_len > 0 ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE,
    };
    struct wifi_data wifi;
    int err;
    int attempts = 10;
    int timeout_msec = 500;

    k_sem_init(&wifi.connect_sem, 0, 1);

    net_mgmt_init_event_callback(
            &wifi.wifi_mgmt_cb,
            wifi_mgmt_event_handler,
            (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT));
    net_mgmt_add_event_callback(&wifi.wifi_mgmt_cb);

    while (attempts--) {
        err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
        if (err == -EALREADY) {
            LOG_INF("Already connected to WiFi");
            break;
        } else if (err) {
            LOG_ERR("Failed to request WiFi connect: %d", err);
            goto retry;
        }

        /* wait for notification from connect request */
        err = k_sem_take(&wifi.connect_sem, K_SECONDS(20));
        if (err) {
            LOG_ERR("Timeout waiting for connection with WiFi");
            goto retry;
        }

        /* verify connect status */
        if (wifi.connect_status == 0) {
            LOG_INF("Successfully connected to WiFi");
            break;
        } else {
            LOG_ERR("Failed to connect to WiFi: %d", wifi.connect_status);
            goto retry;
        }

    retry:
        k_sleep(K_MSEC(timeout_msec));

        /* Exponential backoff, but max 30s */
        timeout_msec = MIN(timeout_msec * 2, 30 * 1000);
    }

    net_mgmt_del_event_callback(&wifi.wifi_mgmt_cb);
}
