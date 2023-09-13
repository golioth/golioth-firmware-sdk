/*
 * copyright (c) 2023 golioth, inc.
 *
 * spdx-license-identifier: apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hello_zephyr, LOG_LEVEL_DBG);

#include "golioth.h"
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

golioth_client_t client;
static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(golioth_client_t client, golioth_client_event_t event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

int main(void) {
    int counter = 0;

    LOG_DBG("start hello sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const golioth_client_config_t* client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    while (true) {
        LOG_INF("Sending hello! %d", counter);

        ++counter;
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
