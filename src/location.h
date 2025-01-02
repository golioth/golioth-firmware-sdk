/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <golioth/golioth_status.h>
#include <golioth/location.h>

#define GOLIOTH_LOCATION_SHIFT_FINISHED 16
#define GOLIOTH_LOCATION_STARTED_MASK GENMASK(GOLIOTH_LOCATION_SHIFT_FINISHED - 1, 0)
#define GOLIOTH_LOCATION_FINISHED_MASK \
    GENMASK(GOLIOTH_LOCATION_SHIFT_FINISHED * 2 - 1, GOLIOTH_LOCATION_SHIFT_FINISHED)

#define GOLIOTH_LOCATION_FLAG_WIFI (1 << 0)
#define GOLIOTH_LOCATION_FLAG_CELLULAR (1 << 1)

enum golioth_status golioth_location_append(struct golioth_location_req *req,
                                            int flag,
                                            const char *name);
