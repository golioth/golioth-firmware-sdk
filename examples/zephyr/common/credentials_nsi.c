/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_credentials_nsi, CONFIG_GOLIOTH_DEBUG_DEFAULT_LOG_LEVEL);

#include <errno.h>
#include <golioth/client.h>
#include <zephyr/init.h>
#include <zephyr/net/tls_credentials.h>

#include "nsi_host_getenv.h"

/*
 * TLS credentials subsystem just remembers pointers to memory areas where
 * credentials are stored. This means that we need to allocate memory for
 * credentials ourselves.
 */
static uint8_t golioth_dtls_psk[CONFIG_GOLIOTH_SAMPLE_PSK_MAX_LEN];
static uint8_t golioth_dtls_psk_id[CONFIG_GOLIOTH_SAMPLE_PSK_ID_MAX_LEN];

static struct golioth_client_config client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
            .psk =
                {
                    .psk_id = golioth_dtls_psk_id,
                    .psk_id_len = 0,
                    .psk = golioth_dtls_psk,
                    .psk_len = 0,
                },
        },
};

static int credentials_nsi_init(void)
{
    struct golioth_psk_credential *cred_psk = &client_config.credentials.psk;
    char *psk_id;
    char *psk;

    LOG_INF("Loading credentials from environment variables");

    psk_id = nsi_host_getenv("GOLIOTH_SAMPLE_PSK_ID");
    psk = nsi_host_getenv("GOLIOTH_SAMPLE_PSK");

    if (psk_id)
    {
        LOG_INF("PSK-ID: %s", psk_id);
        cred_psk->psk_id_len = MIN(strlen(psk_id), CONFIG_GOLIOTH_SAMPLE_PSK_ID_MAX_LEN);
        memcpy(golioth_dtls_psk_id, psk_id, cred_psk->psk_id_len);
    }
    else
    {
        LOG_WRN("No %s set", "GOLIOTH_SAMPLE_PSK_ID");
    }

    if (psk)
    {
        LOG_INF("PSK: %s", psk);
        cred_psk->psk_len = MIN(strlen(psk), CONFIG_GOLIOTH_SAMPLE_PSK_MAX_LEN);
        memcpy(golioth_dtls_psk, psk, cred_psk->psk_len);
    }
    else
    {
        LOG_WRN("No %s set", "GOLIOTH_SAMPLE_PSK");
    }

    return 0;
}

SYS_INIT(credentials_nsi_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

const struct golioth_client_config *golioth_sample_credentials_get(void)
{
    return &client_config;
}
