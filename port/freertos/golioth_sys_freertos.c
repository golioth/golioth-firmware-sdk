/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/golioth_sys.h>
#include <golioth/golioth_status.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <string.h>  // memset
#include "mbedtls/sha256.h"

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/

uint64_t golioth_sys_now_ms()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void golioth_sys_msleep(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

/*--------------------------------------------------
 * Mutexes
 *------------------------------------------------*/

golioth_sys_mutex_t golioth_sys_mutex_create(void)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    return (golioth_sys_mutex_t) mutex;
}

bool golioth_sys_mutex_lock(golioth_sys_mutex_t mutex, int32_t ms_to_wait)
{
    TickType_t ticks_to_wait =
        (ms_to_wait < 0 ? portMAX_DELAY : (uint32_t) ms_to_wait / portTICK_PERIOD_MS);
    return (pdTRUE == xSemaphoreTake((SemaphoreHandle_t) mutex, ticks_to_wait));
}

bool golioth_sys_mutex_unlock(golioth_sys_mutex_t mutex)
{
    return (pdTRUE == xSemaphoreGive((SemaphoreHandle_t) mutex));
}

void golioth_sys_mutex_destroy(golioth_sys_mutex_t mutex)
{
    vSemaphoreDelete((SemaphoreHandle_t) mutex);
}

/*--------------------------------------------------
 * Semaphores
 *------------------------------------------------*/

golioth_sys_sem_t golioth_sys_sem_create(uint32_t sem_max_count, uint32_t sem_initial_count)
{
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(sem_max_count, sem_initial_count);
    return (golioth_sys_sem_t) sem;
}

bool golioth_sys_sem_take(golioth_sys_sem_t sem, int32_t ms_to_wait)
{
    TickType_t ticks_to_wait =
        (ms_to_wait < 0 ? portMAX_DELAY : (uint32_t) ms_to_wait / portTICK_PERIOD_MS);
    return (pdTRUE == xSemaphoreTake((SemaphoreHandle_t) sem, ticks_to_wait));
}

bool golioth_sys_sem_give(golioth_sys_sem_t sem)
{
    return (pdTRUE == xSemaphoreGive((SemaphoreHandle_t) sem));
}

void golioth_sys_sem_destroy(golioth_sys_sem_t sem)
{
    vSemaphoreDelete((SemaphoreHandle_t) sem);
}

int golioth_sys_sem_get_fd(golioth_sys_sem_t sem)
{
    return -1;
}

/*--------------------------------------------------
 * Software Timers
 *------------------------------------------------*/

// Create a wrapper struct to workaround awkwardness of FreeRTOS
// timer callbacks not having a separate user_arg param (it's
// embedded inside of the TimerHandle_t as the "TimerID").
//
// This wrapped timer is the underlying type of opaque golioth_sys_timer_t.
typedef struct
{
    TimerHandle_t timer;
    golioth_sys_timer_fn_t fn;
    void *user_arg;
} wrapped_timer_t;

// Same callback used for all timers. We extract the user arg from
// the underlying freertos timer and call the callback with it.
static void freertos_timer_callback(TimerHandle_t xTimer)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) pvTimerGetTimerID(xTimer);
    wt->fn(wt->timer, wt->user_arg);
}

static TickType_t ms_to_ticks(uint32_t ms)
{
    // Round to the nearest multiple of the tick period
    uint32_t remainder = ms % portTICK_PERIOD_MS;
    uint32_t rounded_ms = ms;
    if (remainder != 0)
    {
        rounded_ms = ms + portTICK_PERIOD_MS - remainder;
    }
    return (rounded_ms / portTICK_PERIOD_MS);
}

golioth_sys_timer_t golioth_sys_timer_create(const struct golioth_timer_config *config)
{
    assert(config->fn);  // timer callback function is required

    wrapped_timer_t *wrapped_timer =
        (wrapped_timer_t *) golioth_sys_malloc(sizeof(wrapped_timer_t));
    memset(wrapped_timer, 0, sizeof(wrapped_timer_t));

    TimerHandle_t timer = xTimerCreate(config->name,
                                       ms_to_ticks(config->expiration_ms),
                                       pdTRUE,  // periodic
                                       wrapped_timer,
                                       freertos_timer_callback);

    wrapped_timer->timer = timer;
    wrapped_timer->fn = config->fn;
    wrapped_timer->user_arg = config->user_arg;

    return wrapped_timer;
}

bool golioth_sys_timer_start(golioth_sys_timer_t timer)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) timer;
    if (!wt)
    {
        return false;
    }
    return (pdPASS == xTimerStart((TimerHandle_t) wt->timer, 0));  // non-blocking
}

bool golioth_sys_timer_reset(golioth_sys_timer_t timer)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) timer;
    if (!wt)
    {
        return false;
    }
    return (pdPASS == xTimerReset((TimerHandle_t) wt->timer, 0));  // non-blocking
}

