/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <golioth_version.h>

#define GOLIOTH_BOOT_BANNER_PRIO 32
int golioth_boot_banner(void)
{
    printk("*** Golioth Firmware SDK %s ***\n",
           (IS_ENABLED(CONFIG_GOLIOTH_VERSION_INCLUDE_COMMIT)) ? STRINGIFY(GOLIOTH_BUILD_VERSION)
                                                               : "v" GOLIOTH_VERSION_STRING);

    return 0;
}

SYS_INIT(golioth_boot_banner, APPLICATION, GOLIOTH_BOOT_BANNER_PRIO);
