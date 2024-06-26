/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "event_group.h"
#include <golioth/golioth_sys.h>
#include <string.h>  // memset

golioth_event_group_t golioth_event_group_create(void)
{
    golioth_event_group_t eg =
        (golioth_event_group_t) golioth_sys_malloc(sizeof(struct golioth_event_group));
    if (!eg)
    {
        return NULL;
    }

    memset(eg, 0, sizeof(struct golioth_event_group));
    eg->bitmap = 0;

    eg->bitmap_mutex = golioth_sys_sem_create(1, 1);
    if (!eg->bitmap_mutex)
    {
        goto cleanup_eg;
    }

    eg->sem = golioth_sys_sem_create(1, 0);
    if (!eg->sem)
    {
        goto cleanup_eg_with_mutex;
    }

    return eg;

cleanup_eg_with_mutex:
    golioth_sys_sem_destroy(eg->bitmap_mutex);

cleanup_eg:
    golioth_sys_free(eg);
    return NULL;
}

void golioth_event_group_set_bits(golioth_event_group_t eg, uint32_t bits_to_set)
{
    // Update the bitmap
    golioth_sys_sem_take(eg->bitmap_mutex, GOLIOTH_SYS_WAIT_FOREVER);
    eg->bitmap |= (bits_to_set);
    golioth_sys_sem_give(eg->bitmap_mutex);

    // Signal that the bitmap has been changed
    golioth_sys_sem_give(eg->sem);
}

uint32_t golioth_event_group_wait_bits(golioth_event_group_t eg,
                                       uint32_t bits_to_wait_for,
                                       bool clear_set_bits,
                                       int32_t wait_timeout_ms)
{
    uint32_t bitmap = 0;

    while (1)
    {
        // Wait for the bitmap to change
        bool taken = golioth_sys_sem_take(eg->sem, wait_timeout_ms);

        golioth_sys_sem_take(eg->bitmap_mutex, GOLIOTH_SYS_WAIT_FOREVER);

        // Read the pre-cleared value of the bitmap
        bitmap = eg->bitmap;

        // Check if one of the bits_to_wait_for was set
        bool got_a_bit = (bits_to_wait_for & bitmap);

        if (got_a_bit && clear_set_bits)
        {
            // Clear the bits_to_wait_for
            eg->bitmap &= ~(bits_to_wait_for);
        }

        golioth_sys_sem_give(eg->bitmap_mutex);

        // Break out of loop on timeout or one of the bits was set
        if (!taken || got_a_bit)
        {
            break;
        }
    }

    return bitmap;
}

void golioth_event_group_destroy(golioth_event_group_t eg)
{
    if (!eg)
    {
        return;
    }
    golioth_sys_sem_destroy(eg->bitmap_mutex);
    golioth_sys_sem_destroy(eg->sem);
    golioth_sys_free(eg);
}
