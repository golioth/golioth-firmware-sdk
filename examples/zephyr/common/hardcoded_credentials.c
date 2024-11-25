/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_hardcoded_credentials, LOG_LEVEL_DBG);

#include <golioth/client.h>

#ifdef CONFIG_GOLIOTH_SAMPLE_AUTH_TYPE_PKI

static const uint8_t tls_client_crt[] = {
#include "golioth-systemclient-crt.inc"
};

static const uint8_t tls_client_key[] = {
#include "golioth-systemclient-key.inc"
};

static const uint8_t tls_ca_crt[] = {
#include "golioth-systemclient-ca_crt.inc"
};

static const uint8_t tls_secondary_ca_crt[] = {
#if CONFIG_GOLIOTH_SECONDARY_CA_CRT
#include "golioth-systemclient-secondary_ca_crt.inc"
#endif
};

static const struct golioth_client_config _golioth_client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
            .pki =
                {
                    .ca_cert = tls_ca_crt,
                    .ca_cert_len = sizeof(tls_ca_crt),
                    .public_cert = tls_client_crt,
                    .public_cert_len = sizeof(tls_client_crt),
                    .private_key = tls_client_key,
                    .private_key_len = sizeof(tls_client_key),
                    .secondary_ca_cert = tls_secondary_ca_crt,
                    .secondary_ca_cert_len = sizeof(tls_secondary_ca_crt),
                },
        },
};

#elif defined(CONFIG_GOLIOTH_SAMPLE_AUTH_TYPE_PSK)

static const struct golioth_client_config _golioth_client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
            .psk =
                {
                    .psk_id = CONFIG_GOLIOTH_SAMPLE_PSK_ID,
                    .psk_id_len = sizeof(CONFIG_GOLIOTH_SAMPLE_PSK_ID) - 1,
                    .psk = CONFIG_GOLIOTH_SAMPLE_PSK,
                    .psk_len = sizeof(CONFIG_GOLIOTH_SAMPLE_PSK) - 1,
                },
        },
};

#elif defined(CONFIG_GOLIOTH_SAMPLE_AUTH_TYPE_TAG)

static const struct golioth_client_config _golioth_client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_TAG,
            .tag = CONFIG_GOLIOTH_COAP_CLIENT_CREDENTIALS_TAG,
        },
};

#endif

const struct golioth_client_config *golioth_sample_credentials_get(void)
{
    return &_golioth_client_config;
}
