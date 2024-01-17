/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ringbuf.h"
#include <string.h>

/// When ringbuf is empty, write_index == read_index.
/// When ringbuf is full, write_index == read_index - 1, modulo total_items

static size_t total_items(const ringbuf_t *ringbuf)
{
    return ringbuf->buffer_size / ringbuf->item_size;
}

static ringbuf_index_t next_write_index(const ringbuf_t *ringbuf)
{
    return (ringbuf->write_index + 1) % total_items(ringbuf);
}

bool ringbuf_is_empty(const ringbuf_t *ringbuf)
{
    return (ringbuf->write_index == ringbuf->read_index);
}

bool ringbuf_is_full(const ringbuf_t *ringbuf)
{
    ringbuf_index_t next_wr_index = (ringbuf->write_index + 1) % total_items(ringbuf);
    return (next_wr_index == ringbuf->read_index);
}

bool ringbuf_put(ringbuf_t *ringbuf, const void *item)
{
    if (!item)
    {
        return false;
    }

    if (ringbuf_is_full(ringbuf))
    {
        return false;
    }

    uint8_t *buffer_wr_ptr = ringbuf->buffer + ringbuf->write_index * ringbuf->item_size;
    memcpy(buffer_wr_ptr, item, ringbuf->item_size);
    ringbuf->write_index = next_write_index(ringbuf);

    return true;
}

bool ringbuf_get_internal(ringbuf_t *ringbuf, void *item, bool remove)
{
    if (ringbuf_is_empty(ringbuf))
    {
        return false;
    }

    if (item)
    {
        const uint8_t *buffer_rd_ptr = ringbuf->buffer + ringbuf->read_index * ringbuf->item_size;
        memcpy(item, buffer_rd_ptr, ringbuf->item_size);
    }

    if (remove)
    {
        ringbuf->read_index = (ringbuf->read_index + 1) % total_items(ringbuf);
    }

    return true;
}

bool ringbuf_get(ringbuf_t *ringbuf, void *item)
{
    return ringbuf_get_internal(ringbuf, item, true);
}

bool ringbuf_peek(ringbuf_t *ringbuf, void *item)
{
    return ringbuf_get_internal(ringbuf, item, false);
}

size_t ringbuf_size(const ringbuf_t *ringbuf)
{
    ringbuf_index_t write_index = ringbuf->write_index;
    ringbuf_index_t read_index = ringbuf->read_index;

    if (write_index > read_index)
    {
        return (write_index - read_index);
    }

    if (read_index > write_index)
    {
        return (write_index + total_items(ringbuf) - read_index);
    }

    // read_index == write_index
    return 0;
}

size_t ringbuf_capacity(const ringbuf_t *ringbuf)
{
    // When the ringbuf is full, write_index will be one before read_index,
    // meaning we will only be able to store a number of items that is one
    // less than total_items.
    return total_items(ringbuf) - 1;
}

void ringbuf_reset(ringbuf_t *ringbuf)
{
    ringbuf->write_index = 0;
    ringbuf->read_index = 0;
}
