/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "shell.h"
#include "wifi.h"
#include "leds.h"
#include "i2c.h"
#include "buttons.h"
#include "light_sensor.h"
#include "epaper.h"
#include "lis3dh.h"
#include "speaker.h"
#include "audio/coin.h"
#include "board.h"
#include "events.h"
#include "golioth.h"

#define TAG "magtag_demo"

static TimerHandle_t _timer250ms;
EventGroupHandle_t _event_group;

static struct {
    uint32_t light_mV;
    lis3dh_accel_data_t accel;
    char text[81];
    uint32_t leds[NUM_LEDS];
} _app_state;

static void on_timer250ms(TimerHandle_t xTimer) {
    xEventGroupSetBits(_event_group, EVENT_TIMER_250MS);
}

static void set_all_leds(uint32_t color) {
    leds_set_all_immediate(color);
    for (int i = 0; i < NUM_LEDS; i++) {
        _app_state.leds[i] = color;
    }
}

static void write_text(const char* text, size_t len) {
    assert(len < sizeof(_app_state.text) - 1);
    memset(_app_state.text, 0, sizeof(_app_state.text));
    strncpy(_app_state.text, text, len);
    epaper_autowrite_len((uint8_t*)text, len);
}

static void on_client_event(golioth_client_t client, golioth_client_event_t event, void* arg) {
    bool connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    ESP_LOGI(TAG, "Golioth client %s", connected ? "connected" : "disconnected");
    uint32_t rgb = (connected ? GREEN : BLUE);
    set_all_leds(rgb);
    const char* text = "Connected to Golioth!";
    write_text(text, strlen(text));
}

static void on_desired_buzz(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        return;
    }

    if (golioth_payload_is_null(payload, payload_size)) {
        return;
    }

    bool buzz = golioth_payload_as_bool(payload, payload_size);
    ESP_LOGI(TAG, "Got desired/buzz");

    if (buzz) {
        speaker_play_audio(coin_audio, sizeof(coin_audio), COIN_SAMPLE_RATE);
        golioth_lightdb_delete_async(client, path, NULL, NULL);
    }
}

static void on_desired_text(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        return;
    }

    if (golioth_payload_is_null(payload, payload_size)) {
        return;
    }
    if (payload_size >= 80) {
        ESP_LOGE(TAG, "Text too long, limit of 80 chars");
        return;
    }

    cJSON* json = cJSON_ParseWithLength((const char*)payload, payload_size);
    if (!json || !cJSON_IsString(json) || (json->valuestring == NULL)) {
        ESP_LOGE(TAG, "Failed to parse");
        return;
    }
    const char* text = json->valuestring;
    ESP_LOGI(TAG, "Got desired/text = %s", text);
    write_text(text, strlen(text));
    cJSON_Delete(json);
    golioth_lightdb_delete_async(client, path, NULL, NULL);
}

static void set_led_from_json(uint32_t led_index, const cJSON* json) {
    assert(led_index < NUM_LEDS);

    if (!json || !cJSON_IsString(json) || (json->valuestring == NULL)) {
        return;
    }
    const char* color_str = json->valuestring;

    size_t len = strlen(color_str);
    if ((len != 7) || (color_str[0] != '#')) {
        ESP_LOGW(TAG, "Invalid color string, expected #RRGGBB");
        return;
    }

    uint32_t color = (uint32_t)strtoul(&color_str[1], NULL, 16);
    ESP_LOGI(TAG, "Setting LED index %" PRIu32 " to color 0x%06" PRIx32, led_index, color);
    leds_set_led_immediate(led_index, color);
    _app_state.leds[led_index] = color;
}

static void on_desired_leds(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    if (response->status != GOLIOTH_OK) {
        return;
    }

    if (golioth_payload_is_null(payload, payload_size)) {
        return;
    }

    cJSON* json = cJSON_ParseWithLength((const char*)payload, payload_size);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse desired leds");
        return;
    }

    ESP_LOGI(TAG, "Got desired/leds: %.*s", payload_size, payload);

    set_led_from_json(0, cJSON_GetObjectItemCaseSensitive(json, "0"));
    set_led_from_json(1, cJSON_GetObjectItemCaseSensitive(json, "1"));
    set_led_from_json(2, cJSON_GetObjectItemCaseSensitive(json, "2"));
    set_led_from_json(3, cJSON_GetObjectItemCaseSensitive(json, "3"));

    cJSON_Delete(json);
    golioth_lightdb_delete_async(client, path, NULL, NULL);
}

static const char* led_color_to_str(uint32_t color) {
    static char buf[8] = {};
    snprintf(
            buf,
            sizeof(buf),
            "#%02" PRIx32 "%02" PRIx32 "%02" PRIx32,
            (color & 0xFF0000) >> 16,
            (color & 0x00FF00) >> 8,
            (color & 0x0000FF));
    return buf;
}

