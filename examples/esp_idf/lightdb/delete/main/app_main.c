/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <string.h>

#define TAG "lightdb"

#define APP_TIMEOUT_S 5

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

static void counter_handler(struct golioth_client *client,
                            const struct golioth_response *response,
                            const char *path,
                            void *arg)
{
    if (response->status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG, "Failed to deleted counter: %d", response->status);
        return;
    }

    GLTH_LOGI(TAG, "Counter deleted successfully");

    return;
}

static void counter_delete_async(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_delete_async(client, "counter", counter_handler, NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "failed to delete data from LightDB: %d", err);
    }
}

static void counter_delete_sync(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_delete_sync(client, "counter", APP_TIMEOUT_S);
    if (err)
    {
        GLTH_LOGW(TAG, "failed to delete data from LightDB: %d", err);
    }

    GLTH_LOGI(TAG, "Counter deleted successfully");
}

void app_main(void)
{
    GLTH_LOGI(TAG, "Start LightDB delete sample");

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

    // Now we are ready to connect to the Golioth cloud.
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
    client = golioth_client_create(&config);
    assert(client);

    _connected_sem = xSemaphoreCreateBinary();

    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    while (true)
    {
        GLTH_LOGI(TAG, "Before request (async)");
        counter_delete_async(client);
        GLTH_LOGI(TAG, "After request (async)");

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        GLTH_LOGI(TAG, "Before request (sync)");
        counter_delete_sync(client);
        GLTH_LOGI(TAG, "After request (sync)");

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
