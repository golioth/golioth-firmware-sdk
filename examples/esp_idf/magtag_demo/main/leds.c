#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "leds.h"
#include "led_strip.h"
#include "board.h"

#define TAG "leds"

// TODO - address this warning on recent esp-idf versions:
// #warning "The legacy RMT driver is deprecated, please use driver/rmt_tx.h and/or driver/rmt_rx.h"
// [-Werror=cpp]

#define RMT_TX_CHANNEL RMT_CHANNEL_0

#define EXAMPLE_CHASE_SPEED_MS (250)

static led_strip_t* _strip;

void leds_init(void) {
    // Enable power
    gpio_reset_pin(LED_STRIP_POWER_PIN);
    gpio_set_direction(LED_STRIP_POWER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STRIP_POWER_PIN, 0);  // active low

    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_STRIP_DATA_PIN, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config =
            LED_STRIP_DEFAULT_CONFIG(LED_STRIP_NUM_RGB_LEDS, (led_strip_dev_t)config.channel);
    _strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!_strip) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
    }
    leds_set_all_immediate(BLACK);
}

void leds_display(void) {
    esp_err_t err = _strip->refresh(_strip, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "refresh err: %d (%s)", err, esp_err_to_name(err));
    }
}

void leds_set_led(int led_index, uint32_t rgb) {
    uint32_t r = (rgb & 0xFF0000) >> 16;
    uint32_t g = (rgb & 0x00FF00) >> 8;
    uint32_t b = (rgb & 0x0000FF);
    esp_err_t err = _strip->set_pixel(_strip, led_index, r, g, b);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_pixel err: %d (%s)", err, esp_err_to_name(err));
    }
}

void leds_set_led_immediate(int led_index, uint32_t rgb) {
    leds_set_led(led_index, rgb);
    leds_display();
}

void leds_set_all(uint32_t rgb) {
    for (int i = 0; i < LED_STRIP_NUM_RGB_LEDS; i++) {
        leds_set_led(i, rgb);
    }
}
void leds_set_all_immediate(uint32_t rgb) {
    leds_set_all(rgb);
    leds_display();
}
