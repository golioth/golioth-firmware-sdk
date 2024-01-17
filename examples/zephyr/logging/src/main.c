/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_logging, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
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

static void func_1(int counter)
{
    LOG_DBG("Log 1: %d", counter);
}

static void func_2(int counter)
{
    LOG_DBG("Log 2: %d", counter);
}

int main(void)
{
    int counter = 0;

    LOG_DBG("start logging sample");

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
        LOG_DBG("Debug info! %d", counter);
        func_1(counter);
        func_2(counter);
        LOG_WRN("Warn: %d", counter);
        LOG_ERR("Err: %d", counter);

        /* Ensure a known endian-ness for the counter value */
        uint32_t counter_be = sys_cpu_to_be32(counter);
        LOG_HEXDUMP_INF(&counter_be, sizeof(counter_be), "Counter hexdump");

        counter++;

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
