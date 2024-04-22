/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include "sample_credentials.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <golioth/client.h>
#include <golioth/fw_update.h>
#include <string.h>

#define TAG "fw_update"

static const char *_current_version = "255.8.9";
static const char *_sdk_version = "0.16.0";

static SemaphoreHandle_t _connected_sem = NULL;

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        xSemaphoreGive(_connected_sem);
    }
    GLTH_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

void app_main(void)
{
    GLTH_LOGI(TAG, "CI Testing fw_update artifact");
    GLTH_LOGI(TAG, "Golioth Firmware SDK version: %s", _sdk_version);

    // Initialize NVS first.
    nvs_init();

    // Create a background shell/CLI task (type "help" to see a list of supported commands)
    shell_start();

    // If the WiFi credentials haven't been set in NVS, we will wait here for the user
    // to input them via the shell.
    if (!nvs_credentials_are_set())
    {
        ESP_LOGW(TAG,
                 "WiFi credentials are not set. "
                 "Use the shell settings commands to set them.");

        while (!nvs_credentials_are_set())
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    const struct golioth_client_config *config = golioth_sample_credentials_get();

    struct golioth_client *client = golioth_client_create(config);
    assert(client);

    _connected_sem = xSemaphoreCreateBinary();

    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    GLTH_LOGW(TAG, "Current FW Version: %s", _current_version);

    golioth_fw_update_init(client, _current_version);

    /* No while(true) loop needed, the Golioth client thread will handle updates */
}
