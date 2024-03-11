#include <stdbool.h>
#include <stdint.h>
#include <string>
#include "nvs.h"
#include "wifi.h"
#include "shell.h"

extern "C" {

#include "freertos/FreeRTOS.h"
#include <golioth/client.h>
#include <golioth/lightdb_state.h>

}

#define TAG "app_main"

extern "C" void app_main(void) {
    nvs_init();
    shell_start();

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

    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    const std::string psk_id = nvs_read_golioth_psk_id();
    const std::string psk = nvs_read_golioth_psk();

    struct golioth_client_config config = {};
    config.credentials.auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK;
    config.credentials.psk.psk_id = psk_id.c_str();
    config.credentials.psk.psk_id_len = psk_id.length();
    config.credentials.psk.psk = psk.c_str();
    config.credentials.psk.psk_len = psk.length();

    struct golioth_client* client = golioth_client_create(&config);

    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Hello, Golioth %d!", counter);
        golioth_lightdb_set_int_async(client, "counter", counter, NULL, NULL);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        counter++;
    }
}
