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
#include <golioth/lightdb_state.h>
#include <string.h>

#define TAG "lightdb"

struct golioth_client *client;
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

static void counter_observe_handler(struct golioth_client *client,
                                    enum golioth_status status,
                                    const struct golioth_coap_rsp_code *coap_rsp_code,
                                    const char *path,
                                    const uint8_t *payload,
                                    size_t payload_size,
                                    void *arg)
{
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG,
                  "Failed to receive observed payload: %d (%s)",
                  status,
                  golioth_status_to_str(status));
        return;
    }

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, payload_size, GOLIOTH_DEBUG_LOG_LEVEL_INFO);

    return;
}

void app_main(void)
{
    GLTH_LOGI(TAG, "Start LightDB observe sample");

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
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    const struct golioth_client_config *config = golioth_sample_credentials_get();

    // Now we are ready to connect to the Golioth cloud.

    struct golioth_client *client = golioth_client_create(config);
    assert(client);

    _connected_sem = xSemaphoreCreateBinary();

    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    /* Observe LightDB State Path */
    golioth_lightdb_observe(client,
                            "counter",
                            GOLIOTH_CONTENT_TYPE_JSON,
                            counter_observe_handler,
                            NULL);

    while (true)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
