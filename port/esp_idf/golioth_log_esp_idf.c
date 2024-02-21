/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/golioth_debug.h>
#include <golioth/log.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "string.h"

static bool _cloud_log_enabled = CONFIG_GOLIOTH_AUTO_LOG_TO_CLOUD;

static struct golioth_client *_client = NULL;

static int golioth_log_espidf(const char *format,
                          va_list args) {

    const char *new_format = format;

    if (!_cloud_log_enabled)
    {
        return -1;
    }

    if (!_client)
    {
        return -1;
    }

    // Avoid re-entering this function
    static bool log_in_progress = false;
    if (log_in_progress)
    {
        return -1;
    }

    va_list new_args;
    va_copy(new_args, args);

    // Since the format contains time (%lu) and tag (%s) values that need not
    // be sent to the cloud like so : "[0;32mI (%lu) %s: Actual Log %d [0m",
    // we remove the string upto colon : and update the args list
    // Also, the 7th character in the string tells the log level as
    // E = Error, W = Warning, I = info, D = Debug, V = Verbose
    char log_level = format[7];
    const char *colon_pos = strchr(format, ':');
    if (colon_pos != NULL) {
        new_format = colon_pos + 1; // Skip ':' and space

        // Move va_list to correspond to the new format
        va_arg(new_args, unsigned long); // Skip %lu
        va_arg(new_args, const char *);   // Skip %s
    }

    // Figure out how large of a char buffer we need to store this message
    int buffer_size = vsnprintf(NULL, 0, new_format, new_args) - 3;  // -3 to remove [0m at the end

    if (buffer_size <= 0)
    {
        return -1;
    }

    // Temporarily allocate a buffer to store the message
    char *msg_buffer = golioth_sys_malloc(buffer_size);
    if (!msg_buffer)
    {
        return -1;
    }

    vsnprintf(msg_buffer, buffer_size, new_format, new_args);
    printf("%s\n", msg_buffer);

    // Log to Golioth asynchronously.
    //
    // Setting the "in progress" flag ensures that we can't re-enter this function
    // while calling the golioth_log_X_async functions, which might themselves
    // use GLTH_LOGX statements (which would cause infinite re-entrance).
    log_in_progress = true;
    switch (log_level)
    {
        case 'E':
            golioth_log_error_async(_client, "", msg_buffer, NULL, NULL);
            break;
        case 'W':
            golioth_log_warn_async(_client, "", msg_buffer, NULL, NULL);
            break;
        case 'I':
            golioth_log_info_async(_client, "", msg_buffer, NULL, NULL);
            break;
        case 'V':  // fallthrough
        case 'D':
            golioth_log_debug_async(_client, "", msg_buffer, NULL, NULL);
            break;
        default:
            break;
    }
    log_in_progress = false;

    // It's safe to free the message buffer, since the async log above
    // makes a copy of the message.
    golioth_sys_free(msg_buffer);
    return 0;
}

vprintf_like_t golioth_log_set_esp_vprintf(void* client) {
    _client = client;
    return esp_log_set_vprintf(golioth_log_espidf);
}

vprintf_like_t golioth_log_reset_esp_vprintf(vprintf_like_t original_log_func) {
    return esp_log_set_vprintf(original_log_func);
}
