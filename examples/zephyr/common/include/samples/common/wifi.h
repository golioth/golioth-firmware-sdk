/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GOLIOTH_INCLUDE_GOLIOTH_WIFI_H__
#define __GOLIOTH_INCLUDE_GOLIOTH_WIFI_H__

#include <zephyr/net/net_if.h>

/**
 * @defgroup wifi Golioth Wifi
 * @ingroup net
 * @{
 */

void wifi_connect(struct net_if* iface);

/** @} */

#endif /* __GOLIOTH_INCLUDE_GOLIOTH_WIFI_H__ */
