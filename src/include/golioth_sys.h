#pragma once

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include "golioth_config.h"

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/

// For functions that take a wait/timeout parameter, -1 will wait forever
#define GOLIOTH_SYS_WAIT_FOREVER -1

void golioth_sys_msleep(uint32_t ms);
uint64_t golioth_sys_now_ms(void);

/*--------------------------------------------------
 * Semaphores
 *------------------------------------------------*/

// Opaque handle for semaphore
typedef void* golioth_sys_sem_t;

golioth_sys_sem_t golioth_sys_sem_create(uint32_t sem_max_count, uint32_t sem_initial_count);
bool golioth_sys_sem_take(golioth_sys_sem_t sem, int32_t ms_to_wait);
bool golioth_sys_sem_give(golioth_sys_sem_t sem);
void golioth_sys_sem_destroy(golioth_sys_sem_t sem);

/*--------------------------------------------------
 * Software Timers
 *------------------------------------------------*/

// Opaque handle for timers
typedef void* golioth_sys_timer_t;

typedef void (*golioth_sys_timer_fn_t)(golioth_sys_timer_t timer, void* user_arg);

typedef struct {
    const char* name;
    uint32_t expiration_ms;
    golioth_sys_timer_fn_t fn;
    void* user_arg;
} golioth_sys_timer_config_t;

golioth_sys_timer_t golioth_sys_timer_create(golioth_sys_timer_config_t config);
bool golioth_sys_timer_start(golioth_sys_timer_t timer);
bool golioth_sys_timer_reset(golioth_sys_timer_t timer);
void golioth_sys_timer_destroy(golioth_sys_timer_t timer);

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/

// Opaque handle for threads
typedef void* golioth_sys_thread_t;

typedef void (*golioth_sys_thread_fn_t)(void* user_arg);

typedef struct {
    const char* name;
    golioth_sys_thread_fn_t fn;
    void* user_arg;
    int32_t stack_size;  // in bytes
    int32_t prio;        // large numbers == high priority
} golioth_sys_thread_config_t;

golioth_sys_thread_t golioth_sys_thread_create(golioth_sys_thread_config_t config);
void golioth_sys_thread_destroy(golioth_sys_thread_t thread);

/*--------------------------------------------------
 * Malloc/Free
 *------------------------------------------------*/

// Can be overridden via golioth_{user,port}_config
#ifndef golioth_sys_malloc
#define golioth_sys_malloc(sz) malloc((sz))
#endif

#ifndef golioth_sys_free
#define golioth_sys_free(ptr) free((ptr))
#endif

/*--------------------------------------------------
 * Logging
 *------------------------------------------------*/

#if CONFIG_GOLIOTH_DEBUG_LOG_ENABLE

#include <stdio.h>
#include "golioth_debug.h"

#define LOG_COLOR_RED "31"
#define LOG_COLOR_GREEN "32"
#define LOG_COLOR_BROWN "33"
#define LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define LOG_RESET_COLOR "\033[0m"
#define LOG_COLOR_E LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D
#define LOG_COLOR_V

#ifndef GLTH_LOGX
#define GLTH_LOGX(COLOR, LEVEL, LEVEL_STR, TAG, ...) \
    do { \
        if ((LEVEL) <= golioth_debug_get_log_level()) { \
            uint64_t now_ms = golioth_time_millis(); \
            printf(COLOR "%s (%" PRIu64 ") %s: ", LEVEL_STR, now_ms, TAG); \
            printf(__VA_ARGS__); \
            golioth_debug_printf(now_ms, LEVEL, TAG, __VA_ARGS__); \
            printf("%s", LOG_RESET_COLOR); \
            puts(""); \
        } \
    } while (0)
#endif

// Logging macros can be overridden from golioth_{user,port}config.h
#ifndef GLTH_LOGV
#define GLTH_LOGV(TAG, ...) \
    GLTH_LOGX(LOG_COLOR_V, GOLIOTH_DEBUG_LOG_LEVEL_VERBOSE, "V", TAG, __VA_ARGS__)
#endif

#ifndef GLTH_LOGD
#define GLTH_LOGD(TAG, ...) \
    GLTH_LOGX(LOG_COLOR_D, GOLIOTH_DEBUG_LOG_LEVEL_DEBUG, "D", TAG, __VA_ARGS__)
#endif

#ifndef GLTH_LOGI
#define GLTH_LOGI(TAG, ...) \
    GLTH_LOGX(LOG_COLOR_I, GOLIOTH_DEBUG_LOG_LEVEL_INFO, "I", TAG, __VA_ARGS__)
#endif

#ifndef GLTH_LOGW
#define GLTH_LOGW(TAG, ...) \
    GLTH_LOGX(LOG_COLOR_W, GOLIOTH_DEBUG_LOG_LEVEL_WARN, "W", TAG, __VA_ARGS__)
#endif

#ifndef GLTH_LOGE
#define GLTH_LOGE(TAG, ...) \
    GLTH_LOGX(LOG_COLOR_E, GOLIOTH_DEBUG_LOG_LEVEL_ERROR, "E", TAG, __VA_ARGS__)
#endif

#ifndef GLTH_LOG_BUFFER_HEXDUMP
#define GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, size, level) \
    do { \
        if ((level) <= golioth_debug_get_log_level()) { \
            golioth_debug_hexdump(TAG, payload, size); \
        } \
    } while (0);
#endif

#else /* CONFIG_GOLIOTH_DEBUG_LOG_ENABLE */

#define GLTH_LOGV(TAG, ...)
#define GLTH_LOGD(TAG, ...)
#define GLTH_LOGI(TAG, ...)
#define GLTH_LOGW(TAG, ...)
#define GLTH_LOGE(TAG, ...)
#define GLTH_LOG_BUFFER_HEXDUMP(TAG, ...)

#endif /* CONFIG_GOLIOTH_DEBUG_LOG_ENABLE */
