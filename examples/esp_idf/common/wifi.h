/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void wifi_init(const char* ssid, const char* password);
void wifi_wait_for_connected(void);
bool wifi_wait_for_connected_with_timeout(uint32_t timeout_s);

#ifdef __cplusplus
}
#endif
