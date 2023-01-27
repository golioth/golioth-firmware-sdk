#ifndef __LIBCOAP_SYS_RANDOM_H__
#define __LIBCOAP_SYS_RANDOM_H__

#include <zephyr/random/rand32.h>

static inline ssize_t getrandom(void* buf, size_t buflen, unsigned int flags) {
    sys_rand_get(buf, buflen);

    return buflen;
}

#endif /* __LIBCOAP_SYS_RANDOM_H__ */
