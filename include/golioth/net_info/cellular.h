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

enum golioth_cellular_type
{
    GOLIOTH_CELLULAR_TYPE_LTECATM,
    GOLIOTH_CELLULAR_TYPE_NBIOT,
};

/** @brief Cellular tower information, which is passed to golioth_net_info_cellular_append() */
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

/// Append information about cellular network (cell tower) to network information
///
/// @param info Network information
/// @param cell Information about cellular network
///
/// @retval GOLIOTH_OK Cellular network info added successfully
/// @retval GOLIOTH_ERR_MEM_ALLOC Not enough memory in network info buffer
enum golioth_status golioth_net_info_cellular_append(struct golioth_net_info *info,
                                                     const struct golioth_cellular_info *cell);

#ifdef __cplusplus
}
#endif
