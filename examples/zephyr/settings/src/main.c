/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(device_settings, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/settings.h>
#include <samples/common/sample_credentials.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

// Define min and max acceptable values for setting
#define LOOP_DELAY_S_MIN 1
#define LOOP_DELAY_S_MAX 300

// Configurable via Settings service, key = "LOOP_DELAY_S"
int32_t _loop_delay_s = 10;

struct golioth_client* client;
static K_SEM_DEFINE(connected, 0, 1);

static golioth_settings_status_t on_loop_delay_setting(int32_t new_value, void* arg) {
    LOG_INF("Setting loop delay to %" PRId32 " s", new_value);
    _loop_delay_s = new_value;
    return GOLIOTH_SETTINGS_SUCCESS;
}

static void on_client_event(struct golioth_client* client, enum golioth_client_event event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

int main(void) {
    int counter = 0;

    LOG_DBG("Start Golioth Device Settings sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const golioth_client_config_t* client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_settings_register_int_with_range(client,
                                             "LOOP_DELAY_S",
                                             LOOP_DELAY_S_MIN,
                                             LOOP_DELAY_S_MAX,
                                             on_loop_delay_setting,
                                             NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true) {
        LOG_INF("Sending hello! %d", counter);

        ++counter;
        k_sleep(K_SECONDS(_loop_delay_s));
    }

    return 0;
}
