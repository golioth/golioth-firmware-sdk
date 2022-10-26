#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "golioth_sys.h"

// Similar in concept to FreeRTOS event groups:
//  https://www.freertos.org/event-groups-API.html

struct golioth_event_group {
    // Event bitmap
    uint32_t bitmap;
    // Protects access to bitmap
    golioth_sys_sem_t bitmap_mutex;
    // Signals that bitmap changed
    golioth_sys_sem_t sem;
};
typedef struct golioth_event_group* golioth_event_group_t;

golioth_event_group_t golioth_event_group_create(void);
void golioth_event_group_set_bits(golioth_event_group_t eg, uint32_t bits_to_set);

// Returns the value of the bitmap at the time one of the
// bits to wait for was or timeout occurred.
//
// If clear_set_bits is true, the value returned will be the bitmap
// value before the bits were cleared.
uint32_t golioth_event_group_wait_bits(
        golioth_event_group_t eg,
        // Bitmap of bits to wait. Will return if/when any are set.
        uint32_t bits_to_wait_for,
        // If true, will automatically clear any set bits.
        bool clear_set_bits,
        // set to -1 to wait forever
        int32_t wait_timeout_ms);
void golioth_event_group_destroy(golioth_event_group_t);
