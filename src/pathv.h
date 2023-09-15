/*
 * Copyright (c) 2022-2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zephyr/sys/util.h>

#define PATHV(...)                                                  \
    (const uint8_t *[])                                             \
    {                                                               \
        FOR_EACH(IDENTITY, (, ), __VA_ARGS__), NULL, /* sentinel */ \
    }
