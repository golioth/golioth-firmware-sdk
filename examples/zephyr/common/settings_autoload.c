/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/settings/settings.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_settings_autoload, CONFIG_GOLIOTH_DEBUG_DEFAULT_LOG_LEVEL);

static int settings_autoload(void)
{
    LOG_INF("Initializing settings subsystem");
    int err = settings_subsys_init();

    if (err)
    {
        LOG_ERR("Failed to initialize settings subsystem: %d", err);
        return err;
    }

    LOG_INF("Loading settings");
    return settings_load();
}

SYS_INIT(settings_autoload, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
