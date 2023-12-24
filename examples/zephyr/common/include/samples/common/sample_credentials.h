/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GOLIOTH_INCLUDE_SAMPLE_CREDENTIALS_H__
#define __GOLIOTH_INCLUDE_SAMPLE_CREDENTIALS_H__

#include <golioth/golioth.h>

/**
 * @brief This function returns Golioth credentials in one of three different ways
 *
 *   1. Return hardcoded PSK credentials
 *   2. Return hardcoded certificate (PKI) credentials
 *   3. Return PSK credentials stored on the settings partition
 *
 * The function is defined in both hardcoded_credentials.c and settings_golioth.c and will be
 * automatically added to the build based on Kconfig symbols.
 *
 * @return golioth_client_config_t configuration to use when creating a golioth_client_t
 */
const golioth_client_config_t* golioth_sample_credentials_get(void);

#endif /* __GOLIOTH_INCLUDE_SAMPLE_CREDENTIALS_H__ */
