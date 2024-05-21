/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/golioth_debug.h>
#include "golioth_log_esp_idf.h"
#include <golioth/log.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "string.h"

static vprintf_like_t original_log_func;

static struct golioth_client *_client = NULL;

static int golioth_log_espidf(const char *format, va_list args)
{
    if (!CONFIG_GOLIOTH_AUTO_LOG_TO_CLOUD)
    {
        return -1;
    }

    if (!_client)
    {
        return -1;
    }

    // The 7th character in the format tells the log level as
    // E = Error, W = Warning, I = info, D = Debug, V = Verbose
    char log_level = format[7];

    // Temporary buffer to store the message
    char msg_buffer[CONFIG_GOLIOTH_ESPLOG_MAX_BUFF_SIZE] = {0};
    vsnprintf(msg_buffer, CONFIG_GOLIOTH_ESPLOG_MAX_BUFF_SIZE, format, args);
    printf("%s", msg_buffer);

    // The formatted string looks like this: "[0;32mI (%lu) %s: Actual Log %d [0m"
    // The start and end characters convey formatting and color information
    // This information should not be sent to the cloud.
    // Insert a null char 5 places behind to remove ESC[0m at the end of the string
    // Here strlen(msg_buffer) - 5 will not go out of bounds because the format will
    // always have at least 5 characters, even with an empty string
    msg_buffer[strlen(msg_buffer) - 5] = '\0';

    // Find ) and : characters in the string for extracting tag
    char tag_str[64] = {0};
    const char *tag = strchr(msg_buffer, ')');
    const char *msg_str = strchr(msg_buffer, ':');
    if (tag != NULL && msg_str != NULL)
    {
        tag = tag + 1;  // Skip space after )
        strncpy(tag_str, tag, msg_str - tag);
        // Ignore everything before ':'
        msg_str = msg_str + 2;  // Skip ':' and space
    }

    // Log to Golioth asynchronously.
    switch (log_level)
    {
        case 'E':
            golioth_log_error_async(_client, tag_str, msg_str, NULL, NULL);
            break;
        case 'W':
            golioth_log_warn_async(_client, tag_str, msg_str, NULL, NULL);
            break;
        case 'I':
            golioth_log_info_async(_client, tag_str, msg_str, NULL, NULL);
            break;
        case 'V':  // fallthrough
        case 'D':
            golioth_log_debug_async(_client, tag_str, msg_str, NULL, NULL);
            break;
        default:
            break;
    }

    return 0;
}

vprintf_like_t golioth_log_set_esp_vprintf(void *client)
{
    _client = client;
    return esp_log_set_vprintf(golioth_log_espidf);
}

vprintf_like_t golioth_log_reset_esp_vprintf(vprintf_like_t original_log_func)
{
    return esp_log_set_vprintf(original_log_func);
}

void golioth_sys_client_connected(void *client)
{
    if (CONFIG_GOLIOTH_ESPLOG_AUTO_LOG_TO_CLOUD == 1)
    {
        original_log_func = golioth_log_set_esp_vprintf(client);
    }
}

void golioth_sys_client_disconnected(void *client)
{
    if (CONFIG_GOLIOTH_ESPLOG_AUTO_LOG_TO_CLOUD == 1)
    {
        golioth_log_reset_esp_vprintf(original_log_func);
    }
}
