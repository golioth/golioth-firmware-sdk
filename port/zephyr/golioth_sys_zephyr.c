#include <zephyr/kernel.h>

#include "golioth_sys.h"
#include "golioth_log_zephyr.h"

LOG_TAG_DEFINE(golioth_sys_zephyr);

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/

void golioth_sys_msleep(uint32_t ms) {
    k_msleep(ms);
}

uint64_t golioth_sys_now_ms(void) {
    return k_uptime_get();
}

/*--------------------------------------------------
 * Semaphores
 *------------------------------------------------*/

golioth_sys_sem_t golioth_sys_sem_create(uint32_t sem_max_count, uint32_t sem_initial_count) {
    struct k_sem* sem;
    int err;

    sem = golioth_sys_malloc(sizeof(*sem));
    if (!sem) {
        GLTH_LOGE(TAG, "sem malloc failed: %d", errno);
        return NULL;
    }

    err = k_sem_init(sem, sem_initial_count, sem_max_count);
    if (err) {
        GLTH_LOGE(TAG, "sem init failed (%d): %d", (int)sem_initial_count, err);
        golioth_sys_free(sem);
        return NULL;
    }

    return (golioth_sys_sem_t)sem;
}

bool golioth_sys_sem_take(golioth_sys_sem_t sem, int32_t ms_to_wait) {
    int err;

    err = k_sem_take(
            (struct k_sem*)sem, SYS_TIMEOUT_MS(ms_to_wait < 0 ? SYS_FOREVER_MS : ms_to_wait));

    return !err; /* What should be returned here? */
}

bool golioth_sys_sem_give(golioth_sys_sem_t sem) {
    k_sem_give((struct k_sem*)sem);
    return true;
}

void golioth_sys_sem_destroy(golioth_sys_sem_t sem) {
    golioth_sys_free(sem);
}

/*--------------------------------------------------
 * Software Timers
 *------------------------------------------------*/

struct golioth_timer {
    struct k_timer timer;
    golioth_sys_timer_config_t config;
};

static void on_timer(struct k_timer* ztimer) {
    struct golioth_timer* timer = CONTAINER_OF(ztimer, struct golioth_timer, timer);

    if (timer->config.fn) {
        timer->config.fn(timer, timer->config.user_arg);
    }
}

golioth_sys_timer_t golioth_sys_timer_create(golioth_sys_timer_config_t config) {
    struct golioth_timer* timer;

    timer = golioth_sys_malloc(sizeof(*timer));
    if (!timer) {
        return NULL;
    }

    timer->config = config;

    k_timer_init(&timer->timer, on_timer, NULL);

    return (golioth_sys_timer_t)timer;
}

bool golioth_sys_timer_start(golioth_sys_timer_t gtimer) {
    struct golioth_timer* timer = gtimer;

    k_timer_start(&timer->timer, K_MSEC(timer->config.expiration_ms), K_NO_WAIT);

    return true;
}

bool golioth_sys_timer_reset(golioth_sys_timer_t timer) {
    return golioth_sys_timer_start(timer);
}

void golioth_sys_timer_destroy(golioth_sys_timer_t gtimer) {
    struct golioth_timer* timer = gtimer;

    k_timer_stop(&timer->timer);
    golioth_sys_free(timer);
}

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/

// Wrap pthread due to incompatibility with thread function type.
//   pthread fn returns void*
//   golioth_sys_thread_fn_t returns void
struct golioth_thread {
    struct k_thread thread;
    k_tid_t tid;
    golioth_sys_thread_fn_t fn;
    void* user_arg;
};

K_THREAD_STACK_ARRAY_DEFINE(
        golioth_thread_stacks,
        CONFIG_GOLIOTH_ZEPHYR_THREAD_STACKS,
        CONFIG_GOLIOTH_ZEPHYR_THREAD_STACK_SIZE);
static atomic_t golioth_thread_idx;

static k_thread_stack_t* stack_alloc(void) {
    atomic_val_t idx = atomic_inc(&golioth_thread_idx);

    if (idx > ARRAY_SIZE(golioth_thread_stacks)) {
        return NULL;
    }

    return golioth_thread_stacks[idx];
}

static void golioth_thread_main(void* p1, void* p2, void* p3) {
    golioth_sys_thread_fn_t fn = p1;
    void* user_arg = p2;

    fn(user_arg);
}

golioth_sys_thread_t golioth_sys_thread_create(golioth_sys_thread_config_t config) {
    // Intentionally ignoring from config:
    //      name
    //      stack_size
    //      prio
    struct golioth_thread* thread = golioth_sys_malloc(sizeof(*thread));
    k_thread_stack_t* stack;

    stack = stack_alloc();
    if (!stack) {
        goto free_thread;
    }

    thread->tid = k_thread_create(
            &thread->thread,
            stack,
            CONFIG_GOLIOTH_ZEPHYR_THREAD_STACK_SIZE,
            golioth_thread_main,
            config.fn,
            config.user_arg,
            NULL,
            config.prio,
            0,
            K_NO_WAIT);
    if (config.name) {
        k_thread_name_set(thread->tid, config.name);
    }

    return (golioth_sys_thread_t)thread;

free_thread:
    golioth_sys_free(thread);

    return NULL;
}

void golioth_sys_thread_destroy(golioth_sys_thread_t gthread) {
    struct golioth_thread* thread = gthread;

    k_thread_abort(thread->tid);
    golioth_sys_free(thread);
}

/*--------------------------------------------------
 * Misc
 *------------------------------------------------*/

void golioth_sys_client_connected(void* client) {
    if (IS_ENABLED(CONFIG_LOG_BACKEND_GOLIOTH)) {
        log_backend_golioth_enable(client);
    }
}

void golioth_sys_client_disconnected(void* client) {
    if (IS_ENABLED(CONFIG_LOG_BACKEND_GOLIOTH)) {
        log_backend_golioth_disable(client);
    }
}
