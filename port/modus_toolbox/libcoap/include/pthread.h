#ifndef __LIBCOAP_PTHREAD_H__
#define __LIBCOAP_PTHREAD_H__

#include <errno.h>

#ifndef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER \
    { 0 }
#endif

static inline int pthread_mutex_lock(pthread_mutex_t* mutex) {
    return -ENOSYS;
}
static inline int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    return -ENOSYS;
}
static inline int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    return -ENOSYS;
}

#endif /* __LIBCOAP_PTHREAD_H__ */
