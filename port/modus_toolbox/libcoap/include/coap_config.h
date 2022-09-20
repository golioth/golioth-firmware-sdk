#pragma once

#include <sys/socket.h>
#include <net/if.h>
#include "libcoap_posix_timers.h"

#define HAVE_SYS_SOCKET_H
#define HAVE_MALLOC
#define HAVE_ARPA_INET_H
#define HAVE_TIME_H
#define HAVE_NETDB_H
#define HAVE_INTTYPES_H
#define HAVE_STRUCT_CMSGHDR
#define HAVE_MBEDTLS
#define HAVE_STDIO_H
#define HAVE_ASSERT_H
#define HAVE_STRNLEN 1
#define HAVE_LIMITS_H
#define HAVE_NETINET_IN_H
#define _POSIX_TIMERS 1

#define COAP_CONSTRAINED_STACK 1
#define COAP_DISABLE_TCP 1
#define COAP_RESOURCES_NOHASH
#define ESPIDF_VERSION /* hack for net.c, to avoid usage of SIOCGIFADDR, which is not defined */
// #define WITH_LWIP 1 /* compilation error related to MEMP macros */

#define PACKAGE_NAME "libcoap"
#define PACKAGE_VERSION "4.3.1-rc2"
#define PACKAGE_STRING PACKAGE_NAME PACKAGE_VERSION

#define gai_strerror(x) "gai_strerror() not supported"

/*
 * IPv6 stubs and definitions
 *
 * We're not actually using IPv6, so these are needed simply to appease the compiler.
 */

#define ipi_spec_dst ipi_addr
struct in6_pktinfo {
  struct in6_addr ipi6_addr;        /* src/dst IPv6 address */
  unsigned int ipi6_ifindex;        /* send/recv interface index */
};
#define IN6_IS_ADDR_V4MAPPED(a) \
        ((((__const uint32_t *) (a))[0] == 0)                                 \
         && (((__const uint32_t *) (a))[1] == 0)                              \
         && (((__const uint32_t *) (a))[2] == htonl (0xffff)))

#define IPV6_PKTINFO IPV6_CHECKSUM
#define IN6_IS_ADDR_MULTICAST(a) false
#define IPV6_MULTICAST_HOPS 18

/*
 * Stubs for X509 file-oriented functions.
 *
 * We're not using X509 cert/key files (we use binary buffers),
 * so these are needed simply to appease the compiler.
 *
 * Normally, these would be defined in mbedtls if compiled with MBEDTLS_FS_IO,
 * but that is not the case for this port.
 */
#define mbedtls_x509_crt_parse_file(_1, _2) -1
#define mbedtls_pk_parse_keyfile(_1, _2, _3) -1
