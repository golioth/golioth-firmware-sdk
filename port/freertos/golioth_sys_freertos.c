#include "golioth_sys.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <stdlib.h>

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/
uint64_t golioth_sys_now_ms() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void golioth_sys_msleep(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

/*--------------------------------------------------
 * Semaphores
 *------------------------------------------------*/
golioth_sys_sem_t golioth_sys_sem_create(uint32_t sem_max_count, uint32_t sem_initial_count) {
    SemaphoreHandle_t sem = xSemaphoreCreateCounting(sem_max_count, sem_initial_count);
    return (golioth_sys_sem_t)sem;
}

bool golioth_sys_sem_take(golioth_sys_sem_t sem, int32_t ms_to_wait) {
    TickType_t ticks_to_wait =
            (ms_to_wait < 0 ? portMAX_DELAY : (uint32_t)ms_to_wait / portTICK_PERIOD_MS);
    return (pdTRUE == xSemaphoreTake((SemaphoreHandle_t)sem, ticks_to_wait));
}

bool golioth_sys_sem_give(golioth_sys_sem_t sem) {
    return (pdTRUE == xSemaphoreGive((SemaphoreHandle_t)sem));
}

void golioth_sys_sem_destroy(golioth_sys_sem_t sem) {
    vSemaphoreDelete((SemaphoreHandle_t)sem);
}

/*--------------------------------------------------
 * Software Timers
 *------------------------------------------------*/

// Create a wrapper struct to workaround awkwardness of FreeRTOS
// timer callbacks not having a separate user_arg param (it's
// embedded inside of the TimerHandle_t as the "TimerID").
//
// This wrapped timer is the underlying type of opaque golioth_sys_timer_t.
typedef struct {
    TimerHandle_t timer;
    golioth_sys_timer_fn_t fn;
    void* user_arg;
} wrapped_timer_t;

// Same callback used for all timers. We extract the user arg from
// the underlying freertos timer and call the callback with it.
static void freertos_timer_callback(TimerHandle_t xTimer) {
    wrapped_timer_t* wt = (wrapped_timer_t*)pvTimerGetTimerID(xTimer);
    wt->fn(wt->timer, wt->user_arg);
}

static TickType_t ms_to_ticks(uint32_t ms) {
    // Round to the nearest multiple of the tick period
    uint32_t remainder = ms % portTICK_PERIOD_MS;
    uint32_t rounded_ms = ms;
    if (remainder != 0) {
        rounded_ms = ms + portTICK_PERIOD_MS - remainder;
    }
    return (rounded_ms / portTICK_PERIOD_MS);
}

golioth_sys_timer_t golioth_sys_timer_create(golioth_sys_timer_config_t config) {
    assert(config.fn);  // timer callback function is required

    wrapped_timer_t* wrapped_timer = (wrapped_timer_t*)malloc(sizeof(wrapped_timer_t));

    TimerHandle_t timer = xTimerCreate(
            config.name,
            ms_to_ticks(config.expiration_ms),
            (config.type == GOLIOTH_SYS_TIMER_PERIODIC ? pdTRUE : pdFALSE),
            wrapped_timer,
            freertos_timer_callback);

    wrapped_timer->timer = timer;
    wrapped_timer->fn = config.fn;
    wrapped_timer->user_arg = config.user_arg;

    return wrapped_timer;
}

bool golioth_sys_timer_start(golioth_sys_timer_t timer) {
    wrapped_timer_t* wt = (wrapped_timer_t*)timer;
    if (!wt) {
        return false;
    }
    return (pdPASS == xTimerStart((TimerHandle_t)wt->timer, 0));  // non-blocking
}

bool golioth_sys_timer_reset(golioth_sys_timer_t timer) {
    wrapped_timer_t* wt = (wrapped_timer_t*)timer;
    if (!wt) {
        return false;
    }
    return (pdPASS == xTimerReset((TimerHandle_t)wt->timer, 0));  // non-blocking
}

void golioth_sys_timer_destroy(golioth_sys_timer_t timer) {
    wrapped_timer_t* wt = (wrapped_timer_t*)timer;
    if (!wt) {
        return;
    }
    xTimerDelete(wt->timer, 0);  // non-blocking
    free(wt);
}

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/

golioth_sys_thread_t golioth_sys_thread_create(golioth_sys_thread_config_t config) {
    TaskHandle_t task_handle = NULL;
    xTaskCreate(
            config.fn, config.name, config.stack_size, config.user_arg, config.prio, &task_handle);
    return (golioth_sys_thread_t)task_handle;
}

void golioth_sys_thread_destroy(golioth_sys_thread_t thread) {
    vTaskDelete((TaskHandle_t)thread);
}
