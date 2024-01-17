/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize and start the BLE stack
void ble_init(const char *device_name);

#ifdef __cplusplus
}
#endif
