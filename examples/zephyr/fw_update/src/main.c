/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fw_update_sample, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/fw_update.h>
#include <samples/common/sample_credentials.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <samples/common/net_connect.h>

// Current firmware version; update in prj.conf or via build argument
static const char* _current_version = CONFIG_GOLIOTH_SAMPLE_FW_VERSION;

static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client* client, enum golioth_client_event event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

int main(void) {
    LOG_DBG("Start FW Update sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config* client_config = golioth_sample_credentials_get();

    struct golioth_client* client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_fw_update_init(client, _current_version);

    k_sem_take(&connected, K_FOREVER);

    /* No while(true) loop needed, the Golioth client thread will handle updates */

    return 0;
}
