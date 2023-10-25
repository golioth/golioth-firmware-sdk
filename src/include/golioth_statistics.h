/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/// Statistics internal to the Golioth SDK, for debug and troubleshoot of the SDK itself.
#pragma once

#include <stdbool.h>

bool golioth_statistics_has_allocation_leaks(void);
void golioth_statistics_increment_alloc(const char* name);
void golioth_statistics_increment_free(const char* name);

#if (CONFIG_GOLIOTH_ALLOCATION_TRACKING == 1)

#define GSTATS_INC_ALLOC(name) golioth_statistics_increment_alloc(name)
#define GSTATS_INC_FREE(name) golioth_statistics_increment_free(name)

#else  // CONFIG_GOLIOTH_ALLOCATION_TRACKING

#define GSTATS_INC_ALLOC(name)
#define GSTATS_INC_FREE(name)

#endif  // CONFIG_GOLIOTH_ALLOCATION_TRACKING
