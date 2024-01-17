/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <golioth/payload_utils.h>

int32_t golioth_payload_as_int(const uint8_t *payload, size_t payload_size)
{
    // Copy payload to a NULL-terminated string
    char value[12] = {};
    assert(payload_size <= sizeof(value));
    memcpy(value, payload, payload_size);

    return strtol(value, NULL, 10);
}

float golioth_payload_as_float(const uint8_t *payload, size_t payload_size)
{
    // Copy payload to a NULL-terminated string
    char value[32] = {};
    assert(payload_size <= sizeof(value));
    memcpy(value, payload, payload_size);

    return strtof(value, NULL);
}

bool golioth_payload_as_bool(const uint8_t *payload, size_t payload_size)
{
    if (payload_size < 4)
    {
        return false;
    }
    return (0 == strncmp((const char *) payload, "true", 4));
}

bool golioth_payload_is_null(const uint8_t *payload, size_t payload_size)
{
    if (!payload || payload_size == 0)
    {
        return true;
    }
    if (payload_size >= 4)
    {
        if (strncmp((const char *) payload, "null", 4) == 0)
        {
            return true;
        }
    }
    return false;
}
