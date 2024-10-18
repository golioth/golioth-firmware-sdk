/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/golioth_sys.h>
#include <golioth/golioth_status.h>
#include <assert.h>
#include <errno.h>
#include <openssl/evp.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>
#include "../utils/hex.h"


#define TAG "golioth_sys_linux"

/*--------------------------------------------------
 * Time
 *------------------------------------------------*/

void golioth_sys_msleep(uint32_t ms)
{
    struct timespec to_sleep = {ms / 1000, (ms % 1000) * 1000000};
    while ((nanosleep(&to_sleep, &to_sleep) == -1) && (errno == EINTR))
        ;
}

uint64_t golioth_sys_now_ms(void)
{
    // Return time relative to when the program started
    static struct timespec start_spec;
    static uint64_t start_ms;
    if (!start_spec.tv_sec)
    {
        clock_gettime(CLOCK_REALTIME, &start_spec);
        start_ms = (start_spec.tv_sec * 1000 + start_spec.tv_nsec / 1000000);
    }

    struct timespec now_spec;
    clock_gettime(CLOCK_REALTIME, &now_spec);
    uint64_t now_ms = (now_spec.tv_sec * 1000 + now_spec.tv_nsec / 1000000);

    return now_ms - start_ms;
}

/*--------------------------------------------------
 * Mutexes
 *------------------------------------------------*/

golioth_sys_mutex_t golioth_sys_mutex_create(void)
{
    pthread_mutex_t *mutex = golioth_sys_malloc(sizeof(pthread_mutex_t));
    if (!mutex)
    {
        GLTH_LOGE(TAG, "Failed to allocate memory for mutex");
        return NULL;
    }

    int err = pthread_mutex_init(mutex, NULL);
    if (err)
    {
        GLTH_LOGE(TAG, "Failed to initialize mutex");
        golioth_sys_free(mutex);
        return NULL;
    }

    return (golioth_sys_mutex_t) mutex;
}

bool golioth_sys_mutex_lock(golioth_sys_mutex_t mutex, int32_t ms_to_wait)
{

    if (ms_to_wait <= GOLIOTH_SYS_WAIT_FOREVER)
    {
        return (0 == pthread_mutex_lock(mutex));
    }

    struct timespec abstime;
    timespec_get(&abstime, TIME_UTC);

    if (ms_to_wait)
    {
        abstime.tv_sec += ms_to_wait / 1000;
        abstime.tv_nsec += (ms_to_wait % 1000) * 1000000;
    }

    return (0 == pthread_mutex_timedlock(mutex, &abstime));
}

bool golioth_sys_mutex_unlock(golioth_sys_mutex_t mutex)
{
    return (0 == pthread_mutex_unlock(mutex));
}

void golioth_sys_mutex_destroy(golioth_sys_mutex_t mutex)
{
    pthread_mutex_destroy(mutex);
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
        assert(false);
        return NULL;
    }

    return FD_TO_SEM(fd);
}

bool golioth_sys_sem_take(golioth_sys_sem_t sem, int32_t ms_to_wait)
{
    int fd = SEM_TO_FD(sem);
    while (true)
    {
        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN,
        };
        eventfd_t val;
        int ret;

        ret = poll(&pfd, 1, ms_to_wait);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                GLTH_LOGI(TAG, "EINTR");
                continue;
            }
            GLTH_LOGE(TAG, "sem poll failed, errno: %d", errno);
            assert(false);
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
            assert(false);
            return false;
        }

        assert(val == 1);
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
    close(SEM_TO_FD(sem));
}

int golioth_sys_sem_get_fd(golioth_sys_sem_t sem)
{
    return SEM_TO_FD(sem);
}

/*--------------------------------------------------
 * Software Timers
 *------------------------------------------------*/

// Wrap timer_t to also capture user's config
typedef struct
{
    timer_t timer;
    struct golioth_timer_config config;
} wrapped_timer_t;

static void on_timer(int sig, siginfo_t *si, void *uc)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) si->si_value.sival_ptr;
    if (wt->config.fn)
    {
        wt->config.fn(wt, wt->config.user_arg);
    }
}

golioth_sys_timer_t golioth_sys_timer_create(const struct golioth_timer_config *config)
{
    static bool initialized = false;
    const int signo = SIGRTMIN;

    if (!initialized)
    {
        // Install the signal handler
        struct sigaction sa = {
            .sa_flags = SA_SIGINFO,
            .sa_sigaction = on_timer,
        };
        sigemptyset(&sa.sa_mask);
        if (sigaction(signo, &sa, NULL) < 0)
        {
            GLTH_LOGE(TAG, "sigaction errno: %d", errno);
            return NULL;
        }

        initialized = true;
    }

    // Note: config.name is unused
    wrapped_timer_t *wt = (wrapped_timer_t *) golioth_sys_malloc(sizeof(wrapped_timer_t));
    memcpy(&wt->config, config, sizeof(wt->config));
    int err = timer_create(CLOCK_REALTIME,
                           &(struct sigevent){
                               .sigev_notify = SIGEV_SIGNAL,
                               .sigev_signo = signo,
                               .sigev_value.sival_ptr = wt,
                           },
                           &wt->timer);
    if (err)
    {
        goto error;
    }

    struct itimerspec spec = {
        .it_interval =
            {
                .tv_sec = config->expiration_ms / 1000,
                .tv_nsec = (config->expiration_ms % 1000) * 1000000,
            },
    };

    err = timer_settime(wt->timer, 0, &spec, NULL);
    if (err)
    {
        goto error;
    }

    return (golioth_sys_timer_t) wt;

error:
    GLTH_LOGE(TAG, "timer_create errno: %d", errno);
    golioth_sys_free(wt);
    return NULL;
}

