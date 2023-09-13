/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

#define shell_json_fprintf(_sh, _color, _status, _json_output,       \
                           _msg_type, _fmt, ...)                     \
    shell_fprintf(_sh, _color,                                       \
                  (_json_output) ?                                   \
                        "{"                                          \
                        "\"status\": \"" _status "\", "              \
                        "\"" _msg_type "\": \"" _fmt "\""            \
                        "}\n"                                        \
                  :                                                  \
                        _fmt "\n", ##__VA_ARGS__)

#define shell_json_print(_sh, _json_output, _msg_type, _fmt, ...)    \
    shell_json_fprintf(_sh, SHELL_NORMAL, "success",                 \
                       _json_output, _msg_type, _fmt, ##__VA_ARGS__)

#define shell_json_error(_sh, _json_output, _fmt, ...)               \
    shell_json_fprintf(_sh, SHELL_ERROR, "failed",                   \
                       _json_output, "msg", _fmt, ##__VA_ARGS__)

struct settings_read_callback_params {
    const struct shell *shell_ptr;
    bool value_found;
    bool json_output;
};

struct settings_list_callback_params {
    const struct shell *shell_ptr;
};

static int cmd_settings_set(const struct shell *shell, size_t argc, char *argv[])
{
    bool json_output;

    if (argc < 3) {
        shell_warn(shell, "Wrong number of arguments");
        shell_help(shell);
        return -ENOEXEC;
    }

    json_output = false;
    if (argc >= 4) {
        if (strcmp(argv[3], "--json") == 0) {
            json_output = true;
        }
    }

    const char *name = argv[1];
    const char *val = argv[2];
    size_t val_len = strlen(val);

    int err;

    shell_print(shell, "Setting %s to %s", name, val);

#ifdef CONFIG_SETTINGS_RUNTIME
    err = settings_runtime_set(name, val, val_len);
    if (err) {
        shell_json_error(shell, json_output,
                 "Failed to set runtime setting: %s:%s",
                 name, val);

        return -ENOEXEC;
    }
#endif

    err = settings_save_one(name, val, val_len);
    if (err) {
        shell_json_error(shell, json_output,
                 "Failed to save setting %s:%s",
                 name, val);

        return -ENOEXEC;
    }

    shell_json_print(shell, json_output, "msg",
             "Setting %s saved as %s",
             name, val);

    return 0;
}

static int settings_read_callback(const char      *key,
                                  size_t           len,
                                  settings_read_cb read_cb,
                                  void            *cb_arg,
                                  void            *param)
{
    ssize_t num_read_bytes = MIN(len, SETTINGS_MAX_VAL_LEN);
    uint8_t buffer[num_read_bytes + 1];
    struct settings_read_callback_params *params = param;

    /* Process only the exact match and ignore descendants of the searched name */
    if (settings_name_next(key, NULL) != 0) {
        return 0;
    }

    params->value_found = true;
    num_read_bytes = read_cb(cb_arg, buffer, num_read_bytes);

    if (num_read_bytes < 0) {
        shell_json_error(params->shell_ptr, params->json_output,
                 "Failed to read value: %d",
                 (int)num_read_bytes);

        return 0;
    }

    /*  add NULL to the last position in the buffer */
    buffer[num_read_bytes] = 0x00;

    shell_json_print(params->shell_ptr, params->json_output, "value",
             "%s", buffer);

    if (len > SETTINGS_MAX_VAL_LEN) {
        shell_print(params->shell_ptr, "(The output has been truncated)");
    }

    return 0;
}

static int cmd_settings_get(const struct shell *shell, size_t argc,
                            char *argv[])
{
    bool json_output;

    int err;
    const char *name = argv[1];

    json_output = false;
    if (argc >= 3) {
        if (strcmp(argv[2], "--json") == 0) {
            json_output = true;
        }
    }

    struct settings_read_callback_params params = {
        .shell_ptr = shell,
        .value_found = false,
        .json_output = json_output
    };

    err = settings_load_subtree_direct(name, settings_read_callback, &params);

    if (err) {
        shell_json_error(shell, json_output,
                 "Failed to load settings: %d", err);
    } else if (!params.value_found) {
        shell_json_error(shell, json_output,
                 "Setting not found");
    }

    return 0;
}

static int settings_list_callback(const char      *key,
                                  size_t           len,
                                  settings_read_cb read_cb,
                                  void            *cb_arg,
                                  void            *param)
{
    struct settings_list_callback_params *params = param;

    shell_print(params->shell_ptr, "%s", key);

    return 0;
}

static int cmd_settings_list(const struct shell *shell, size_t argc, char *argv[])
{
    int err;

    struct settings_list_callback_params params = {
        .shell_ptr = shell,
    };

    err = settings_load_subtree_direct(NULL, settings_list_callback, &params);

    if (err) {
        shell_error(shell, "Failed to load settings: %d", err);
    }

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(settings_commands,
    SHELL_CMD_ARG(set, NULL,
        "set a setting (usage: settings set <key> <value> [--json])",
        cmd_settings_set, 3, 1),
    SHELL_CMD_ARG(get, NULL,
        "get a setting (usage: settings get <key> [--json])",
        cmd_settings_get, 2, 1),
    SHELL_CMD_ARG(list, NULL,
        "list all settings (usage: settings list)",
        cmd_settings_list, 1, 0),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(settings, &settings_commands, "Settings commands", NULL);
