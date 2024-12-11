/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

enum golioth_status golioth_location_wifi_append(struct golioth_location_req *req,
                                                 const struct golioth_wifi_scan_result *entry);
