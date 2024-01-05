/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LIBCOAP_SYS_RANDOM_H__
#define __LIBCOAP_SYS_RANDOM_H__

#include <zephyr/random/random.h>

static inline ssize_t getrandom(void* buf, size_t buflen, unsigned int flags) {
    sys_rand_get(buf, buflen);

    return buflen;
}

#endif /* __LIBCOAP_SYS_RANDOM_H__ */
