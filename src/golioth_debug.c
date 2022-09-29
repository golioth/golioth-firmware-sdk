#include "golioth_debug.h"

static golioth_debug_log_level_t _level = GOLIOTH_DEBUG_LOG_LEVEL_INFO;

void golioth_debug_set_log_level(golioth_debug_log_level_t level) {
    _level = level;
}

golioth_debug_log_level_t golioth_debug_get_log_level(void) {
    return _level;
}
