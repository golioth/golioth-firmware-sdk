/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <golioth/golioth_status.h>

#define GENERATE_GOLIOTH_STATUS_STR(code) #code,
static const char* _status_strings[NUM_GOLIOTH_STATUS_CODES] = {
        FOREACH_GOLIOTH_STATUS(GENERATE_GOLIOTH_STATUS_STR)};

const char* golioth_status_to_str(enum golioth_status status) {
    assert(status < NUM_GOLIOTH_STATUS_CODES);
    return _status_strings[status];
}
