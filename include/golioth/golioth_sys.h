/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <golioth/config.h>

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/

// For functions that take a wait/timeout parameter, -1 will wait forever
#define GOLIOTH_SYS_WAIT_FOREVER -1

void golioth_sys_msleep(uint32_t ms);
uint64_t golioth_sys_now_ms(void);

/*--------------------------------------------------
 * Mutexes
 *------------------------------------------------*/

// Opaque handle for mutex
typedef void *golioth_sys_mutex_t;

golioth_sys_mutex_t golioth_sys_mutex_create(void);
bool golioth_sys_mutex_lock(golioth_sys_mutex_t mutex, int32_t ms_to_wait);
bool golioth_sys_mutex_unlock(golioth_sys_mutex_t mutex);
void golioth_sys_mutex_destroy(golioth_sys_mutex_t mutex);

/*--------------------------------------------------
 * Semaphores
 *------------------------------------------------*/

// Opaque handle for semaphore
typedef void *golioth_sys_sem_t;

golioth_sys_sem_t golioth_sys_sem_create(uint32_t sem_max_count, uint32_t sem_initial_count);
bool golioth_sys_sem_take(golioth_sys_sem_t sem, int32_t ms_to_wait);
bool golioth_sys_sem_give(golioth_sys_sem_t sem);
void golioth_sys_sem_destroy(golioth_sys_sem_t sem);
int golioth_sys_sem_get_fd(golioth_sys_sem_t sem);

/*--------------------------------------------------
 * Software Timers
 *------------------------------------------------*/

// Opaque handle for timers
typedef void *golioth_sys_timer_t;

typedef void (*golioth_sys_timer_fn_t)(golioth_sys_timer_t timer, void *user_arg);

struct golioth_timer_config
{
    const char *name;
    uint32_t expiration_ms;
    golioth_sys_timer_fn_t fn;
    void *user_arg;
};

golioth_sys_timer_t golioth_sys_timer_create(const struct golioth_timer_config *config);
bool golioth_sys_timer_start(golioth_sys_timer_t timer);
bool golioth_sys_timer_reset(golioth_sys_timer_t timer);
void golioth_sys_timer_destroy(golioth_sys_timer_t timer);

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/

// Opaque handle for threads
typedef void *golioth_sys_thread_t;

typedef void (*golioth_sys_thread_fn_t)(void *user_arg);

struct golioth_thread_config
{
    const char *name;
    golioth_sys_thread_fn_t fn;
    void *user_arg;
    int32_t stack_size;  // in bytes
    int32_t prio;        // large numbers == high priority
};

golioth_sys_thread_t golioth_sys_thread_create(const struct golioth_thread_config *config);
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
 * Random
 *------------------------------------------------*/

// Can be overridden via golioth_{user,port}_config
#ifndef golioth_sys_srand
#define golioth_sys_srand(seed) srand((seed))
#endif

#ifndef golioth_sys_rand
#define golioth_sys_rand() rand()
#endif

/*--------------------------------------------------
 * Hash
 *------------------------------------------------*/

/* Hash functions are only needed when the OTA component is enabled. */

/// Opaque handle for sha256 context
typedef void *golioth_sys_sha256_t;

/// Create a context for generating a sha256 hash.
///
/// Dynamically creates and initializes a context, then returns an opaque handle to the context.
/// The handle is a required parameter for all other Golioth sha256 functions.
///
/// @return Non-NULL The sha256 context handle (success)
/// @return NULL There was an error creating the context
golioth_sys_sha256_t golioth_sys_sha256_create(void);

/// Destroys a Golioth sha256 context
///
/// Frees memory dynamically allocated for golioth_sys_sha256_t context handle by @ref
/// golioth_sys_sha256_create.
///
/// @param sha_ctx A sha256 context handle
void golioth_sys_sha256_destroy(golioth_sys_sha256_t sha_ctx);

/// Adds an input buffer into a sha256 hash calculation.
///
/// Call on an existing context to add data to the sha256 calculation. May be called repeatedly (eg:
/// each block in a block download) or once for a large data set (eg: calculate the hash after
/// writing all bytes to memory).
///
/// @param sha_ctx A sha256 context handle
/// @param input   Input buffer to be added to the sha256 calculation
/// @param len     Length of the input buffer, in bytes.
///
/// @return GOLIOTH_OK On success
/// @return GOLIOTH_ERR_FAIL On failure
enum golioth_status golioth_sys_sha256_update(golioth_sys_sha256_t sha_ctx,
                                              const uint8_t *input,
                                              size_t len);

