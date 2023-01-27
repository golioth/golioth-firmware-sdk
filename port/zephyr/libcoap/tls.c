/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_zephyr_tls);

#include "coap3/coap_internal.h"

#include <errno.h>
#include <zephyr/net/tls_credentials.h>

#include "coap_zephyr.h"

/*
 * TODO: Support one secure tag per session.
 */
static sec_tag_t libcoap_sec_tag = {
        515765868,
};

void* coap_dtls_new_context(coap_context_t* context) {
    return context;
}

void coap_dtls_free_context(void* dtls_context) {}

void* coap_dtls_new_client_session(coap_session_t* session) {
    return session;
}

void coap_dtls_free_session(coap_session_t* session) {
    (void)tls_credential_delete(libcoap_sec_tag, TLS_CREDENTIAL_PSK);
    (void)tls_credential_delete(libcoap_sec_tag, TLS_CREDENTIAL_PSK_ID);

    if (session && session->context && session->tls) {
        session->tls = NULL;
        coap_handle_event(session->context, COAP_EVENT_DTLS_CLOSED, session);
    }
}

int coap_dtls_zephyr_connect(coap_session_t* session) {
    coap_address_t connect_addr;
    int ret;

    coap_address_copy(&connect_addr, &session->addr_info.remote);

    ret = zsock_setsockopt(
            session->sock.fd, SOL_TLS, TLS_SEC_TAG_LIST, &libcoap_sec_tag, sizeof(libcoap_sec_tag));
    if (ret < 0) {
        return 0;
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
    int err;

    err = tls_credential_add(
            libcoap_sec_tag,
            TLS_CREDENTIAL_PSK_ID,
            setup_data->psk_info.identity.s,
            setup_data->psk_info.identity.length);
    if (err < 0) {
        LOG_ERR("Failed to register PSK ID: %d", err);
        return err;
    }

    err = tls_credential_add(
            libcoap_sec_tag,
            TLS_CREDENTIAL_PSK,
            setup_data->psk_info.key.s,
            setup_data->psk_info.key.length);
    if (err < 0) {
        LOG_ERR("Failed to register PSK: %d", err);
        return err;
    }

    return 1;
}

int coap_dtls_context_set_pki(
        coap_context_t* c_context,
        const coap_dtls_pki_t* setup_data,
        const coap_dtls_role_t role COAP_UNUSED) {
    /* Not supported yet */
    return 0;
}
