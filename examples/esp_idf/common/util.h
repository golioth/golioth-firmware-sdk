/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t) (!(sizeof(x) % sizeof(0 [x])))))

#ifdef __cplusplus
}
#endif
