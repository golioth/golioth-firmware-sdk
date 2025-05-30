/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/sys/eventfd.h>

#include <mbedtls/sha256.h>

#include <golioth/golioth_sys.h>
#include <golioth/golioth_status.h>
#include "golioth_log_zephyr.h"

LOG_TAG_DEFINE(golioth_sys_zephyr);

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/

void golioth_sys_msleep(uint32_t ms)
{
    k_msleep(ms);
}

uint64_t golioth_sys_now_ms(void)
{
    return k_uptime_get();
}

/*--------------------------------------------------
 * Mutexes
 *------------------------------------------------*/

golioth_sys_mutex_t golioth_sys_mutex_create(void)
{
    struct k_mutex *mutex = golioth_sys_malloc(sizeof(struct k_mutex));
    k_mutex_init(mutex);
    return (golioth_sys_mutex_t) mutex;
}

bool golioth_sys_mutex_lock(golioth_sys_mutex_t mutex, int32_t ms_to_wait)
{
    k_timeout_t timeout = (ms_to_wait == GOLIOTH_SYS_WAIT_FOREVER) ? K_FOREVER : K_MSEC(ms_to_wait);
    return (0 == k_mutex_lock(mutex, timeout));
}

bool golioth_sys_mutex_unlock(golioth_sys_mutex_t mutex)
{
    return (0 == k_mutex_unlock(mutex));
}

void golioth_sys_mutex_destroy(golioth_sys_mutex_t mutex)
{
    golioth_sys_free(mutex);
}

/*--------------------------------------------------
 * Semaphores
 *------------------------------------------------*/

#define SEM_TO_FD(sem) ((int) (intptr_t) (sem))
#define FD_TO_SEM(sem) ((golioth_sys_sem_t) (intptr_t) (fd))

golioth_sys_sem_t golioth_sys_sem_create(uint32_t sem_max_count, uint32_t sem_initial_count)
{
    int fd;

    fd = eventfd(sem_initial_count, EFD_SEMAPHORE);
    if (fd < 0)
    {
        GLTH_LOGE(TAG, "eventfd creation failed, errno: %d", errno);
        return NULL;
    }

    return FD_TO_SEM(fd);
}

bool golioth_sys_sem_take(golioth_sys_sem_t sem, int32_t ms_to_wait)
{
    int fd = SEM_TO_FD(sem);
    while (true)
    {
        struct zsock_pollfd pfd = {
            .fd = fd,
            .events = ZSOCK_POLLIN,
        };
        eventfd_t val;
        int ret;

        ret = zsock_poll(&pfd, 1, ms_to_wait);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                GLTH_LOGI(TAG, "EINTR");
                continue;
            }
            GLTH_LOGE(TAG, "sem poll failed, errno: %d", errno);
            return false;
        }
        else if (ret == 0)
        {
            return false;
        }

        ret = eventfd_read(fd, &val);
        if (ret < 0)
        {
            GLTH_LOGE(TAG, "sem eventfd_read failed, errno: %d", errno);
            return false;
        }

        break;
    }

    return true;
}

bool golioth_sys_sem_give(golioth_sys_sem_t sem)
{
    return (0 == eventfd_write(SEM_TO_FD(sem), 1));
}

void golioth_sys_sem_destroy(golioth_sys_sem_t sem)
{
    zsock_close(SEM_TO_FD(sem));
}

int golioth_sys_sem_get_fd(golioth_sys_sem_t sem)
{
    return SEM_TO_FD(sem);
}

/*--------------------------------------------------
 * Software Timers
 *------------------------------------------------*/

struct golioth_timer
{
    struct k_work_delayable work;
    struct golioth_timer_config config;
};

static void timer_worker(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct golioth_timer *timer = CONTAINER_OF(dwork, struct golioth_timer, work);

    if (timer->config.fn)
    {
        timer->config.fn(timer, timer->config.user_arg);
    }
}

golioth_sys_timer_t golioth_sys_timer_create(const struct golioth_timer_config *config)
{
    struct golioth_timer *timer;

    timer = golioth_sys_malloc(sizeof(*timer));
    if (!timer)
    {
        return NULL;
    }

    memcpy(&timer->config, config, sizeof(timer->config));

    k_work_init_delayable(&timer->work, timer_worker);

    return (golioth_sys_timer_t) timer;
}

bool golioth_sys_timer_start(golioth_sys_timer_t gtimer)
{
    struct golioth_timer *timer = gtimer;

    int ret = k_work_schedule(&timer->work, K_MSEC(timer->config.expiration_ms));

    return ret > 0;
}

