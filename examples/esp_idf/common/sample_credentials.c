/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <golioth/client.h>
#include "wifi.h"
#include "nvs.h"

#ifndef CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS
#include "nvs.h"
#endif

#define TAG "sample_credentials"

static struct golioth_client_config _golioth_client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
            .psk =
                {
                    .psk_id = NULL,
                    .psk_id_len = 0,
                    .psk = NULL,
                    .psk_len = 0,
                },
        },
};

const struct golioth_client_config *golioth_sample_credentials_get(void)
{
#ifdef CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS

    _golioth_client_config.credentials.psk.psk_id = CONFIG_GOLIOTH_SAMPLE_PSK_ID;
    _golioth_client_config.credentials.psk.psk_id_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK_ID);
    _golioth_client_config.credentials.psk.psk = CONFIG_GOLIOTH_SAMPLE_PSK;
    _golioth_client_config.credentials.psk.psk_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK);

#else

    const char *psk_id = nvs_read_golioth_psk_id();
    const char *psk = nvs_read_golioth_psk();
    _golioth_client_config.credentials.psk.psk_id = psk_id;
    _golioth_client_config.credentials.psk.psk_id_len = strlen(psk_id);
    _golioth_client_config.credentials.psk.psk = psk;
    _golioth_client_config.credentials.psk.psk_len = strlen(psk);
#endif

    return &_golioth_client_config;
}
