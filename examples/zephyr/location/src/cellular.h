/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <golioth/net_info/cellular.h>

int cellular_info_get(struct golioth_cellular_info *infos,
                      size_t num_max_infos,
                      size_t *num_returned_infos);