void golioth_sys_timer_destroy(golioth_sys_timer_t timer)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) timer;
    if (!wt)
    {
        return;
    }
    xTimerDelete(wt->timer, 0);  // non-blocking
    golioth_sys_free(wt);
}

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/

golioth_sys_thread_t golioth_sys_thread_create(const struct golioth_thread_config *config)
{
    TaskHandle_t task_handle = NULL;
    xTaskCreate(config->fn,
                config->name,
                config->stack_size,
                config->user_arg,
                config->prio,
                &task_handle);
    return (golioth_sys_thread_t) task_handle;
}

void golioth_sys_thread_destroy(golioth_sys_thread_t thread)
{
    vTaskDelete((TaskHandle_t) thread);
}

/*-------------------------------------------------
 * Hash
 *------------------------------------------------*/

struct golioth_hash
{
    mbedtls_sha256_context sha256_ctx;
};

golioth_sys_sha256_t golioth_sys_sha256_create(void)
{
    struct golioth_hash *hash;

    hash = golioth_sys_malloc(sizeof(*hash));
    if (!hash)
    {
        return NULL;
    }

    golioth_sys_sha256_init(hash);

    return (golioth_sys_sha256_t) hash;
}

void golioth_sys_sha256_init(golioth_sys_sha256_t sha_ctx)
{
    if (!sha_ctx)
    {
        return;
    }

    struct golioth_hash *hash = sha_ctx;
    mbedtls_sha256_init(&hash->sha256_ctx);
    mbedtls_sha256_starts(&hash->sha256_ctx, 0);
}

void golioth_sys_sha256_free(golioth_sys_sha256_t sha_ctx)
{
    if (!sha_ctx)
    {
        return;
    }

    struct golioth_hash *hash = sha_ctx;
    mbedtls_sha256_free(&hash->sha256_ctx);
}

enum golioth_status golioth_sys_sha256_update(golioth_sys_sha256_t sha_ctx,
                                              uint8_t *input,
                                              size_t len)
{
    if (!sha_ctx || !input)
    {
        return GOLIOTH_ERR_NULL;
    }

    struct golioth_hash *hash = sha_ctx;
    int err = mbedtls_sha256_update(&hash->sha256_ctx, input, len);
    if (err) {
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

enum golioth_status golioth_sys_sha256_finish(golioth_sys_sha256_t sha_ctx, uint8_t *output)
{
    if (!sha_ctx || !output)
    {
        return GOLIOTH_ERR_NULL;
    }

    struct golioth_hash *hash = sha_ctx;
    int err = mbedtls_sha256_finish(&hash->sha256_ctx, output);
    if (err) {
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

/* This function based on Zephry RTOS
 *
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2024 Golioth Inc.
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/zephyrproject-rtos/zephyr/blob/6f64f77557967a41ef7df429f0a831787bb8418f/lib/utils/hex.c#L12-L25
 */
static enum golioth_status char2hex(char c, uint8_t *x)
{
	if ((c >= '0') && (c <= '9')) {
		*x = c - '0';
	} else if ((c >= 'a') && (c <= 'f')) {
		*x = c - 'a' + 10;
	} else if ((c >= 'A') && (c <= 'F')) {
		*x = c - 'A' + 10;
	} else {
		return GOLIOTH_ERR_FAIL;
	}

	return GOLIOTH_OK;
}

/* This function based on Zephry RTOS
 *
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2024 Golioth Inc.
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/zephyrproject-rtos/zephyr/blob/6f64f77557967a41ef7df429f0a831787bb8418f/lib/utils/hex.c#L59-L91
 */
size_t golioth_sys_hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen)
{
    uint8_t dec;

    if (buflen < (hexlen / 2U + hexlen % 2U)) {
        return 0;
    }

    /* if hexlen is uneven, insert leading zero nibble */
    if ((hexlen % 2U) != 0) {
        if (char2hex(hex[0], &dec) < 0) {
            return 0;
        }
        buf[0] = dec;
        hex++;
        buf++;
    }

    /* regular hex conversion */
    for (size_t i = 0; i < (hexlen / 2U); i++) {
        if (char2hex(hex[2U * i], &dec) < 0) {
            return 0;
        }
        buf[i] = dec << 4;

        if (char2hex(hex[2U * i + 1U], &dec) < 0) {
            return 0;
        }
        buf[i] += dec;
    }

    return hexlen / 2U + hexlen % 2U;
}

/*--------------------------------------------------
 * Misc
 *------------------------------------------------*/

void golioth_sys_client_connected(void *client)
{
    if (CONFIG_GOLIOTH_AUTO_LOG_TO_CLOUD != 0)
    {
        golioth_debug_set_cloud_log_enabled(true);
    }
}

void golioth_sys_client_disconnected(void *client)
{
    golioth_debug_set_cloud_log_enabled(false);
}
