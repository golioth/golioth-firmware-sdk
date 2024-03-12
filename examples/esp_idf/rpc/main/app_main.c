/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <golioth/client.h>
#include <golioth/rpc.h>
#include <string.h>

#define TAG "rpc"

static SemaphoreHandle_t connected = NULL;

static enum golioth_rpc_status on_multiply(zcbor_state_t *request_params_array,
                                           zcbor_state_t *response_detail_map,
                                           void *callback_arg)
{
    double a, b;
    double value;
    bool ok;

    ok = zcbor_float_decode(request_params_array, &a)
        && zcbor_float_decode(request_params_array, &b);
    if (!ok)
    {
        ESP_LOGE(TAG, "Failed to decode array items");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    value = a * b;

    ESP_LOGI(TAG, "%lf * %lf = %lf", a, b, value);

    ok = zcbor_tstr_put_lit(response_detail_map, "value")
        && zcbor_float64_put(response_detail_map, value);
    if (!ok)
    {
        ESP_LOGE(TAG, "Failed to encode value");
        return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
    }

    return GOLIOTH_RPC_OK;
}

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        xSemaphoreGive(connected);
    }
    ESP_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Start RPC sample");

    /* Note: In production, you would provision unique credentials onto each
     * device. For this example, it is assumed that WiFi and Golioth
     * PSK credentials are stored in NVS.
     */
    // Initialize NVS first.
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
            ESP_LOGW(TAG, "WiFi and golioth credentials are not set");
            ESP_LOGW(TAG, "Use the shell settings commands to set them, then restart");
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
    struct golioth_client *client = golioth_client_create(&config);
    assert(client);

    connected = xSemaphoreCreateBinary();

    struct golioth_rpc *rpc = golioth_rpc_init(client);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    ESP_LOGW(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(connected, portMAX_DELAY);

    int err = golioth_rpc_register(rpc, "multiply", on_multiply, NULL);

    if (err)
    {
        ESP_LOGE(TAG, "Failed to register RPC: %d", err);
    }

    while (true)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
