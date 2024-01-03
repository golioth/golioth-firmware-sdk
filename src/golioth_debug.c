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

static enum golioth_debug_log_level _level = CONFIG_GOLIOTH_DEBUG_DEFAULT_LOG_LEVEL;
static struct golioth_client* _client = NULL;
static bool _cloud_log_enabled = CONFIG_GOLIOTH_AUTO_LOG_TO_CLOUD;

void golioth_debug_set_log_level(enum golioth_debug_log_level level) {
    _level = level;
}

enum golioth_debug_log_level golioth_debug_get_log_level(void) {
    return _level;
}

// Adapted from https://stackoverflow.com/a/7776146
void golioth_debug_hexdump(const char* tag, const void* addr, int len) {
    const int per_line = 16;

    int i;
    unsigned char buff[per_line + 1];
    const unsigned char* pc = (const unsigned char*)addr;

    if (len <= 0) {
        return;
    }

    // Output description if given.
    if (tag != NULL) {
        printf("%s:\n", tag);
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of per_line means new or first line (with line offset).

        if ((i % per_line) == 0) {
            // Only print previous-line ASCII buffer for lines beyond first.
            if (i != 0) {
                printf("  %s\n", buff);
            }

            // Output the offset of current line.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And buffer a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {  // isprint() may be better.
            buff[i % per_line] = '.';
        } else {
            buff[i % per_line] = pc[i];
        }
        buff[(i % per_line) + 1] = '\0';
    }

    // Pad out last line if not exactly per_line characters.
    while ((i % per_line) != 0) {
        printf("   ");
        i++;
    }

    // And print the final ASCII buffer.
    printf("  %s\n", buff);
}

// Important Note!
//
// Do not use GLTH_LOGX statements in this function, as it can cause an infinite
// recursion with golioth_log_X_async().
//
// If you must log, use printf instead.
void golioth_debug_printf(
        uint64_t tstamp_ms,
        enum golioth_debug_log_level level,
        const char* tag,
        const char* format,
        ...) {
    if (!_cloud_log_enabled) {
        return;
    }

    if (!_client) {
        return;
    }

    // Avoid re-entering this function
    static bool log_in_progress = false;
    if (log_in_progress) {
        return;
    }

    // Figure out how large of a char buffer we need to store this message
    va_list args;
    va_start(args, format);
    int buffer_size = vsnprintf(NULL, 0, format, args) + 1;  // +1 for NULL
    va_end(args);

    if (buffer_size <= 0) {
        return;
    }

    // Temporarily allocate a buffer to store the message
    char* msg_buffer = golioth_sys_malloc(buffer_size);
    if (!msg_buffer) {
        return;
    }

    va_start(args, format);
    vsnprintf(msg_buffer, buffer_size, format, args);
    va_end(args);

    // Log to Golioth asynchronously.
    //
    // Setting the "in progress" flag ensures that we can't re-enter this function
    // while calling the golioth_log_X_async functions, which might themselves
    // use GLTH_LOGX statements (which would cause infinite re-entrance).
    log_in_progress = true;
    switch (level) {
        case GOLIOTH_DEBUG_LOG_LEVEL_ERROR:
            golioth_log_error_async(_client, tag, msg_buffer, NULL, NULL);
            break;
        case GOLIOTH_DEBUG_LOG_LEVEL_WARN:
            golioth_log_warn_async(_client, tag, msg_buffer, NULL, NULL);
            break;
        case GOLIOTH_DEBUG_LOG_LEVEL_INFO:
            golioth_log_info_async(_client, tag, msg_buffer, NULL, NULL);
            break;
        case GOLIOTH_DEBUG_LOG_LEVEL_VERBOSE:  // fallthrough
        case GOLIOTH_DEBUG_LOG_LEVEL_DEBUG:
            golioth_log_debug_async(_client, tag, msg_buffer, NULL, NULL);
            break;
        case GOLIOTH_DEBUG_LOG_LEVEL_NONE:  // fallthrough
        default:
            break;
    }
    log_in_progress = false;

    // It's safe to free the message buffer, since the async log above
    // makes a copy of the message.
    golioth_sys_free(msg_buffer);
}

void golioth_debug_set_client(struct golioth_client* client) {
    _client = client;
}

void golioth_debug_set_cloud_log_enabled(bool enable) {
    _cloud_log_enabled = enable;
}
