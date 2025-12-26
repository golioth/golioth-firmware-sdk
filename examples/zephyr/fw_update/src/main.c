/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <app_version.h>

#include <golioth/client.h>

#include <samples/common/sample_credentials.h>
#include <samples/common/net_connect.h>

#include "fw_update.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fw_update_sample, LOG_LEVEL_DBG);

// Current firmware version; update in VERSION
static const char *_current_version =
    STRINGIFY(APP_VERSION_MAJOR) "." STRINGIFY(APP_VERSION_MINOR) "." STRINGIFY(APP_PATCHLEVEL);

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

int main(void)
{
    LOG_DBG("Start FW Update sample");

    net_connect();

    /* Note: In production, credentials should be saved in secure storage. For
     * simplicity, we provide a utility that stores credentials as plaintext
     * settings.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    struct golioth_client *client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    /* Does not return */
    golioth_fw_update_run(client, _current_version);

    /* Unreachable */
    assert(0);
}
