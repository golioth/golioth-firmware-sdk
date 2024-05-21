/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/golioth_sys.h>

vprintf_like_t golioth_log_set_esp_vprintf(void *client);
vprintf_like_t golioth_log_reset_esp_vprintf(vprintf_like_t original_log_func);
