/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __GOLIOTH_INCLUDE_SAMPLE_CREDENTIALS_H__
#define __GOLIOTH_INCLUDE_SAMPLE_CREDENTIALS_H__

#include <golioth/client.h>

/**
 * @brief This function returns Golioth credentials either from hardcoded values defined in
 * sdkconfig.defaults or from shell at runtime
 *
 * @return struct golioth_client_config configuration to use when creating a struct golioth_client*
 */
const struct golioth_client_config *golioth_sample_credentials_get(void);

#endif /* __GOLIOTH_INCLUDE_SAMPLE_CREDENTIALS_H__ */

#ifdef __cplusplus
}
#endif
