/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include <golioth/client.h>
#include <golioth/golioth_sys.h>

#define TAG "certificate_auth"

extern const uint8_t ca_pem_start[] asm("_binary_isrgrootx1_der_start");
extern const uint8_t ca_pem_end[] asm("_binary_isrgrootx1_der_end");
extern const uint8_t client_pem_start[] asm("_binary_client_crt_der_start");
extern const uint8_t client_pem_end[] asm("_binary_client_crt_der_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_der_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_der_end");

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    ESP_LOGI(TAG,
             "Golioth client %s",
             event == GOLIOTH_CLIENT_EVENT_CONNECTED ? "connected" : "disconnected");
}

void app_main(void)
{
    // Initialize NVS first. For this example, it is assumed that WiFi
    // credentials are stored in NVS.
    nvs_init();

    // Create a background shell/CLI task (type "help" to see a list of supported commands)
    shell_start();

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    size_t ca_pem_len = ca_pem_end - ca_pem_start;
    size_t client_pem_len = client_pem_end - client_pem_start;
    size_t client_key_len = client_key_end - client_key_start;

    struct golioth_client_config config = {
        .credentials =
            {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
                .pki =
                    {
                        .ca_cert = ca_pem_start,
                        .ca_cert_len = ca_pem_len,
                        .public_cert = client_pem_start,
                        .public_cert_len = client_pem_len,
                        .private_key = client_key_start,
                        .private_key_len = client_key_len,
                    },
            },
    };
    struct golioth_client *client = golioth_client_create(&config);
    assert(client);

    // Register a callback function that will be called by the client task when
    // connect and disconnect events happen.
    //
    // This is optional, but can be useful for synchronizing operations on connect/disconnect
    // events. For this example, the on_client_event callback will simply log a message.
    golioth_client_register_event_callback(client, on_client_event, NULL);

    int counter = 0;
    while (1)
    {
        GLTH_LOGI(TAG, "Counter = %d", counter);
        counter++;
        golioth_sys_msleep(5000);
    };
}