bool golioth_sys_timer_start(golioth_sys_timer_t timer)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) timer;

    struct timespec spec = {
        .tv_sec = wt->config.expiration_ms / 1000,
        .tv_nsec = (wt->config.expiration_ms % 1000) * 1000000,
    };

    struct itimerspec ispec = {
        .it_interval = spec,
        .it_value = spec,
    };

    int err = timer_settime(wt->timer, 0, &ispec, NULL);
    if (err)
    {
        GLTH_LOGE(TAG, "timer_start errno: %d", errno);
        return false;
    }

    return true;
}

bool golioth_sys_timer_reset(golioth_sys_timer_t timer)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) timer;

    struct timespec spec = {
        .tv_sec = wt->config.expiration_ms / 1000,
        .tv_nsec = (wt->config.expiration_ms % 1000) * 1000000,
    };

    struct itimerspec ispec = {
        .it_interval = spec,
        // disarm by setting it_value to zero
    };


    int err = timer_settime(wt->timer, 0, &ispec, NULL);
    if (err)
    {
        GLTH_LOGE(TAG, "timer_reset disarm errno: %d", errno);
        return false;
    }

    return golioth_sys_timer_start(wt);
}

void golioth_sys_timer_destroy(golioth_sys_timer_t timer)
{
    wrapped_timer_t *wt = (wrapped_timer_t *) timer;
    if (!wt)
    {
        return;
    }
    timer_delete(wt->timer);
    golioth_sys_free(wt);
}

/*--------------------------------------------------
 * Threads
 *------------------------------------------------*/

// Wrap pthread due to incompatibility with thread function type.
//   pthread fn returns void*
//   golioth_sys_thread_fn_t returns void
typedef struct
{
    pthread_t pthread;
    golioth_sys_thread_fn_t fn;
    void *user_arg;
} wrapped_pthread_t;

static void *pthread_callback(void *arg)
{
    wrapped_pthread_t *wt = (wrapped_pthread_t *) arg;
    assert(wt);
    assert(wt->fn);
    wt->fn(wt->user_arg);
}

golioth_sys_thread_t golioth_sys_thread_create(const struct golioth_thread_config *config)
{
    // Intentionally ignoring from config:
    //      name
    //      stack_size
    //      prio
    wrapped_pthread_t *wt = (wrapped_pthread_t *) golioth_sys_malloc(sizeof(wrapped_pthread_t));

    wt->fn = config->fn;
    wt->user_arg = config->user_arg;

    int err = pthread_create(&wt->pthread, NULL, pthread_callback, wt);
    if (err)
    {
        GLTH_LOGE(TAG, "pthread_create errno: %d", errno);
        golioth_sys_free(wt);
        assert(false);
        return NULL;
    }

    return (golioth_sys_thread_t) wt;
}

void golioth_sys_thread_destroy(golioth_sys_thread_t thread)
{
    // Do nothing.
    //
    // Thread will be automatically destroyed when/if the parent
    // process exits.
}

/*--------------------------------------------------
 * Hash
 *------------------------------------------------*/

golioth_sys_sha256_t golioth_sys_sha256_create(void)
{
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx)
    {
        return NULL;
    }

    EVP_MD *md = EVP_MD_fetch(NULL, "SHA2-256", NULL);
    EVP_DigestInit_ex2(mdctx, md, NULL);

    return (golioth_sys_sha256_t) mdctx;
}

void golioth_sys_sha256_destroy(golioth_sys_sha256_t sha_ctx)
{
    if (!sha_ctx)
    {
        return;
    }

    EVP_MD_CTX *mdctx = sha_ctx;
    EVP_MD_CTX_free(mdctx);
}

enum golioth_status golioth_sys_sha256_update(golioth_sys_sha256_t sha_ctx,
                                              const uint8_t *input,
                                              size_t len)
{
    if (!sha_ctx || !input)
    {
        return GOLIOTH_ERR_NULL;
    }

    EVP_MD_CTX *mdctx = sha_ctx;
    int err = EVP_DigestUpdate(mdctx, input, len);
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

    EVP_MD_CTX *mdctx = sha_ctx;
    int err = EVP_DigestFinal_ex(mdctx, output, NULL);
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

void golioth_sys_client_connected(void *client) {}

void golioth_sys_client_disconnected(void *client) {}
