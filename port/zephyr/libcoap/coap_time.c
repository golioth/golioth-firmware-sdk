/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "coap3/coap_internal.h"

void coap_ticks(coap_tick_t* t) {
    *t = (k_uptime_get() * COAP_TICKS_PER_SECOND) / MSEC_PER_SEC;
}

void coap_clock_init(void) {}

uint64_t coap_ticks_to_rt_us(coap_tick_t t) {
    return (uint64_t)t * 1000000 / COAP_TICKS_PER_SECOND;
}
