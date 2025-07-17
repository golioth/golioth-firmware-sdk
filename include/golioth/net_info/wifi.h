/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stdint.h>

#include <golioth/net_info.h>

/** @brief Wi-Fi scan result, which is passed to golioth_net_info_wifi_append() */
struct golioth_wifi_scan_result
{
    /** RSSI */
    int8_t rssi;
    /** BSSID */
    uint8_t mac[6];
};

/// Append information about scanned Wi-Fi network to network information
///
/// @param info Network information
/// @param entry Information about scanned Wi-Fi network
///
/// @retval GOLIOTH_OK Wi-Fi network info added successfully
/// @retval GOLIOTH_ERR_MEM_ALLOC Not enough memory in network info buffer
enum golioth_status golioth_net_info_wifi_append(struct golioth_net_info *info,
                                                 const struct golioth_wifi_scan_result *entry);

#ifdef __cplusplus
}
#endif
