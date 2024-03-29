/*
 * Copyright (c) 2023-2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/shell/shell.h>
#include <nrf_modem_at.h>

static char at_resp_buf[2048];

static int cmd_at(const struct shell *sh, size_t argc, char **argv)
{
    int err;

    if (argc < 2)
    {
        shell_help(sh);
        return -EINVAL;
    }

    err = nrf_modem_at_cmd(at_resp_buf, sizeof(at_resp_buf), "%s", argv[1]);
    if (err)
    {
        shell_error(sh, "Failed with error: %d", err);
        return err;
    }

    shell_print(sh, "%s", at_resp_buf);

    return 0;
}

SHELL_CMD_REGISTER(nrf_modem_at, NULL, "nrf_modem_at <bypassed_at_cmd>", cmd_at);
