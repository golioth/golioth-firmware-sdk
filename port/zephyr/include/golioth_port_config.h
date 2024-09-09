/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/logging/log.h>

#define LOG_TAG_DEFINE(tag) LOG_MODULE_REGISTER(tag, CONFIG_GOLIOTH_LOG_LEVEL)

#ifdef CONFIG_GOLIOTH_DEBUG_LOG

#define GLTH_LOGE(tag, ...) LOG_ERR(__VA_ARGS__)

#define GLTH_LOGW(tag, ...) LOG_WRN(__VA_ARGS__)

#define GLTH_LOGI(tag, ...) LOG_INF(__VA_ARGS__)

#define GLTH_LOGD(tag, ...) LOG_DBG(__VA_ARGS__)

#define GLTH_LOGV(tag, ...) LOG_DBG(__VA_ARGS__)

#define GLTH_LOG_BUFFER_HEXDUMP(tag, buf, size, level) LOG_HEXDUMP_DBG(buf, size, "buffer")

#endif
