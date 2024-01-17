/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/settings/settings.h>

static int settings_autoload(void)
{
    return settings_load();
}

SYS_INIT(settings_autoload, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
