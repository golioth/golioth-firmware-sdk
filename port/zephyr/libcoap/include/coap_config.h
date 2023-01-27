/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ZEPHYR_MODULE_COAP_CONFIG_H__
#define __ZEPHYR_MODULE_COAP_CONFIG_H__

#define HAVE_MALLOC
#define HAVE_ARPA_INET_H
#define HAVE_NETDB_H
#define HAVE_INTTYPES_H
#define HAVE_STRNLEN

/* Provide <sys/random.h> header and getrandom() function implementation */
#define HAVE_GETRANDOM

/* Use statically (i.e. not on stack) allocated, mutex protected buffers */
#define COAP_CONSTRAINED_STACK 1

/* Only UDP is supported for now */
#define COAP_DISABLE_TCP 1

/* Only client mode is supported */
#define COAP_CLIENT_SUPPORT 1
#define COAP_SERVER_SUPPORT 0

/* Those are not really useful, as libcoap is not an externally distributed shared library */
#define PACKAGE_NAME ""
#define PACKAGE_VERSION ""
#define PACKAGE_STRING PACKAGE_NAME PACKAGE_VERSION

/* Workarounds for missing networking bits */
#define IN_MULTICAST(...) 0
#define IN6_IS_ADDR_MULTICAST(a) IN_MULTICAST(a)
#define IN6_IS_ADDR_V4MAPPED(...) 0
#define IP_MULTICAST_TTL 1
#define IPV6_MULTICAST_HOPS 1

#endif /* __ZEPHYR_MODULE_COAP_CONFIG_H__ */
