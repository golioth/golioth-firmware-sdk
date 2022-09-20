/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "golioth_time.h"
#include <FreeRTOS.h>
#include <task.h>

uint64_t golioth_time_millis() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void golioth_time_delay_ms(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}
