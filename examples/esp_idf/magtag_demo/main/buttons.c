#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "buttons.h"
#include "board.h"
#include "golioth_time.h"
#include "time.h"
#include "events.h"

#define TAG "buttons"

static uint32_t _debounce_ms[4];

static void IRAM_ATTR button_isr(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    assert(gpio_num < 4);
    if (_debounce_ms[gpio_num] > golioth_time_millis()) {
        // Too soon, don't handle this edge
        return;
    }
    _debounce_ms[gpio_num] = golioth_time_millis() + 100;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(_event_group, (1 << gpio_num), &xHigherPriorityTaskWoken);
}

void buttons_gpio_init(void) {
    gpio_config_t button_config = {
            .intr_type = GPIO_INTR_POSEDGE,
            .pin_bit_mask =
                    ((1 << BUTTON_A_GPIO_PIN) | (1 << BUTTON_B_GPIO_PIN) | (1 << BUTTON_C_GPIO_PIN)
                     | (1 << BUTTON_D_GPIO_PIN)),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = 1,
    };
    gpio_config(&button_config);
    gpio_isr_handler_add(BUTTON_A_GPIO_PIN, button_isr, (void*)0);
    gpio_isr_handler_add(BUTTON_B_GPIO_PIN, button_isr, (void*)1);
    gpio_isr_handler_add(BUTTON_C_GPIO_PIN, button_isr, (void*)2);
    gpio_isr_handler_add(BUTTON_D_GPIO_PIN, button_isr, (void*)3);
}
