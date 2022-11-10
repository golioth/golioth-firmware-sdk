#pragma once

// Same as GLTH_LOGX, but with 32-bit timestamp (PRIu32),
// since MTB built-in C lib doesn't support 64-bit formatters.
#define GLTH_LOGX(COLOR, LEVEL, LEVEL_STR, TAG, ...) \
    do { \
        if ((LEVEL) <= golioth_debug_get_log_level()) { \
            uint32_t now_ms = (uint32_t)golioth_time_millis(); \
            printf(COLOR "%s (%" PRIu32 ") %s: ", LEVEL_STR, now_ms, TAG); \
            printf(__VA_ARGS__); \
            golioth_debug_printf(now_ms, LEVEL, TAG, __VA_ARGS__); \
            printf("%s", LOG_RESET_COLOR); \
            puts(""); \
        } \
    } while (0)
