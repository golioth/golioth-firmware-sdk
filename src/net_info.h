/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <golioth/golioth_status.h>
#include <golioth/net_info.h>

#define GOLIOTH_NET_INFO_SHIFT_FINISHED 16
#define GOLIOTH_NET_INFO_STARTED_MASK GENMASK(GOLIOTH_NET_INFO_SHIFT_FINISHED - 1, 0)
#define GOLIOTH_NET_INFO_FINISHED_MASK \
    GENMASK(GOLIOTH_NET_INFO_SHIFT_FINISHED * 2 - 1, GOLIOTH_NET_INFO_SHIFT_FINISHED)

#define GOLIOTH_NET_INFO_FLAG_WIFI (1 << 0)
#define GOLIOTH_NET_INFO_FLAG_CELLULAR (1 << 1)

struct golioth_net_info
{
    uint8_t buf[CONFIG_GOLIOTH_NET_INFO_BUFFER_SIZE];
    zcbor_state_t zse[2 /* backup */ + 1 /* WiFi only */];
    int flags;
};

enum golioth_status golioth_net_info_append(struct golioth_net_info *info,
                                            int flag,
                                            const char *name);
