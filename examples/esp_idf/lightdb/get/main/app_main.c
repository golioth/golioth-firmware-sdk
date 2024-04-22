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
#include <golioth/payload_utils.h>
#include <golioth/zcbor_utils.h>
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

static void counter_get_handler(struct golioth_client *client,
                                const struct golioth_response *response,
                                const char *path,
                                const uint8_t *payload,
                                size_t payload_size,
                                void *arg)
{
    if ((response->status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        GLTH_LOGW(TAG, "Failed to get counter (async): %d", response->status);
    }
    else
    {
        GLTH_LOGI(TAG, "Counter (async): %" PRId32, golioth_payload_as_int(payload, payload_size));
    }
}

static void counter_get_async(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_get_async(client,
                                    "counter",
                                    GOLIOTH_CONTENT_TYPE_JSON,
                                    counter_get_handler,
                                    NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "failed to get data from LightDB: %d", err);
    }
}

static void counter_get_sync(struct golioth_client *client)
{
    int32_t value;
    int err;

    err = golioth_lightdb_get_int_sync(client, "counter", &value, APP_TIMEOUT_S);
    if (err)
    {
        GLTH_LOGW(TAG, "failed to get data from LightDB: %d", err);
    }
    else
    {
        GLTH_LOGI(TAG, "Counter (sync): %" PRId32, value);
    }
}

static void counter_get_json_sync(struct golioth_client *client)
{
    uint8_t sbuf[128];
    size_t len = sizeof(sbuf);
    int err;

    /* Get root of LightDB State, but JSON can be returned for any path */
    err =
        golioth_lightdb_get_sync(client, "", GOLIOTH_CONTENT_TYPE_JSON, sbuf, &len, APP_TIMEOUT_S);
    if (err || (0 == strlen((char *) sbuf)))
    {
        GLTH_LOGW(TAG, "failed to get JSON data from LightDB: %d", err);
    }
    else
    {
        GLTH_LOG_BUFFER_HEXDUMP(TAG, sbuf, len, GOLIOTH_DEBUG_LOG_LEVEL_INFO);
    }
}

static void counter_get_cbor_handler(struct golioth_client *client,
                                     const struct golioth_response *response,
                                     const char *path,
                                     const uint8_t *payload,
                                     size_t payload_size,
                                     void *arg)
{
    if ((response->status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        GLTH_LOGW(TAG, "Failed to get counter (async): %d", response->status);
        return;
    }

    ZCBOR_STATE_D_COMPAT(zsd, 1, payload, payload_size, 1, 0);
    int64_t counter;
    struct zcbor_map_entry map_entry =
        ZCBOR_TSTR_LIT_MAP_ENTRY("counter", zcbor_map_int64_decode, &counter);

    int err = zcbor_map_decode(zsd, &map_entry, 1);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to decode CBOR data: %d", err);
    }

    GLTH_LOGI(TAG, "Counter (CBOR async): %" PRId64, counter);
}

static void counter_get_cbor_async(struct golioth_client *client)
{
    int err = golioth_lightdb_get_async(client,
                                        "",
                                        GOLIOTH_CONTENT_TYPE_CBOR,
                                        counter_get_cbor_handler,
                                        NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to get data from LightDB: %d", err);
    }
}

void app_main(void)
{
    GLTH_LOGI(TAG, "Start LightDB get sample");

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
        GLTH_LOGI(TAG, "Before request (async)");
        counter_get_async(client);
        GLTH_LOGI(TAG, "After request (async)");

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        GLTH_LOGI(TAG, "Before request (sync)");
        counter_get_sync(client);
        GLTH_LOGI(TAG, "After request (sync)");

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        GLTH_LOGI(TAG, "Before JSON request (sync)");
        counter_get_json_sync(client);
        GLTH_LOGI(TAG, "After JSON request (sync)");

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        GLTH_LOGI(TAG, "Before CBOR request (async)");
        counter_get_cbor_async(client);
        GLTH_LOGI(TAG, "After CBOR request (async)");

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
