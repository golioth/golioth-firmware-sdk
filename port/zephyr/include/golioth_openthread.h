/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GOLIOTH_OPENTHREAD_H__
#define __GOLIOTH_OPENTHREAD_H__

/**
 * @brief Synthesize IPv6 address from a given host name
 *
 * Get the IPv6 address of Golioth Server to avoid hardcoding it in applications.
 * NAT64 prefix used by the Thread Border Router is set while synthesizing the address.
 *
 * @param[in] hostname Pointer to the host name for which to querry the address
 * @param[out] ipv6_addr_buffer Buffer to char array to output the synthesized IPv6 address
 *
 * @retval 0 On success
 * @retval <0 On failure
 */
int golioth_ot_synthesize_ipv6_address(char *hostname, char *ipv6_addr_buffer);

#endif /* __GOLIOTH_OPENTHREAD_H__ */
