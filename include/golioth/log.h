/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <golioth/golioth_status.h>
#include <golioth/client.h>

/// @defgroup golioth_log golioth_log
/// Functions for logging messages to Golioth
///
/// https://docs.golioth.io/reference/protocols/coap/logging
/// @{

/// Log an error to Golioth
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. The callback will be invoked when a response
/// is received or a timeout occurs.
///
/// @param client The client handle from @ref golioth_client_create
/// @param tag A free-form string to identify/tag the message
/// @param log_message String to log. Must be NULL-terminated.
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
enum golioth_status golioth_log_error(struct golioth_client *client,
                                      const char *tag,
                                      const char *log_message,
                                      golioth_set_cb_fn callback,
                                      void *callback_arg);

/// Same as @ref golioth_log_error, but for warning level
enum golioth_status golioth_log_warn(struct golioth_client *client,
                                     const char *tag,
                                     const char *log_message,
                                     golioth_set_cb_fn callback,
                                     void *callback_arg);

/// Same as @ref golioth_log_error, but for info level
enum golioth_status golioth_log_info(struct golioth_client *client,
                                     const char *tag,
                                     const char *log_message,
                                     golioth_set_cb_fn callback,
                                     void *callback_arg);

/// Same as @ref golioth_log_error, but for debug level
enum golioth_status golioth_log_debug(struct golioth_client *client,
                                      const char *tag,
                                      const char *log_message,
                                      golioth_set_cb_fn callback,
                                      void *callback_arg);

/// @}

#ifdef __cplusplus
}
#endif
