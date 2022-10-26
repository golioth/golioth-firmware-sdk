#pragma once

#include <stdint.h>
#include <stdbool.h>

// For functions that take a wait/timeout parameter, -1 will wait forever
#define GOLIOTH_SYS_WAIT_FOREVER -1

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/
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

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/
