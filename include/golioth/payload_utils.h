/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>


/// @defgroup golioth_payload_utils golioth_payload_utils
/// Functions for converting JSON types into C types
/// @{

/// Convert raw byte payload into an int32_t
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @return int32_t value returned from strtol(payload, NULL, 10)
int32_t golioth_payload_as_int(const uint8_t *payload, size_t payload_size);

/// Convert raw byte payload into a float
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @return float value returned from strtof(payload, NULL)
float golioth_payload_as_float(const uint8_t *payload, size_t payload_size);

/// Convert raw byte payload into a bool
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @retval true payload is exactly the string "true"
/// @retval false otherwise
bool golioth_payload_as_bool(const uint8_t *payload, size_t payload_size);

/// Returns true if payload has no contents
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @retval true payload is NULL, payload_size is 0 or payload is exactly the string "null"
/// @retval false otherwise
bool golioth_payload_is_null(const uint8_t *payload, size_t payload_size);

/// @}

#ifdef __cplusplus
}
#endif
