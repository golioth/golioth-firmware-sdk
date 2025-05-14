/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "nsi_host_getenv.h"

char *nsi_host_getenv(const char *name)
{
    return getenv(name);
}
