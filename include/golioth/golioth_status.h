/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/// Status code enum used throughout Golioth SDK
#define FOREACH_GOLIOTH_STATUS(STATUS) \
    STATUS(GOLIOTH_OK) /* 0 */ \
    STATUS(GOLIOTH_ERR_FAIL) \
    STATUS(GOLIOTH_ERR_DNS_LOOKUP) \
    STATUS(GOLIOTH_ERR_NOT_IMPLEMENTED) \
    STATUS(GOLIOTH_ERR_MEM_ALLOC) \
    STATUS(GOLIOTH_ERR_NULL) /* 5 */ \
    STATUS(GOLIOTH_ERR_INVALID_FORMAT) \
    STATUS(GOLIOTH_ERR_SERIALIZE) \
    STATUS(GOLIOTH_ERR_IO) \
    STATUS(GOLIOTH_ERR_TIMEOUT) \
    STATUS(GOLIOTH_ERR_QUEUE_FULL) /* 10 */ \
    STATUS(GOLIOTH_ERR_NOT_ALLOWED) \
    STATUS(GOLIOTH_ERR_INVALID_STATE) \
    STATUS(GOLIOTH_ERR_NO_MORE_DATA)

#define GENERATE_GOLIOTH_STATUS_ENUM(code) code,
enum golioth_status {
    FOREACH_GOLIOTH_STATUS(GENERATE_GOLIOTH_STATUS_ENUM) NUM_GOLIOTH_STATUS_CODES
};

/// Convert status code to human-readable string
///
/// @param status status code to convert to string
///
/// @return static string representation of status code
const char* golioth_status_to_str(enum golioth_status status);

/// Helper macro, for functions that return type enum golioth_status,
/// if they want to return early when an expression returns something
/// other than GOLIOTH_OK.
#define GOLIOTH_STATUS_RETURN_IF_ERROR(expr) \
    do { \
        enum golioth_status status = (expr); \
        if (status != GOLIOTH_OK) { \
            return status; \
        } \
    } while (0)
