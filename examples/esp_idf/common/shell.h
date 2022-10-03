/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_console.h"

// Create task that will run the shell command-line interface.
//
// If you have custom commands to register, make sure to call shell_register_command
// before calling this function.
void shell_start(void);

// Register a custom shell command
void shell_register_command(const esp_console_cmd_t* cmd);
