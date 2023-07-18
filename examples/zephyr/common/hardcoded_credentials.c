/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_hardcoded_credentials, LOG_LEVEL_DBG);

#include "golioth.h"

#ifdef CONFIG_GOLIOTH_AUTH_METHOD_CERT

/* Awaiting certificate support in Zephyr port */

/* FIXME: define start/end addresses for PKI files */
golioth_client_config_t _golioth_client_config_psk = {
        .credentials = {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
                .pki = {
                        .ca_cert = ca_pem_start,
                        .ca_cert_len = ca_pem_len,
                        .public_cert = client_pem_start,
                        .public_cert_len = client_pem_len,
                        .private_key = client_key_start,
                        .private_key_len = client_key_len,
                }}};

#else /* Using PSK Authentication */

golioth_client_config_t _golioth_client_config_psk = {
        .credentials = {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                .psk = {
                        .psk_id = CONFIG_GOLIOTH_SAMPLE_PSK_ID,
                        .psk_id_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK_ID),
                        .psk = CONFIG_GOLIOTH_SAMPLE_PSK,
                        .psk_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK),
                }}};
#endif

golioth_client_config_t* hardcoded_credentials_get(void) {
    return &_golioth_client_config_psk;
}
