/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#if CONFIG_GOLIOTH_BLE_SERVICE_ENABLED
#include "ble.h"
#endif
#include <golioth/client.h>
#include "golioth_basics.h"
#include <string.h>

#define TAG "golioth_example"

void app_main(void)
{
    // Initialize NVS first. For this example, it is assumed that WiFi and Golioth
    // PSK credentials are stored in NVS.
    nvs_init();

#if CONFIG_GOLIOTH_BLE_SERVICE_ENABLED
    // Initialize BLE stack, which can be used for provisioning WiFi and Golioth credentials
    ble_init("golioth_esp32");
#endif

    // Create a background shell/CLI task (type "help" to see a list of supported commands)
    shell_start();

    // If the credentials haven't been set in NVS, we will wait here for the user
    // to input them via the shell.
    if (!nvs_credentials_are_set())
    {
        while (1)
        {
            golioth_sys_msleep(1000);
            ESP_LOGW(TAG, "WiFi and golioth credentials are not set");
            ESP_LOGW(TAG, "Use the shell settings commands to set them, then restart");
            golioth_sys_msleep(UINT32_MAX);
        }
    }

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();


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
    const char *psk_id = nvs_read_golioth_psk_id();
    const char *psk = nvs_read_golioth_psk();

    struct golioth_client_config config = {
        .credentials =
            {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                .psk =
                    {
                        .psk_id = psk_id,
                        .psk_id_len = strlen(psk_id),
                        .psk = psk,
                        .psk_len = strlen(psk),
                    },
            },
    };
    struct golioth_client *client = golioth_client_create(&config);
    assert(client);

    // golioth_basics will interact with each Golioth service and enter an endless loop.
    golioth_basics(client);
}
