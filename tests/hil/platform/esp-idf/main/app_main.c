/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include <golioth/client.h>
#include <golioth/golioth_sys.h>
#include <string.h>

#define TAG "hil_test"

void hil_test_entry(const struct golioth_client_config *config);

void app_main(void) {
    // Initialize NVS first.
    nvs_init();

    // Create a background shell/CLI task
    shell_start();

    // If the credentials haven't been set in NVS, we will wait here for the user
    // to input them via the shell.
    if (!nvs_credentials_are_set()) {
        while (1) {
            golioth_sys_msleep(1000);
            ESP_LOGW(TAG, "WiFi and golioth credentials are not set");
            ESP_LOGW(TAG, "Use the shell settings commands to set them, then restart");
            golioth_sys_msleep(UINT32_MAX);
        }
    }

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();


    // Read Golioth credentials
    const char* psk_id = nvs_read_golioth_psk_id();
    const char* psk = nvs_read_golioth_psk();

    struct golioth_client_config config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                    .psk = {
                            .psk_id = psk_id,
                            .psk_id_len = strlen(psk_id),
                            .psk = psk,
                            .psk_len = strlen(psk),
                    }}};

    // Run HIL test
    hil_test_entry(&config);
}
