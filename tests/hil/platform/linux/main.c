/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <golioth/client.h>

#define TAG "main"

void hil_test_entry(const struct golioth_client_config *config);

static void term(int signum)
{
    exit(0);
}

int main(void)
{
    /* Register handler for SIGTERM so that we exit gracefuly
       Without calling exit(), will not output code coverage
       information. */
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);

    char *golioth_psk_id = getenv("GOLIOTH_PSK_ID");
    if ((!golioth_psk_id) || strlen(golioth_psk_id) <= 0)
    {
        fprintf(stderr, "PSK ID is not specified.\n");
        return 1;
    }
    char *golioth_psk = getenv("GOLIOTH_PSK");
    if ((!golioth_psk) || strlen(golioth_psk) <= 0)
    {
        fprintf(stderr, "PSK is not specified.\n");
        return 1;
    }

    struct golioth_client_config config = {
        .credentials =
            {
                .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                .psk =
                    {
                        .psk_id = golioth_psk_id,
                        .psk_id_len = strlen(golioth_psk_id),
                        .psk = golioth_psk,
                        .psk_len = strlen(golioth_psk),
                    },
            },
    };

    hil_test_entry(&config);

    return 0;
}