static void publish_state(golioth_client_t client) {
    static char jsonbuf[1024] = {};

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "light", _app_state.light_mV);
    cJSON_AddNumberToObject(root, "accX", _app_state.accel.x_mps2);
    cJSON_AddNumberToObject(root, "accY", _app_state.accel.y_mps2);
    cJSON_AddNumberToObject(root, "accZ", _app_state.accel.z_mps2);
    cJSON_AddStringToObject(root, "text", _app_state.text);

    cJSON* leds = cJSON_CreateObject();
    cJSON_AddStringToObject(leds, "0", led_color_to_str(_app_state.leds[0]));
    cJSON_AddStringToObject(leds, "1", led_color_to_str(_app_state.leds[1]));
    cJSON_AddStringToObject(leds, "2", led_color_to_str(_app_state.leds[2]));
    cJSON_AddStringToObject(leds, "3", led_color_to_str(_app_state.leds[3]));
    cJSON_AddItemToObject(root, "leds", leds);

    bool printed = cJSON_PrintPreallocated(root, jsonbuf, sizeof(jsonbuf) - 5, false);
    assert(printed);
    cJSON_Delete(root);
    golioth_lightdb_set_json_async(client, "state", jsonbuf, strlen(jsonbuf), NULL, NULL);
}

static void app_gpio_init(void) {
    gpio_install_isr_service(0);
    gpio_reset_pin(D13_LED_GPIO_PIN);
    gpio_set_direction(D13_LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    buttons_gpio_init();
}

void app_main(void) {
    _event_group = xEventGroupCreate();
    _timer250ms = xTimerCreate("timer250ms", 250 / portTICK_PERIOD_MS, pdTRUE, NULL, on_timer250ms);
    xTimerStart(_timer250ms, 0);

    nvs_flash_init();
    app_gpio_init();
    i2c_master_init(I2C_SCL_PIN, I2C_SDA_PIN);
    lis3dh_init(LIS3DH_I2C_ADDR);
    speaker_init(SPEAKER_DAC1_PIN, SPEAKER_ENABLE_PIN);
    light_sensor_init();

    bool d13_on = true;
    gpio_set_level(D13_LED_GPIO_PIN, d13_on);

    leds_init();
    set_all_leds(YELLOW);

    epaper_init();

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

    // Initialize WiFi and wait for it to connect
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    set_all_leds(BLUE);

    const char* psk_id = nvs_read_golioth_psk_id();
    const char* psk = nvs_read_golioth_psk();

    golioth_client_config_t config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                    .psk = {
                            .psk_id = psk_id,
                            .psk_id_len = strlen(psk_id),
                            .psk = psk,
                            .psk_len = strlen(psk),
                    }}};
    golioth_client_t client = golioth_client_create(&config);
    assert(client);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_lightdb_observe_async(client, "desired/buzz", on_desired_buzz, NULL);
    golioth_lightdb_observe_async(client, "desired/leds", on_desired_leds, NULL);
    golioth_lightdb_observe_async(client, "desired/text", on_desired_text, NULL);

    while (1) {
        EventBits_t events =
                xEventGroupWaitBits(_event_group, EVENT_ANY, pdTRUE, pdFALSE, portMAX_DELAY);
        if (events & EVENT_BUTTON_A_PRESSED) {
            ESP_LOGI(TAG, "Button A pressed");
        }
        if (events & EVENT_BUTTON_B_PRESSED) {
            ESP_LOGI(TAG, "Button B pressed");
        }
        if (events & EVENT_BUTTON_C_PRESSED) {
            ESP_LOGI(TAG, "Button C pressed");
        }
        if (events & EVENT_BUTTON_D_PRESSED) {
            ESP_LOGI(TAG, "Button D pressed");
            speaker_play_audio(coin_audio, sizeof(coin_audio), COIN_SAMPLE_RATE);
        }
        if (events & EVENT_TIMER_250MS) {
            static int iteration = 0;

            d13_on = !d13_on;
            gpio_set_level(D13_LED_GPIO_PIN, d13_on);

            if ((iteration % 4) == 0) {  // 1 s
                lis3dh_accel_read(&_app_state.accel);
                ESP_LOGI(
                        TAG,
                        "accel (x, y, z) = (%.03f, %.03f, %.03f) m/s^2",
                        _app_state.accel.x_mps2,
                        _app_state.accel.y_mps2,
                        _app_state.accel.z_mps2);

                _app_state.light_mV = light_sensor_read_mV();
                ESP_LOGI(TAG, "light sensor = %" PRIu32 " mV", _app_state.light_mV);

                if (golioth_client_is_connected(client)) {
                    publish_state(client);
                }
            }

            iteration++;
        }
    };
}
