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
                                enum golioth_status status,
                                const struct golioth_coap_rsp_code *coap_rsp_code,
                                const char *path,
                                const uint8_t *payload,
                                size_t payload_size,
                                void *arg)
{
    if ((status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        GLTH_LOGW(TAG, "Failed to get counter: %d (%s)", status, golioth_status_to_str(status));
    }
    else
    {
        GLTH_LOGI(TAG, "Counter: %" PRId32, golioth_payload_as_int(payload, payload_size));
    }
}

static void counter_get(struct golioth_client *client)
{
    int err;

    err = golioth_lightdb_get(client,
                              "counter",
                              GOLIOTH_CONTENT_TYPE_JSON,
                              counter_get_handler,
                              NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "failed to get data from LightDB: %d (%s)", err, golioth_status_to_str(err));
    }
}

static void counter_get_json_handler(struct golioth_client *client,
                                     enum golioth_status status,
                                     const struct golioth_coap_rsp_code *coap_rsp_code,
                                     const char *path,
                                     const uint8_t *payload,
                                     size_t payload_size,
                                     void *arg)
{
    if ((GOLIOTH_OK != status) || golioth_payload_is_null(payload, payload_size))
    {
        GLTH_LOGE(TAG,
                  "Error fetching LightDB JSON: %d (%s)",
                  status,
                  golioth_status_to_str(status));
    }
    else
    {
        char sbuf[128];
        snprintf(sbuf, sizeof(sbuf), "LightDB JSON: %.*s", payload_size, payload);
        GLTH_LOGI(TAG, "LightDB JSON: %s", payload);
    }
}

static void counter_get_json(struct golioth_client *client)
{
    /* Get root of LightDB State, but JSON can be returned for any path */
    int err =
        golioth_lightdb_get(client, "", GOLIOTH_CONTENT_TYPE_JSON, counter_get_json_handler, NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "failed to get JSON data from LightDB: %d", err);
    }
}

static void counter_get_cbor_handler(struct golioth_client *client,
                                     enum golioth_status status,
                                     const struct golioth_coap_rsp_code *coap_rsp_code,
                                     const char *path,
                                     const uint8_t *payload,
                                     size_t payload_size,
                                     void *arg)
{
    if ((status != GOLIOTH_OK) || golioth_payload_is_null(payload, payload_size))
    {
        GLTH_LOGW(TAG, "Failed to get counter: %d (%s)", status, golioth_status_to_str(status));
        return;
    }

    ZCBOR_STATE_D(zsd, 1, payload, payload_size, 1, 0);
    int64_t counter;
    struct zcbor_map_entry map_entry =
        ZCBOR_TSTR_LIT_MAP_ENTRY("counter", zcbor_map_int64_decode, &counter);

    int err = zcbor_map_decode(zsd, &map_entry, 1);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to decode CBOR data: %d", err);
    }

    GLTH_LOGI(TAG, "Counter (CBOR): %" PRId64, counter);
}

static void counter_get_cbor(struct golioth_client *client)
{
    int err =
        golioth_lightdb_get(client, "", GOLIOTH_CONTENT_TYPE_CBOR, counter_get_cbor_handler, NULL);
    if (err)
    {
        GLTH_LOGW(TAG, "Failed to get data from LightDB: %d (%s)", err, golioth_status_to_str(err));
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
        GLTH_LOGI(TAG, "Before request");
        counter_get(client);
        GLTH_LOGI(TAG, "After request");

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        GLTH_LOGI(TAG, "Before JSON request");
        counter_get_json(client);
        GLTH_LOGI(TAG, "After JSON request");

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        GLTH_LOGI(TAG, "Before CBOR request");
        counter_get_cbor(client);
        GLTH_LOGI(TAG, "After CBOR request");

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
