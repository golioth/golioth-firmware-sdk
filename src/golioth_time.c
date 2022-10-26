/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "golioth_time.h"
#include "golioth_sys.h"

uint64_t golioth_time_millis() {
    return golioth_sys_now_ms();
}

void golioth_time_delay_ms(uint32_t ms) {
    return golioth_sys_msleep(ms);
}
