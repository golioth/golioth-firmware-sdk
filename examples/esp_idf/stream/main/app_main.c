/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include "sample_credentials.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <golioth/client.h>
#include <golioth/stream.h>

#define TAG "stream"

#define SYNC_TIMEOUT_S 2

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

    temp = 20.0 + (float) counter / 2;

    counter = (counter + 1) % 20;

    return temp;
}

static void temperature_push_handler(struct golioth_client *client,
                                     enum golioth_status status,
                                     const struct golioth_coap_rsp_code *coap_rsp_code,
                                     const char *path,
                                     void *arg)
{
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG,
                  "Failed to push temperature: %d (%s)",
                  status,
                  golioth_status_to_str(status));
        return;
    }

    GLTH_LOGI(TAG, "Temperature successfully pushed");

    return;
}

static void temperature_push_cbor(struct golioth_client *client, float temp, bool async)
{
    uint8_t buf[15];
    int err;

    /* Create zcbor state variable for encoding */
    ZCBOR_STATE_E(zse, 1, buf, sizeof(buf), 1);

    /* Encode the CBOR map header */
    bool ok = zcbor_map_start_encode(zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to start encoding CBOR map");
        return;
    }

    /* Encode the map key "temp" into buf as a text string */
    ok = zcbor_tstr_put_lit(zse, "temp");
    if (!ok)
    {
        GLTH_LOGE(TAG, "CBOR: Failed to encode temp name");
        return;
    }

    /* Encode the temperature value into buf as a float */
    ok = zcbor_float32_put(zse, temp);
    if (!ok)
    {
        GLTH_LOGE(TAG, "CBOR: failed to encode temp value");
        return;
    }

    /* Close the CBOR map */
    ok = zcbor_map_end_encode(zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to close CBOR map");
        return;
    }

    size_t payload_size = (intptr_t) zse->payload - (intptr_t) buf;

    /* Data is sent to Pipelines on the "/data" path */
    if (async)
    {
        err = golioth_stream_set_async(client,
                                       "data",
                                       GOLIOTH_CONTENT_TYPE_CBOR,
                                       buf,
                                       payload_size,
                                       temperature_push_handler,
                                       NULL);
    }
    else
    {
        err = golioth_stream_set_sync(client,
                                      "data",
                                      GOLIOTH_CONTENT_TYPE_CBOR,
                                      buf,
                                      payload_size,
                                      SYNC_TIMEOUT_S);
    }

    if (err)
    {
        GLTH_LOGW(TAG, "Failed to push temperature: %d (%s)", err, golioth_status_to_str(err));
        return;
    }

    if (!async)
    {
        GLTH_LOGI(TAG, "Temperature successfully pushed");
    }
}

void app_main(void)
{
    float temp;

    GLTH_LOGI(TAG, "Start Stream sample");

    // Initialize NVS first. If credentials are not hardcoded in sdkconfig.defaults,
    // it is assumed that WiFi and Golioth PSK credentials are stored in NVS.
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

    struct golioth_client *client = golioth_client_create(config);
    assert(client);

    _connected_sem = xSemaphoreCreateBinary();

    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    while (true)
    {
        /* Synchronous using CBOR object */
        temp = get_temperature();

        GLTH_LOGI(TAG, "Sending temperature %f", temp);

        temperature_push_cbor(client, temp, false);

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        /* Callback-based using CBOR object */
        temp = get_temperature();

        GLTH_LOGI(TAG, "Sending temperature %f", temp);

        temperature_push_cbor(client, temp, true);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
