/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "golioth_statistics.h"
#include "golioth_debug.h"
#include <stdint.h>
#include <stdlib.h>

#define TAG "golioth_statistics"

#if (CONFIG_GOLIOTH_ALLOCATION_TRACKING == 1)

#define GOLIOTH_STATS_MAX_NUM_ALLOCATIONS 100

typedef struct {
    /// Identifier/tag for the allocation
    const char* name;
    int32_t num_allocs;
    int32_t num_frees;
} golioth_allocation_t;

static golioth_allocation_t _allocations[GOLIOTH_STATS_MAX_NUM_ALLOCATIONS];
static size_t _num_allocations;

static golioth_allocation_t* find_allocation(const char* name) {
    golioth_allocation_t* found = NULL;
    for (size_t i = 0; i < _num_allocations; i++) {
        golioth_allocation_t* a = &_allocations[i];
        if (a->name == name) {
            found = a;
            break;
        }
    }
    return found;
}

void golioth_statistics_increment_alloc(const char* name) {
    golioth_allocation_t* existing = find_allocation(name);
    if (existing) {
        existing->num_allocs++;
        return;
    }

    if (_num_allocations >= GOLIOTH_STATS_MAX_NUM_ALLOCATIONS) {
        // No more room for allocations, just drop it
        static bool logged_warning = false;
        if (!logged_warning) {
            GLTH_LOGW(TAG, "Ran out of space for tracking allocations");
            logged_warning = true;
        }
        return;
    }

    golioth_allocation_t* new = &_allocations[_num_allocations++];
    new->name = name;
    new->num_allocs = 1;
    new->num_frees = 0;
}

void golioth_statistics_increment_free(const char* name) {
    golioth_allocation_t* existing = find_allocation(name);
    assert(existing);
    existing->num_frees++;
}

bool golioth_statistics_has_allocation_leaks(void) {
    bool any_alloc_has_leak = false;

    for (size_t i = 0; i < _num_allocations; i++) {
        golioth_allocation_t* a = &_allocations[i];

        bool alloc_has_leak = (a->num_allocs != a->num_frees);
        any_alloc_has_leak |= alloc_has_leak;

        GLTH_LOGD(
                TAG,
                "%-32s: %d %d%s",
                a->name,
                a->num_allocs,
                a->num_frees,
                alloc_has_leak ? " (leak)" : "");
    }

    return any_alloc_has_leak;
}

#else  // CONFIG_GOLIOTH_ALLOCATION_TRACKING

void golioth_statistics_increment_alloc(const char* name) {}

void golioth_statistics_increment_free(const char* name) {}

bool golioth_statistics_has_allocation_leaks(void) {
    return true;
}

#endif  // CONFIG_GOLIOTH_ALLOCATION_TRACKING
