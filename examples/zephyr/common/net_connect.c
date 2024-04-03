/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_samples, LOG_LEVEL_DBG);

#include <samples/common/net_connect.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>

struct wait_data
{
    struct k_sem sem;
    struct net_mgmt_event_callback cb;
};

static void event_cb_handler(struct net_mgmt_event_callback *cb,
                             uint32_t mgmt_event,
                             struct net_if *iface)
{
    struct wait_data *wait = CONTAINER_OF(cb, struct wait_data, cb);

    if (mgmt_event == cb->event_mask)
    {
        k_sem_give(&wait->sem);
    }
}

static void wait_for_net_event(struct net_if *iface, uint32_t event)
{
    struct wait_data wait;

    wait.cb.handler = event_cb_handler;
    wait.cb.event_mask = event;

    k_sem_init(&wait.sem, 0, 1);
    net_mgmt_add_event_callback(&wait.cb);

    k_sem_take(&wait.sem, K_FOREVER);

    net_mgmt_del_event_callback(&wait.cb);
}

void net_connect(void)
{
    struct net_if *iface = net_if_get_default();

    if (!net_if_is_up(iface))
    {
        LOG_INF("Bringing up network interface");
        int ret = net_if_up(iface);
        if ((ret < 0) && (ret != -EALREADY))
        {
            LOG_ERR("Failed to bring up network interface: %d", ret);
            return;
        }
    }

    if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_DHCP_BIND))
    {
        LOG_INF("Starting DHCP to obtain IP address");
        net_dhcpv4_start(iface);
    }

    LOG_INF("Waiting to obtain IP address");
    wait_for_net_event(iface, NET_EVENT_IPV4_ADDR_ADD);
}
