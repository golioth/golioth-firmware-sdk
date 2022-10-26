#include "golioth_sys.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

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

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/
