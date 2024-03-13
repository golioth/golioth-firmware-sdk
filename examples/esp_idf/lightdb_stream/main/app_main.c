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
#include <golioth/stream.h>
#include <string.h>

#define TAG "lightdb_stream"

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

static float get_temperature()
{
    static int counter = 0;
    float temp = 0;

    /* generate a temperature from 20 deg to 30 deg, with 0.5 deg step */

    temp = 20.0 + (float)counter / 2;

    counter = (counter + 1) % 20;

    return temp;
}

static void temperature_push_handler(struct golioth_client *client,
                                     const struct golioth_response *response,
                                     const char *path,
                                     void *arg)
{
    if (response->status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG, "Failed to push temperature: %d", response->status);
        return;
    }

    GLTH_LOGI(TAG, "Temperature successfully pushed");

    return;
}

static void temperature_push_async(const float temp)
{
    char sbuf[sizeof("{\"temp\":-4294967295.123456}")];
    int err;

    snprintf(sbuf, sizeof(sbuf), "{\"temp\":%f}", temp);

    err = golioth_stream_set_async(client,
                                   "sensor",
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (uint8_t *) sbuf,
                                   strlen(sbuf),
                                   temperature_push_handler,
                                   NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to push temperature: %d", err);
        return;
    }
}

static void temperature_push_sync(const float temp)
{
    char sbuf[sizeof("{\"temp\":-4294967295.123456}")];
    int err;

    snprintf(sbuf, sizeof(sbuf), "{\"temp\":%f}", temp);

    err = golioth_stream_set_sync(client,
                                  "sensor",
                                  GOLIOTH_CONTENT_TYPE_JSON,
                                  (uint8_t *) sbuf,
                                  strlen(sbuf),
                                  1);

    if (err)
    {
        GLTH_LOGW(TAG, "Failed to push temperature: %d", err);
        return;
    }

    GLTH_LOGI(TAG, "Temperature successfully pushed");
}

void app_main(void)
{
    float temp;

    GLTH_LOGI(TAG, "Start LightDB Stream sample");

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
        /* Synchronous using JSON object */
        temp = get_temperature();

        GLTH_LOGI(TAG, "Sending temperature %f", temp);

        temperature_push_sync(temp);

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        /* Callback-based using JSON object */
        temp = get_temperature();

        GLTH_LOGI(TAG, "Sending temperature %f", temp);

        temperature_push_async(temp);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
