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
#include <golioth/settings.h>
#include <string.h>

#define TAG "settings"

struct golioth_client *client;
static SemaphoreHandle_t _connected_sem = NULL;

// Define min and max acceptable values for setting
#define LOOP_DELAY_S_MIN 1
#define LOOP_DELAY_S_MAX 300

// Configurable via Settings service, key = "LOOP_DELAY_S"
int32_t _loop_delay_s = 10;

static enum golioth_settings_status on_loop_delay_setting(int32_t new_value, void *arg)
{
    GLTH_LOGI(TAG, "Setting loop delay to %" PRId32 " s", new_value);
    _loop_delay_s = new_value;
    return GOLIOTH_SETTINGS_SUCCESS;
}

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
    int counter = 0;

    GLTH_LOGI(TAG, "Start Golioth Device Settings sample");

    // Initialize NVS first. For this example, it is assumed that WiFi and Golioth
    // PSK credentials are stored in NVS.
    nvs_init();

    // Create a background shell/CLI task (type "help" to see a list of supported commands)
    shell_start();

    // If the credentials haven't been set in NVS, we will wait here for the user
    // to input them via the shell.
    if (!nvs_credentials_are_set())
    {
        while (1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            GLTH_LOGW(TAG, "WiFi and golioth credentials are not set");
            GLTH_LOGW(TAG, "Use the shell settings commands to set them, then restart");
            vTaskDelay(UINT32_MAX);
        }
    }

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    const struct golioth_client_config *config = golioth_sample_credentials_get();

    // Now we are ready to connect to the Golioth cloud.

    client = golioth_client_create(config);
    assert(client);

    _connected_sem = xSemaphoreCreateBinary();

    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    struct golioth_settings *settings = golioth_settings_init(client);

    golioth_settings_register_int_with_range(settings,
                                             "LOOP_DELAY_S",
                                             LOOP_DELAY_S_MIN,
                                             LOOP_DELAY_S_MAX,
                                             on_loop_delay_setting,
                                             NULL);

    while (true)
    {
        GLTH_LOGI(TAG, "Sending hello! %d", counter);

        ++counter;
        vTaskDelay(_loop_delay_s * 1000 / portTICK_PERIOD_MS);
    }
}
