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
