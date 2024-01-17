/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <assert.h>

#include <golioth/client.h>
#include <samples/common/net_connect.h>
#include <samples/common/sample_credentials.h>

#include "golioth_basics.h"

int main(void)
{
    const struct golioth_client_config *config = golioth_sample_credentials_get();

    net_connect();

    struct golioth_client *client = golioth_client_create(config);
    assert(client);
    golioth_basics(client);

    return 0;
}
