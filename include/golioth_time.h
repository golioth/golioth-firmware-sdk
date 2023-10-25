/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

/// For Golioth APIs that take a timeout parameter
#define GOLIOTH_WAIT_FOREVER -1

/// Time since boot, in milliseconds
uint64_t golioth_time_millis(void);

/// Delay the current RTOS thread by a certain number of milliseconds.
void golioth_time_delay_ms(uint32_t ms);
