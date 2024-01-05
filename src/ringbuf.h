/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// This type must be atomic. If you are on an 8-bit CPU, you would change the type to uint8_t.
typedef uint32_t ringbuf_index_t;

typedef struct {
    volatile ringbuf_index_t write_index;
    volatile ringbuf_index_t read_index;
    uint8_t* buffer;
    size_t buffer_size;
    size_t item_size;
} ringbuf_t;

#define RINGBUF_BUFFER_SIZE(item_sz, max_num_items) (item_sz * (max_num_items + 1))

// Convenience macro that defines two variables in the current scope:
//      uint8_t <name>_buffer; // internal use only
//      ringbuf_t <name>;      // user's initialized ringbuf_t
#define RINGBUF_DEFINE(name, item_sz, max_num_items) \
    uint8_t name##_buffer[RINGBUF_BUFFER_SIZE(item_sz, max_num_items)]; \
    ringbuf_t name = { \
            .buffer = name##_buffer, \
            .buffer_size = RINGBUF_BUFFER_SIZE(item_sz, max_num_items), \
            .item_size = item_sz};

bool ringbuf_put(ringbuf_t* ringbuf, const void* item);
bool ringbuf_get(ringbuf_t* ringbuf, void* item);
bool ringbuf_peek(ringbuf_t* ringbuf, void* item);
size_t ringbuf_size(const ringbuf_t* ringbuf);
bool ringbuf_is_empty(const ringbuf_t* ringbuf);
bool ringbuf_is_full(const ringbuf_t* ringbuf);
size_t ringbuf_capacity(const ringbuf_t* ringbuf);
void ringbuf_reset(ringbuf_t* ringbuf);
