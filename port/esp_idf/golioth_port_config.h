/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <sdkconfig.h>
#include <esp_log.h>

#define GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, size, level)                   \
    do                                                                       \
    {                                                                        \
        switch (level)                                                       \
        {                                                                    \
            case GOLIOTH_DEBUG_LOG_LEVEL_ERROR:                              \
                ESP_LOG_BUFFER_HEXDUMP(TAG, payload, size, ESP_LOG_ERROR);   \
                break;                                                       \
            case GOLIOTH_DEBUG_LOG_LEVEL_WARN:                               \
                ESP_LOG_BUFFER_HEXDUMP(TAG, payload, size, ESP_LOG_WARN);    \
                break;                                                       \
            case GOLIOTH_DEBUG_LOG_LEVEL_INFO:                               \
                ESP_LOG_BUFFER_HEXDUMP(TAG, payload, size, ESP_LOG_INFO);    \
                break;                                                       \
            case GOLIOTH_DEBUG_LOG_LEVEL_DEBUG:                              \
                ESP_LOG_BUFFER_HEXDUMP(TAG, payload, size, ESP_LOG_DEBUG);   \
                break;                                                       \
            case GOLIOTH_DEBUG_LOG_LEVEL_VERBOSE:                            \
                ESP_LOG_BUFFER_HEXDUMP(TAG, payload, size, ESP_LOG_VERBOSE); \
                break;                                                       \
            case GOLIOTH_DEBUG_LOG_LEVEL_NONE:                               \
            default:                                                         \
                break;                                                       \
        }                                                                    \
    } while (0)

#define GLTH_LOGX(COLOR, LEVEL, LEVEL_STR, TAG, ...)  \
    do                                                \
    {                                                 \
        if ((LEVEL) <= golioth_debug_get_log_level()) \
        {                                             \
            switch (LEVEL)                            \
            {                                         \
                case GOLIOTH_DEBUG_LOG_LEVEL_ERROR:   \
                    ESP_LOGE(TAG, __VA_ARGS__);       \
                    break;                            \
                case GOLIOTH_DEBUG_LOG_LEVEL_WARN:    \
                    ESP_LOGW(TAG, __VA_ARGS__);       \
                    break;                            \
                case GOLIOTH_DEBUG_LOG_LEVEL_INFO:    \
                    ESP_LOGI(TAG, __VA_ARGS__);       \
                    break;                            \
                case GOLIOTH_DEBUG_LOG_LEVEL_DEBUG:   \
                    ESP_LOGD(TAG, __VA_ARGS__);       \
                    break;                            \
                case GOLIOTH_DEBUG_LOG_LEVEL_VERBOSE: \
                    ESP_LOGV(TAG, __VA_ARGS__);       \
                    break;                            \
                case GOLIOTH_DEBUG_LOG_LEVEL_NONE:    \
                default:                              \
                    break;                            \
            }                                         \
        }                                             \
    } while (0)
