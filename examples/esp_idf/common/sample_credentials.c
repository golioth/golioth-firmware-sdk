/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <golioth/client.h>
#include "wifi.h"
#include "nvs.h"

#ifndef CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#endif

#define TAG "sample_credentials"

static struct golioth_client_config _golioth_client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
            .psk =
                {
                    .psk_id = NULL,
                    .psk_id_len = 0,
                    .psk = NULL,
                    .psk_len = 0,
                },
        },
};

const struct golioth_client_config *golioth_sample_credentials_get(void)
{
#ifdef CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS

    _golioth_client_config.credentials.psk.psk_id = CONFIG_GOLIOTH_SAMPLE_PSK_ID;
    _golioth_client_config.credentials.psk.psk_id_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK_ID);
    _golioth_client_config.credentials.psk.psk = CONFIG_GOLIOTH_SAMPLE_PSK;
    _golioth_client_config.credentials.psk.psk_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK);

    // Initialize WiFi and wait for it to connect
    printf("calling wifi_init with %s and %s\n", CONFIG_GOLIOTH_SAMPLE_WIFI_SSID, CONFIG_GOLIOTH_SAMPLE_WIFI_PSK);
    wifi_init(CONFIG_GOLIOTH_SAMPLE_WIFI_SSID, CONFIG_GOLIOTH_SAMPLE_WIFI_PSK);
    wifi_wait_for_connected();

#else

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

    const char *psk_id = nvs_read_golioth_psk_id();
    const char *psk = nvs_read_golioth_psk();
    _golioth_client_config.credentials.psk.psk_id = psk_id;
    _golioth_client_config.credentials.psk.psk_id_len = strlen(psk_id);
    _golioth_client_config.credentials.psk.psk = psk;
    _golioth_client_config.credentials.psk.psk_len = strlen(psk);
#endif

    return &_golioth_client_config;
}
