/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test, LOG_LEVEL_DBG);

#include <samples/common/net_connect.h>
#include <samples/common/sample_credentials.h>

void hil_test_entry(const struct golioth_client_config *config);

int main(void)
{
    net_connect();

    hil_test_entry(golioth_sample_credentials_get());

    return 0;
}
