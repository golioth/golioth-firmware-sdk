/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/* ModusToolbox uses an older 2.25 version of mbedtls
 * These function names changed in version 3.0:
 * https://github.com/Mbed-TLS/mbedtls/commit/26371e47939d02d5f1a5ad9012f49aff0c7f5bc4
 */
#define mbedtls_sha256_starts mbedtls_sha256_starts_ret
#define mbedtls_sha256_update mbedtls_sha256_update_ret
#define mbedtls_sha256_finish mbedtls_sha256_finish_ret

// Same as GLTH_LOGX, but with 32-bit timestamp (PRIu32),
// since MTB built-in C lib doesn't support 64-bit formatters.
#define GLTH_LOGX(COLOR, LEVEL, LEVEL_STR, TAG, ...) \
    do { \
        if ((LEVEL) <= golioth_debug_get_log_level()) { \
            uint32_t now_ms = (uint32_t)golioth_sys_now_ms(); \
            printf(COLOR "%s (%" PRIu32 ") %s: ", LEVEL_STR, now_ms, TAG); \
            printf(__VA_ARGS__); \
            golioth_debug_printf(now_ms, LEVEL, TAG, __VA_ARGS__); \
            printf("%s", LOG_RESET_COLOR); \
            puts(""); \
        } \
    } while (0)
