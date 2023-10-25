/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "golioth_status.h"
#include "golioth_client.h"

/// @defgroup golioth_log golioth_log
/// Functions for logging messages to Golioth
///
/// https://docs.golioth.io/reference/protocols/coap/logging
/// @{

/// Log an error to Golioth asynchronously
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
golioth_status_t golioth_log_error_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Same as @ref golioth_log_error_async, but for warning level
golioth_status_t golioth_log_warn_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Same as @ref golioth_log_error_async, but for info level
golioth_status_t golioth_log_info_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Same as @ref golioth_log_error_async, but for debug level
golioth_status_t golioth_log_debug_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Log an error to Golioth synchronously
///
/// This function will block until one of three things happen (whichever comes first):
///
/// 1. A response is received from the server
/// 2. The user-provided timeout_s period expires without receiving a response
/// 3. The default GOLIOTH_COAP_RESPONSE_TIMEOUT_S period expires without receiving a response
///
/// @param client The client handle from @ref golioth_client_create
/// @param tag A free-form string to identify/tag the message
/// @param log_message String to log. Must be NULL-terminated.
/// @param timeout_s The timeout, in seconds, for receiving a server response
golioth_status_t golioth_log_error_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s);

/// Same as @ref golioth_log_error_sync, but for warning level
golioth_status_t golioth_log_warn_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s);

/// Same as @ref golioth_log_error_sync, but for info level
golioth_status_t golioth_log_info_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s);

/// Same as @ref golioth_log_error_sync, but for debug level
golioth_status_t golioth_log_debug_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s);

/// @}
