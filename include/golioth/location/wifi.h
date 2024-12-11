/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stdint.h>

#include <golioth/location.h>

/** @brief Wi-Fi scan result, which is passed to golioth_location_wifi_append() */
struct golioth_wifi_scan_result
{
    /** RSSI */
    int8_t rssi;
    /** BSSID */
    uint8_t mac[6];
};

/// Append information about scanned WiFi network to location request
///
/// @param req Location request
/// @param entry Information about scanned WiFi network
///
/// @retval GOLIOTH_OK Location request finished successfully
/// @retval GOLIOTH_ERR_MEM_ALLOC Not enough memory in request buffer
enum golioth_status golioth_location_wifi_append(struct golioth_location_req *req,
                                                 const struct golioth_wifi_scan_result *entry);

#ifdef __cplusplus
}
#endif
