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
#include <zcbor_encode.h>

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

static void counter_set_handler(struct golioth_client *client,
                                enum golioth_status status,
                                const struct golioth_coap_rsp_code *coap_rsp_code,
                                const char *path,
                                void *arg)
{
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG, "Failed to set counter: %d (%s)", status, golioth_status_to_str(status));
        return;
    }

    GLTH_LOGI(TAG, "Counter successfully set");

    return;
}

static void counter_set_async(int counter)
{
    int err;

    err = golioth_lightdb_set_int_async(client, "counter", counter, counter_set_handler, NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to set counter: %d (%s)", err, golioth_status_to_str(err));
        return;
    }
}

static void counter_set_sync(int counter)
{
    int err;

    err = golioth_lightdb_set_int_sync(client, "counter", counter, APP_TIMEOUT_S);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to set counter: %d (%s)", err, golioth_status_to_str(err));
        return;
    }

    GLTH_LOGI(TAG, "Counter successfully set");
}

static void counter_set_json_async(int counter)
{
    char sbuf[sizeof("{\"counter\":4294967295}")];
    int err;

    snprintf(sbuf, sizeof(sbuf), "{\"counter\":%d}", counter);

    err = golioth_lightdb_set_async(client,
                                    "",
                                    GOLIOTH_CONTENT_TYPE_JSON,
                                    (const uint8_t *) sbuf,
                                    strlen(sbuf),
                                    counter_set_handler,
                                    NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to set counter: %d (%s)", err, golioth_status_to_str(err));
        return;
    }
}

static void counter_set_cbor_sync(int counter)
{
    uint8_t buf[32];
    ZCBOR_STATE_E(zse, 1, buf, sizeof(buf), 1);

    bool ok = zcbor_map_start_encode(zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to start CBOR encoding");
        return;
    }

    ok = zcbor_tstr_put_lit(zse, "counter");
    if (!ok)
    {
        GLTH_LOGE(TAG, "CBOR: Failed to encode counter name");
        return;
    }

    ok = zcbor_int32_put(zse, counter);
    if (!ok)
    {
        GLTH_LOGE(TAG, "CBOR: failed to encode counter value");
        return;
    }

    ok = zcbor_map_end_encode(zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to close CBOR map object");
        return;
    }

    size_t payload_size = (intptr_t) zse->payload - (intptr_t) buf;

    int err = golioth_lightdb_set_sync(client,
                                       "",
                                       GOLIOTH_CONTENT_TYPE_CBOR,
                                       buf,
                                       payload_size,
                                       APP_TIMEOUT_S);
    if (err != 0)
    {
        GLTH_LOGW(TAG, "Failed to set counter: %d (%s)", err, golioth_status_to_str(err));
    }
    else
    {
        GLTH_LOGI(TAG, "Counter successfully set");
    }
}

void app_main(void)
{
    int counter = 0;

    GLTH_LOGI(TAG, "Start LightDB set sample");

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

    client = golioth_client_create(config);
    assert(client);

    _connected_sem = xSemaphoreCreateBinary();

    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(_connected_sem, portMAX_DELAY);

    while (true)
    {
        /* Callback-based using int */
        GLTH_LOGI(TAG, "Setting counter to %d", counter);

        GLTH_LOGI(TAG, "Before request (async)");
        counter_set_async(counter);
        GLTH_LOGI(TAG, "After request (async)");

        counter++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        /* Synchrnous using int */
        GLTH_LOGI(TAG, "Setting counter to %d", counter);

        GLTH_LOGI(TAG, "Before request (sync)");
        counter_set_sync(counter);
        GLTH_LOGI(TAG, "After request (sync)");

        counter++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        /* Callback-based using JSON object */
        GLTH_LOGI(TAG, "Setting counter to %d", counter);

        GLTH_LOGI(TAG, "Before request (json async)");
        counter_set_json_async(counter);
        GLTH_LOGI(TAG, "After request (json async)");

        counter++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        /* Synchronous using CBOR object */
        GLTH_LOGI(TAG, "Setting counter to %d", counter);

        GLTH_LOGI(TAG, "Before request (cbor sync)");
        counter_set_cbor_sync(counter);
        GLTH_LOGI(TAG, "After request (cbor sync)");

        counter++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