bool golioth_sys_timer_reset(golioth_sys_timer_t gtimer)
{
    struct golioth_timer *timer = gtimer;

    int ret = k_work_reschedule(&timer->work, K_MSEC(timer->config.expiration_ms));

    return ret >= 0;
}

void golioth_sys_timer_destroy(golioth_sys_timer_t gtimer)
{
    struct golioth_timer *timer = gtimer;

    k_work_cancel_delayable(&timer->work);

    golioth_sys_free(timer);
}

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/

// Wrap pthread due to incompatibility with thread function type.
//   pthread fn returns void*
//   golioth_sys_thread_fn_t returns void
struct golioth_thread
{
    struct k_thread thread;
    k_tid_t tid;
    golioth_sys_thread_fn_t fn;
    void *user_arg;
};

K_THREAD_STACK_ARRAY_DEFINE(golioth_thread_stacks,
                            CONFIG_GOLIOTH_ZEPHYR_THREAD_STACKS,
                            CONFIG_GOLIOTH_ZEPHYR_THREAD_STACK_SIZE);
static atomic_t golioth_thread_idx;

static k_thread_stack_t *stack_alloc(void)
{
    atomic_val_t idx = atomic_inc(&golioth_thread_idx);

    if (idx > ARRAY_SIZE(golioth_thread_stacks))
    {
        return NULL;
    }

    return golioth_thread_stacks[idx];
}

static void golioth_thread_main(void *p1, void *p2, void *p3)
{
    golioth_sys_thread_fn_t fn = p1;
    void *user_arg = p2;

    fn(user_arg);
}

golioth_sys_thread_t golioth_sys_thread_create(const struct golioth_thread_config *config)
{
    // Intentionally ignoring from config:
    //      name
    //      stack_size
    //      prio
    struct golioth_thread *thread = golioth_sys_malloc(sizeof(*thread));
    k_thread_stack_t *stack;

    stack = stack_alloc();
    if (!stack)
    {
        goto free_thread;
    }

    thread->tid = k_thread_create(&thread->thread,
                                  stack,
                                  CONFIG_GOLIOTH_ZEPHYR_THREAD_STACK_SIZE,
                                  golioth_thread_main,
                                  config->fn,
                                  config->user_arg,
                                  NULL,
                                  config->prio,
                                  0,
                                  K_NO_WAIT);
    if (config->name)
    {
        k_thread_name_set(thread->tid, config->name);
    }

    return (golioth_sys_thread_t) thread;

free_thread:
    golioth_sys_free(thread);

    return NULL;
}

void golioth_sys_thread_destroy(golioth_sys_thread_t gthread)
{
    struct golioth_thread *thread = gthread;

    k_thread_abort(thread->tid);
    golioth_sys_free(thread);
}

/*--------------------------------------------------
 * Hash
 *------------------------------------------------*/

golioth_sys_sha256_t golioth_sys_sha256_create(void)
{
    mbedtls_sha256_context *hash = golioth_sys_malloc(sizeof(mbedtls_sha256_context));
    if (!hash)
    {
        return NULL;
    }

    mbedtls_sha256_init(hash);
    mbedtls_sha256_starts(hash, 0);

    return (golioth_sys_sha256_t) hash;
}

void golioth_sys_sha256_destroy(golioth_sys_sha256_t sha_ctx)
{
    golioth_sys_free(sha_ctx);
}

enum golioth_status golioth_sys_sha256_update(golioth_sys_sha256_t sha_ctx,
                                              const uint8_t *input,
                                              size_t len)
{
    if (!sha_ctx || !input)
    {
        return GOLIOTH_ERR_NULL;
    }

    mbedtls_sha256_context *hash = sha_ctx;
    int err = mbedtls_sha256_update(hash, input, len);
    if (err)
    {
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

    mbedtls_sha256_context *hash = sha_ctx;
    int err = mbedtls_sha256_finish(hash, output);
    if (err)
    {
        return GOLIOTH_ERR_FAIL;
    }

    return GOLIOTH_OK;
}

size_t golioth_sys_hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen)
{
    return hex2bin(hex, hexlen, buf, buflen);
}

/*--------------------------------------------------
 * Misc
 *------------------------------------------------*/

void golioth_sys_client_connected(void *client)
{
    if (IS_ENABLED(CONFIG_LOG_BACKEND_GOLIOTH))
    {
        log_backend_golioth_enable(client);
    }
}

void golioth_sys_client_disconnected(void *client)
{
    if (IS_ENABLED(CONFIG_LOG_BACKEND_GOLIOTH))
    {
        log_backend_golioth_disable(client);
    }
}
