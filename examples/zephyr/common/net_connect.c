/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_samples, LOG_LEVEL_DBG);

#include <samples/common/net_connect.h>
#include <samples/common/wifi.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>

struct wait_data {
    struct k_sem sem;
    struct net_mgmt_event_callback cb;
};

static void event_cb_handler(
        struct net_mgmt_event_callback* cb,
        uint32_t mgmt_event,
        struct net_if* iface) {
    struct wait_data* wait = CONTAINER_OF(cb, struct wait_data, cb);

    if (mgmt_event == cb->event_mask) {
        k_sem_give(&wait->sem);
    }
}

static void wait_for_iface_up_event(struct net_if* iface) {
    struct wait_data wait;

    wait.cb.handler = event_cb_handler;
    wait.cb.event_mask = NET_EVENT_IF_UP;

    k_sem_init(&wait.sem, 0, 1);
    net_mgmt_add_event_callback(&wait.cb);

    if (!net_if_is_up(iface)) {
        k_sem_take(&wait.sem, K_FOREVER);
    }

    net_mgmt_del_event_callback(&wait.cb);
}

static void wait_for_iface_up_poll(struct net_if* iface) {
    while (!net_if_is_up(iface)) {
        LOG_DBG("Interface is not up yet!");
        k_sleep(K_MSEC(100));
    }
}

static void wait_for_iface_up(struct net_if* iface) {
    if (IS_ENABLED(CONFIG_NET_MGMT_EVENT)) {
        wait_for_iface_up_event(iface);
    } else {
        wait_for_iface_up_poll(iface);
    }
}

static void wait_for_dhcp_bound(struct net_if* iface) {
    (void)net_mgmt_event_wait_on_iface(
            iface, NET_EVENT_IPV4_DHCP_BOUND, NULL, NULL, NULL, K_FOREVER);
}

void net_connect(void) {
    struct net_if* iface = net_if_get_default();

    if (!(IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_WIFI) &&
          net_if_flag_is_set(iface, NET_IF_DORMANT))) {
        LOG_INF("Waiting for interface to be up");
        wait_for_iface_up(iface);
    }

    if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_WIFI)) {
        LOG_INF("Connecting to WiFi");
        wifi_connect(iface);
    }

    if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_DHCP_BIND)) {
        LOG_INF("Starting DHCP to obtain IP address");
        net_dhcpv4_start(iface);
        wait_for_dhcp_bound(iface);
    }
}