/// Finalizes a sha256 hash calculation and outputs to a binary buffer.
///
/// @param sha_ctx A sha256 context handle.
/// @param output  A buffer of exactly 32 bytes where the sha256 value is written.
///
/// @return GOLIOTH_OK On success
/// @return GOLIOTH_ERR_FAIL On failure
enum golioth_status golioth_sys_sha256_finish(golioth_sys_sha256_t sha_ctx, uint8_t *output);

/// Convert a string of hexadecimal values to an array of bytes
///
/// @param hex    Pointer at a hexadecimal string.
/// @param hexlen Length of the hex string.
/// @param buf    A buffer where binary values will be written.
/// @param buflen Length of the binary buffer.
///
/// @return GOLIOTH_OK On success
/// @return GOLIOTH_ERR_FAIL On failure
size_t golioth_sys_hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen);

/*--------------------------------------------------
 * Misc
 *------------------------------------------------*/

// Can be used to adjust behavior based on client connection status,
// e.g. to turn logging backends on and off.
void golioth_sys_client_connected(void *client);
void golioth_sys_client_disconnected(void *client);

/*--------------------------------------------------
 * Logging
 *------------------------------------------------*/

#if defined(CONFIG_GOLIOTH_DEBUG_LOG)

#include <stdio.h>
#include "golioth_debug.h"

#ifndef LOG_COLOR_RED
#define LOG_COLOR_RED "31"
#endif

#ifndef LOG_COLOR_GREEN
#define LOG_COLOR_GREEN "32"
#endif

#ifndef LOG_COLOR_BROWN
#define LOG_COLOR_BROWN "33"
#endif

#ifndef LOG_COLOR
#define LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#endif

#ifndef LOG_RESET_COLOR
#define LOG_RESET_COLOR "\033[0m"
#endif

#ifndef LOG_COLOR_E
#define LOG_COLOR_E LOG_COLOR(LOG_COLOR_RED)
#endif

#ifndef LOG_COLOR_W
#define LOG_COLOR_W LOG_COLOR(LOG_COLOR_BROWN)
#endif

#ifndef LOG_COLOR_I
#define LOG_COLOR_I LOG_COLOR(LOG_COLOR_GREEN)
#endif

#ifndef LOG_COLOR_D
#define LOG_COLOR_D
#endif

#ifndef LOG_COLOR_V
#define LOG_COLOR_V
#endif

#ifndef GLTH_LOGX
#define GLTH_LOGX(COLOR, LEVEL, LEVEL_STR, TAG, ...)                       \
    do                                                                     \
    {                                                                      \
        if ((LEVEL) <= golioth_debug_get_log_level())                      \
        {                                                                  \
            uint64_t now_ms = golioth_sys_now_ms();                        \
            printf(COLOR "%s (%" PRIu64 ") %s: ", LEVEL_STR, now_ms, TAG); \
            printf(__VA_ARGS__);                                           \
            golioth_debug_printf(now_ms, LEVEL, TAG, __VA_ARGS__);         \
            printf("%s", LOG_RESET_COLOR);                                 \
            puts("");                                                      \
        }                                                                  \
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
    do                                                     \
    {                                                      \
        if ((level) <= golioth_debug_get_log_level())      \
        {                                                  \
            golioth_debug_hexdump(TAG, payload, size);     \
        }                                                  \
    } while (0);
#endif

#else /* CONFIG_GOLIOTH_DEBUG_LOG */

#define GLTH_LOGV(TAG, ...)
#define GLTH_LOGD(TAG, ...)
#define GLTH_LOGI(TAG, ...)
#define GLTH_LOGW(TAG, ...)
#define GLTH_LOGE(TAG, ...)
#define GLTH_LOG_BUFFER_HEXDUMP(TAG, ...)

#endif /* CONFIG_GOLIOTH_DEBUG_LOG */

#ifdef __cplusplus
}
#endif
