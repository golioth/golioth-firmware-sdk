/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/golioth_status.h>
#include <golioth/client.h>

/// @defgroup golioth_stream golioth_stream
/// Functions for interacting with Golioth LightDB Stream service.
///
/// https://docs.golioth.io/reference/protocols/coap/stream
/// @{

/// Set an integer in LightDB stream at a particular path asynchronously
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. Optionally, the user may supply
/// a callback that will be called when the response is received (indicating
/// the request was acknowledged by the server) or a timeout occurs (response
/// never received).
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB stream to set (e.g. "my_integer")
/// @param value The value to set at path
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @return GOLIOTH_OK - request enqueued
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
enum golioth_status golioth_stream_set_int_async(struct golioth_client *client,
                                                 const char *path,
                                                 int32_t value,
                                                 golioth_set_cb_fn callback,
                                                 void *callback_arg);

/// Set an integer in LightDB stream at a particular path synchronously
///
/// This function will block until one of three things happen (whichever comes first):
///
/// 1. A response is received from the server
/// 2. The user-provided timeout_s period expires without receiving a response
/// 3. The default GOLIOTH_COAP_RESPONSE_TIMEOUT_S period expires without receiving a response
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB stream to set (e.g. "my_integer")
/// @param value The value to set at path
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @return GOLIOTH_OK - response received from server, set was successful
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
/// @return GOLIOTH_ERR_TIMEOUT - response not received from server, timeout occurred
enum golioth_status golioth_stream_set_int_sync(struct golioth_client *client,
                                                const char *path,
                                                int32_t value,
                                                int32_t timeout_s);

/// Set a bool in LightDB stream at a particular path asynchronously
///
/// Same as @ref golioth_stream_set_int_async, but for type bool
enum golioth_status golioth_stream_set_bool_async(struct golioth_client *client,
                                                  const char *path,
                                                  bool value,
                                                  golioth_set_cb_fn callback,
                                                  void *callback_arg);

/// Set a bool in LightDB state at a particular path synchronously
///
/// Same as @ref golioth_stream_set_int_sync, but for type bool
enum golioth_status golioth_stream_set_bool_sync(struct golioth_client *client,
                                                 const char *path,
                                                 bool value,
                                                 int32_t timeout_s);

/// Set a float in LightDB strean at a particular path asynchronously
///
/// Same as @ref golioth_stream_set_int_async, but for type float
enum golioth_status golioth_stream_set_float_async(struct golioth_client *client,
                                                   const char *path,
                                                   float value,
                                                   golioth_set_cb_fn callback,
                                                   void *callback_arg);

/// Set a float in LightDB strean at a particular path synchronously
///
/// Same as @ref golioth_stream_set_int_sync, but for type float
enum golioth_status golioth_stream_set_float_sync(struct golioth_client *client,
                                                  const char *path,
                                                  float value,
                                                  int32_t timeout_s);

/// Set a string in LightDB stream at a particular path asynchronously
///
/// Same as @ref golioth_stream_set_int_async, but for type string
enum golioth_status golioth_stream_set_string_async(struct golioth_client *client,
                                                    const char *path,
                                                    const char *str,
                                                    size_t str_len,
                                                    golioth_set_cb_fn callback,
                                                    void *callback_arg);

/// Set a string in LightDB stream at a particular path synchronously
///
/// Same as @ref golioth_stream_set_int_sync, but for type string
enum golioth_status golioth_stream_set_string_sync(struct golioth_client *client,
                                                   const char *path,
                                                   const char *str,
                                                   size_t str_len,
                                                   int32_t timeout_s);

/// Set an object in LightDB stream at a particular path asynchronously
///
/// Similar to @ref golioth_stream_set_int_async, but content_type must be specified
////
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB stream to set (e.g. "my_obj")
/// @param content_type The serialization format of buf
/// @param buf A buffer containing the object to send
/// @param buf_len Length of buf
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @return GOLIOTH_OK - request enqueued
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
enum golioth_status golioth_stream_set_async(struct golioth_client *client,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             const uint8_t *buf,
                                             size_t buf_len,
                                             golioth_set_cb_fn callback,
                                             void *callback_arg);

/// Set an object in LightDB stream at a particular path synchronously
///
/// Similar to @ref golioth_stream_set_int_sync, but content_type must be specified
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB stream to set (e.g. "my_obj")
/// @param content_type The serialization format of buf
/// @param buf A buffer containing the object to send
/// @param buf_len Length of buf
/// @param timeout_s The timeout, in seconds, for receiving a server response
enum golioth_status golioth_stream_set_sync(struct golioth_client *client,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            const uint8_t *buf,
                                            size_t buf_len,
                                            int32_t timeout_s);

/// @}
