/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __ZCBOR_ANY_SKIP_FIXED_H__
#define __ZCBOR_ANY_SKIP_FIXED_H__

#if defined(ZCBOR_VARIANCE) && ZCBOR_VERSION_MAJOR == 0 && ZCBOR_VERSION_MINOR < 7

bool zcbor_any_skip_fixed(zcbor_state_t *state, void *result);

#define zcbor_any_skip(state, result) zcbor_any_skip_fixed(state, result)

#endif

#endif /* __ZCBOR_ANY_SKIP_FIXED_H__ */
