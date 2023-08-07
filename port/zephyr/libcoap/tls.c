/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_zephyr_tls);

#include "coap3/coap_internal.h"

#include <errno.h>
#include <string.h>
#include <zephyr/net/tls_credentials.h>

#include <mbedtls/ssl_ciphersuites.h>
#include <golioth_ciphersuites.h>

#include "coap_zephyr.h"

struct dtls_context_s {
    sec_tag_t sec_tag;
    char hostname[CONFIG_LIBCOAP_TLS_HOSTNAME_LEN_MAX + 1];
};

/*
 * TODO: Support one context per session.
 */
static struct dtls_context_s dtls_context = {
    .sec_tag = 515765868,
};

/* Use mbedTLS macros which are IANA ciphersuite names prepended with MBEDTLS_ */
#define GOLIOTH_CIPHERSUITE_ENTRY(x) _CONCAT(MBEDTLS_, x)

static int golioth_ciphersuites[] = {
    FOR_EACH_NONEMPTY_TERM(GOLIOTH_CIPHERSUITE_ENTRY, (, ), GOLIOTH_CIPHERSUITES)
};

void* coap_dtls_new_context(coap_context_t* context) {
    return &dtls_context;
}

void coap_dtls_free_context(void* dtls_context) {}

void* coap_dtls_new_client_session(coap_session_t* session) {
    return session;
}

void coap_dtls_free_session(coap_session_t* session) {
    struct dtls_context_s *dtls_ctx = session->context->dtls_context;

    (void)tls_credential_delete(dtls_ctx->sec_tag, TLS_CREDENTIAL_PSK);
    (void)tls_credential_delete(dtls_ctx->sec_tag, TLS_CREDENTIAL_PSK_ID);
    (void)tls_credential_delete(dtls_ctx->sec_tag, TLS_CREDENTIAL_CA_CERTIFICATE);
    (void)tls_credential_delete(dtls_ctx->sec_tag, TLS_CREDENTIAL_SERVER_CERTIFICATE);
    (void)tls_credential_delete(dtls_ctx->sec_tag, TLS_CREDENTIAL_PRIVATE_KEY);

    if (session && session->context && session->tls) {
        session->tls = NULL;
        coap_handle_event(session->context, COAP_EVENT_DTLS_CLOSED, session);
    }
}

int coap_dtls_zephyr_connect(coap_session_t* session) {
    coap_address_t connect_addr;
    int ret;

    coap_address_copy(&connect_addr, &session->addr_info.remote);

    struct dtls_context_s *dtls_ctx = session->context->dtls_context;

    ret = zsock_setsockopt(session->sock.fd,
                           SOL_TLS,
                           TLS_SEC_TAG_LIST,
                           &dtls_ctx->sec_tag,
                           sizeof(dtls_ctx->sec_tag));
    if (ret < 0) {
        return 0;
    }

    ret = zsock_setsockopt(session->sock.fd,
                           SOL_TLS,
                           TLS_HOSTNAME,
                           dtls_ctx->hostname,
                           strlen(dtls_ctx->hostname) + 1);
    if (ret < 0) {
        return 0;
    }

    if (sizeof(golioth_ciphersuites) > 0) {
        ret = zsock_setsockopt(
            session->sock.fd,
            SOL_TLS,
            TLS_CIPHERSUITE_LIST,
            golioth_ciphersuites,
            sizeof(golioth_ciphersuites));
        if (ret < 0) {
            return -errno;
        }
    }

    ret = zsock_connect(session->sock.fd, &connect_addr.addr.sa, connect_addr.size);
    if (ret < 0) {
        return 0;
    }

    coap_session_connected(session);

    return 1;
}

int coap_dtls_send(coap_session_t* session, const uint8_t* data, size_t data_len) {
    return coap_session_send(session, data, data_len);
}

void coap_dtls_startup(void) {}

int coap_dtls_is_supported(void) {
    return 1;
}

int coap_tls_is_supported(void) {
    return 0;
}

unsigned int coap_dtls_get_overhead(coap_session_t* c_session) {
    return 29;
}

int coap_dtls_receive(coap_session_t* c_session, const uint8_t* data, size_t data_len) {
    return coap_handle_dgram(c_session->context, c_session, (uint8_t*)data, data_len);
}

void coap_dtls_shutdown(void) {}

int coap_dtls_context_set_cpsk(coap_context_t* c_context, coap_dtls_cpsk_t* setup_data) {
    struct dtls_context_s *dtls_ctx = c_context->dtls_context;

    int err = tls_credential_add(
            dtls_ctx->sec_tag,
            TLS_CREDENTIAL_PSK_ID,
            setup_data->psk_info.identity.s,
            setup_data->psk_info.identity.length);
    if (err < 0) {
        LOG_ERR("Failed to register PSK ID: %d", err);
        return 0;
    }

    err = tls_credential_add(
            dtls_ctx->sec_tag,
            TLS_CREDENTIAL_PSK,
            setup_data->psk_info.key.s,
            setup_data->psk_info.key.length);
    if (err < 0) {
        LOG_ERR("Failed to register PSK: %d", err);
        return 0;
    }

    return 1;
}

int coap_dtls_context_set_pki(
        coap_context_t* c_context,
        const coap_dtls_pki_t* setup_data,
        const coap_dtls_role_t role COAP_UNUSED) {
    struct dtls_context_s *dtls_ctx = c_context->dtls_context;

    if (strlen(setup_data->client_sni) > CONFIG_LIBCOAP_TLS_HOSTNAME_LEN_MAX) {
        return 0;
    }

    strcpy(dtls_ctx->hostname, setup_data->client_sni);

    int err = tls_credential_add(
              dtls_ctx->sec_tag,
              TLS_CREDENTIAL_CA_CERTIFICATE,
              setup_data->pki_key.key.pem_buf.ca_cert,
              setup_data->pki_key.key.pem_buf.ca_cert_len);
    if (err < 0) {
        return 0;
    }

    err = tls_credential_add(
              dtls_ctx->sec_tag,
              TLS_CREDENTIAL_SERVER_CERTIFICATE,
              setup_data->pki_key.key.pem_buf.public_cert,
              setup_data->pki_key.key.pem_buf.public_cert_len);
    if (err < 0) {
        return 0;
    }

    err = tls_credential_add(
              dtls_ctx->sec_tag,
              TLS_CREDENTIAL_PRIVATE_KEY,
              setup_data->pki_key.key.pem_buf.private_key,
              setup_data->pki_key.key.pem_buf.private_key_len);
    if (err < 0) {
        return 0;
    }

    return 1;
}
