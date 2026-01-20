/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/kernel.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_openthread);

#include <openthread.h>
#include <openthread/nat64.h>
#include <openthread/dns_client.h>
#include <openthread/error.h>

struct ot_dns_resolve_context
{
    otIp6Address golioth_addr;
    otDnsQueryConfig query_config;
    struct k_sem sem;
    char *ipv6;
    int err;
};

/* Callback for NAT64 IPv6 translated Golioth System Server address from the DNS query response */
static void ot_dns_callback(otError aError, const otDnsAddressResponse *aResponse, void *aContext)
{
    struct ot_dns_resolve_context *context = aContext;

    if (!aError && !otDnsAddressResponseGetAddress(aResponse, 0, &context->golioth_addr, NULL))
    {
        otIp6AddressToString(&context->golioth_addr, context->ipv6, OT_IP6_ADDRESS_STRING_SIZE);
    }

    context->err = aError;

    k_sem_give(&context->sem);
}

int golioth_ot_synthesize_ipv6_address(char *hostname, char *ipv6_addr_buffer)
{
    int err;
    otIp4Address dns_server_addr;

    struct ot_dns_resolve_context ot_dns_context = {
        .ipv6 = ipv6_addr_buffer,
    };

    k_sem_init(&ot_dns_context.sem, 0, 1);

    otInstance *ot_instance = openthread_get_default_instance();

    err = otIp4AddressFromString(CONFIG_DNS_SERVER1, &dns_server_addr);
    if (err != OT_ERROR_NONE)
    {
        LOG_ERR("DNS server IPv4 address error: %d", err);
        return err;
    }

    err = otNat64SynthesizeIp6Address(ot_instance,
                                      &dns_server_addr,
                                      &ot_dns_context.query_config.mServerSockAddr.mAddress);
    if (err != OT_ERROR_NONE)
    {
        LOG_ERR("Synthesize DNS server IPv6 address error: %d", err);
        return err;
    }

    err = otDnsClientResolveIp4Address(ot_instance,
                                       hostname,
                                       ot_dns_callback,
                                       &ot_dns_context,
                                       &ot_dns_context.query_config);
    if (err != OT_ERROR_NONE)
    {
        LOG_ERR("Golioth System Server address resolution DNS query error: %d", err);
        return err;
    }

    k_sem_take(&ot_dns_context.sem, K_FOREVER);

    return ot_dns_context.err;
}
