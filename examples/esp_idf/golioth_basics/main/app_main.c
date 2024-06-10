/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include "sample_credentials.h"
#include <golioth/client.h>
#include "golioth_basics.h"
#include <string.h>

#define TAG "golioth_example"

void app_main(void)
{
    // Initialize NVS first. For this example, it is assumed that WiFi and Golioth
    // PSK credentials are stored in NVS.
    nvs_init();

    // Create a background shell/CLI task (type "help" to see a list of supported commands)
    shell_start();

    // If the credentials haven't been set in NVS, we will wait here for the user
    // to input them via the shell.
    if (!nvs_credentials_are_set())
    {
        ESP_LOGW(TAG,
                 "WiFi and Golioth credentials are not set. "
                 "Use the shell settings commands to set them.");

        while (!nvs_credentials_are_set())
        {
            golioth_sys_msleep(1000);
        }
    }

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    const struct golioth_client_config *config = golioth_sample_credentials_get();

    // Now we are ready to connect to the Golioth cloud.
    //
    // To start, we need to create a client. The function golioth_client_create will
    // dynamically create a client and return a handle to it.
    //
    // The client itself runs in a separate task, so once this function returns,
    // there will be a new task running in the background.
    //
    // As soon as the task starts, it will try to connect to Golioth using the
    // CoAP protocol over DTLS, with the PSK ID and PSK for authentication.

    struct golioth_client *client = golioth_client_create(config);
    assert(client);

    // golioth_basics will interact with each Golioth service and enter an endless loop.
    golioth_basics(client);
}
