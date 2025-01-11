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

enum golioth_cellular_type
{
    GOLIOTH_CELLULAR_TYPE_LTECATM,
    GOLIOTH_CELLULAR_TYPE_NBIOT,
};

/** @brief Wi-Fi scan result, which is passed to golioth_location_cellular_append() */
struct golioth_cellular_info
{
    /** Type */
    enum golioth_cellular_type type;
    /** MCC */
    uint16_t mcc;
    /** MNC */
    uint16_t mnc;
    /** id */
    uint32_t id;
    /** Signal strength */
    int8_t strength;
};

/// Append information about cellular network (cell tower) to location request
///
/// @param req Location request
/// @param cell Information about cellular network
///
/// @retval GOLIOTH_OK Location request finished successfully
/// @retval GOLIOTH_ERR_MEM_ALLOC Not enough memory in request buffer
enum golioth_status golioth_location_cellular_append(struct golioth_location_req *req,
                                                     const struct golioth_cellular_info *cell);

#ifdef __cplusplus
}
#endif
