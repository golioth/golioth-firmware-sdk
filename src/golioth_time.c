/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "golioth_time.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_timer.h"

uint64_t golioth_time_micros() {
    int64_t time_us = esp_timer_get_time();
    if (time_us < 0) {
        time_us = 0;
    }
    return time_us;
}

uint64_t golioth_time_millis() {
    return golioth_time_micros() / 1000;
}

void golioth_time_delay_ms(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}
