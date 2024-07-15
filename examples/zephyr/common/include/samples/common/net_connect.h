/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __GOLIOTH_INCLUDE_GOLIOTH_NET_CONNECT_H__
#define __GOLIOTH_INCLUDE_GOLIOTH_NET_CONNECT_H__

/**
 * @defgroup net_connect Golioth Net Connect
 * @ingroup net
 * @{
 */

#ifdef CONFIG_NET_CONFIG_NEED_IPV4
static inline void net_connect(void) {}
#else
void net_connect(void);
#endif

/** @} */

#endif /* __GOLIOTH_INCLUDE_GOLIOTH_NET_CONNECT_H__ */

#ifdef __cplusplus
}
#endif
