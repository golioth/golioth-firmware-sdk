#pragma once

#include "golioth_config.h"
#include "golioth_time.h"

typedef enum {
    GOLIOTH_DEBUG_LOG_LEVEL_ERROR,
    GOLIOTH_DEBUG_LOG_LEVEL_WARN,
    GOLIOTH_DEBUG_LOG_LEVEL_INFO,
    GOLIOTH_DEBUG_LOG_LEVEL_DEBUG,
    GOLIOTH_DEBUG_LOG_LEVEL_VERBOSE,
} golioth_debug_log_level_t;

void golioth_debug_set_log_level(golioth_debug_log_level_t level);
golioth_debug_log_level_t golioth_debug_get_log_level(void);

#if CONFIG_GOLIOTH_DEBUG_LOG_ENABLE

#ifndef GLTH_LOGX
#include <stdio.h>
#define GLTH_LOGX(LEVEL, LEVEL_STR, TAG, ...) \
    do { \
        if ((LEVEL) <= golioth_debug_get_log_level()) { \
            printf("%s (%lu) %s: ", LEVEL_STR, (uint32_t)golioth_time_millis(), TAG); \
            printf(__VA_ARGS__); \
            puts(""); \
        } \
    } while (0)
#endif /* GLTH_LOGX */

#define GLTH_LOGV(TAG, ...) GLTH_LOGX(GOLIOTH_DEBUG_LOG_LEVEL_VERBOSE, "V", TAG, __VA_ARGS__)
#define GLTH_LOGD(TAG, ...) GLTH_LOGX(GOLIOTH_DEBUG_LOG_LEVEL_DEBUG, "D", TAG, __VA_ARGS__)
#define GLTH_LOGI(TAG, ...) GLTH_LOGX(GOLIOTH_DEBUG_LOG_LEVEL_INFO, "I", TAG, __VA_ARGS__)
#define GLTH_LOGW(TAG, ...) GLTH_LOGX(GOLIOTH_DEBUG_LOG_LEVEL_WARN, "W", TAG, __VA_ARGS__)
#define GLTH_LOGE(TAG, ...) GLTH_LOGX(GOLIOTH_DEBUG_LOG_LEVEL_ERROR, "E", TAG, __VA_ARGS__)
#define GLTH_LOG_BUFFER_HEXDUMP(TAG, ...) /* TODO */

#else /* CONFIG_GOLIOTH_DEBUG_LOG_ENABLE */

#define GLTH_LOGV(TAG, ...)
#define GLTH_LOGD(TAG, ...)
#define GLTH_LOGI(TAG, ...)
#define GLTH_LOGW(TAG, ...)
#define GLTH_LOGE(TAG, ...)
#define GLTH_LOG_BUFFER_HEXDUMP(TAG, ...)

#endif /* CONFIG_GOLIOTH_DEBUG_LOG_ENABLE */
