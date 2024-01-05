/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/config.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef __UNUSED
#if defined(__GNUC__) || defined(__clang__)
#define __UNUSED __attribute__((unused))
#else /* defined(__GNUC__) || defined(__clang__) */
#define __UNUSED
#endif /* defined(__GNUC__) || defined(__clang__) */
#endif /* __UNUSED */

#ifndef LOG_TAG_DEFINE
#define LOG_TAG_DEFINE(tag) static __UNUSED const char* TAG = #tag
#endif

typedef void* golioth_client_t;

typedef enum {
    GOLIOTH_DEBUG_LOG_LEVEL_NONE,
    GOLIOTH_DEBUG_LOG_LEVEL_ERROR,
    GOLIOTH_DEBUG_LOG_LEVEL_WARN,
    GOLIOTH_DEBUG_LOG_LEVEL_INFO,
    GOLIOTH_DEBUG_LOG_LEVEL_DEBUG,
    GOLIOTH_DEBUG_LOG_LEVEL_VERBOSE,
} golioth_debug_log_level_t;

void golioth_debug_set_log_level(golioth_debug_log_level_t level);
golioth_debug_log_level_t golioth_debug_get_log_level(void);
void golioth_debug_hexdump(const char* tag, const void* addr, int len);
void golioth_debug_set_client(golioth_client_t client);
void golioth_debug_set_cloud_log_enabled(bool enable);
void golioth_debug_printf(
        uint64_t tstamp_ms,
        golioth_debug_log_level_t level,
        const char* tag,
        const char* format,
        ...);
