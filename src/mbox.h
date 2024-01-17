/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/golioth_sys.h>
#include "ringbuf.h"

/// A multi-producer, single-consumer queue.
///
/// This is basically a ringbuffer+semaphore+mutex. The semaphore is for
/// signaling when queue has items, so the consumer can be efficiently notified.
/// The mutex is for preventing multiple producers from accessing the ringbuffer
/// at once.

struct golioth_mbox {
    ringbuf_t ringbuf;
    golioth_sys_sem_t fill_count_sem;
    golioth_sys_sem_t ringbuf_mutex;
};
typedef struct golioth_mbox *golioth_mbox_t;

golioth_mbox_t golioth_mbox_create(size_t num_items, size_t item_size);
size_t golioth_mbox_num_messages(golioth_mbox_t mbox);
bool golioth_mbox_try_send(golioth_mbox_t mbox, const void *item);
bool golioth_mbox_recv(golioth_mbox_t mbox, void *item, int32_t timeout_ms);
void golioth_mbox_destroy(golioth_mbox_t mbox);
