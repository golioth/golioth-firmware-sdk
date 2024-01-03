/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <assert.h>

#include <golioth/client.h>
#include "golioth_basics.h"

#include <samples/common/net_connect.h>

int main(void) {
    golioth_client_config_t config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                    .psk = {
                            .psk_id = CONFIG_GOLIOTH_SAMPLE_PSK_ID,
                            .psk_id_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK_ID),
                            .psk = CONFIG_GOLIOTH_SAMPLE_PSK,
                            .psk_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK),
                    }}};

    net_connect();

    struct golioth_client* client = golioth_client_create(&config);
    assert(client);
    golioth_basics(client);

    return 0;
}
