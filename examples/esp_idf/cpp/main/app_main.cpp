#include <stdbool.h>
#include <stdint.h>
#include <string>
#include "nvs.h"
#include "wifi.h"
#include "shell.h"

extern "C" {

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <golioth/golioth_sys.h>

}

#define TAG "app_main"

extern "C" void app_main(void) {
    nvs_init();
    shell_start();

    if (!nvs_credentials_are_set()) {
        while (1) {
            golioth_sys_msleep(1000);
            ESP_LOGW(TAG, "WiFi and golioth credentials are not set");
            ESP_LOGW(TAG, "Use the shell settings commands to set them, then restart");
            golioth_sys_msleep(UINT32_MAX);
        }
    }

    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    const std::string psk_id = nvs_read_golioth_psk_id();
    const std::string psk = nvs_read_golioth_psk();

    golioth_client_config_t config = {};
    config.credentials.auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK;
    config.credentials.psk.psk_id = psk_id.c_str();
    config.credentials.psk.psk_id_len = psk_id.length();
    config.credentials.psk.psk = psk.c_str();
    config.credentials.psk.psk_len = psk.length();

    golioth_client_t client = golioth_client_create(&config);

    int counter = 0;
    while (1) {
        GLTH_LOGI(TAG, "Hello, Golioth %d!", counter);
        golioth_lightdb_set_int_async(client, "counter", counter, NULL, NULL);
        golioth_sys_msleep(5000);
        counter++;
    }
}
