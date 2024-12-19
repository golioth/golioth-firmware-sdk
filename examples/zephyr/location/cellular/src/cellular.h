/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CELLULAR_H__
#define __CELLULAR_H__

#include <golioth/location/cellular.h>

int cellular_info_get(struct golioth_cellular_info *infos,
                      size_t num_max_infos,
                      size_t *num_returned_infos);

#endif /* __CELLULAR_H__ */
