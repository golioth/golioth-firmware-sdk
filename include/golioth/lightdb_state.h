/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/golioth_status.h>
#include <golioth/client.h>

/// @defgroup golioth_lightdb_state golioth_lightdb_state
/// Functions for interacting with Golioth LightDB State service.
///
/// https://docs.golioth.io/reference/protocols/coap/lightdb
/// @{

//-------------------------------------------------------------------------------
// LightDB State
//-------------------------------------------------------------------------------

/// Set an integer in LightDB state at a particular path asynchronously
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. Optionally, the user may supply
/// a callback that will be called when the response is received (indicating
/// the request was acknowledged by the server) or a timeout occurs (response
/// never received).
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to set (e.g. "my_integer")
/// @param value The value to set at path
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @return GOLIOTH_OK - request enqueued
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
enum golioth_status golioth_lightdb_set_int_async(struct golioth_client *client,
                                                  const char *path,
                                                  int32_t value,
                                                  golioth_set_cb_fn callback,
                                                  void *callback_arg);

/// Set an integer in LightDB state at a particular path synchronously
///
/// This function will block until one of three things happen (whichever comes first):
///
/// 1. A response is received from the server
/// 2. The user-provided timeout_s period expires without receiving a response
/// 3. The default GOLIOTH_COAP_RESPONSE_TIMEOUT_S period expires without receiving a response
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to set (e.g. "my_integer")
/// @param value The value to set at path
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @return GOLIOTH_OK - response received from server, set was successful
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
/// @return GOLIOTH_ERR_TIMEOUT - response not received from server, timeout occurred
enum golioth_status golioth_lightdb_set_int_sync(struct golioth_client *client,
                                                 const char *path,
                                                 int32_t value,
                                                 int32_t timeout_s);

/// Set a bool in LightDB state at a particular path asynchronously
///
/// Same as @ref golioth_lightdb_set_int_async, but for type bool
enum golioth_status golioth_lightdb_set_bool_async(struct golioth_client *client,
                                                   const char *path,
                                                   bool value,
                                                   golioth_set_cb_fn callback,
                                                   void *callback_arg);

/// Set a bool in LightDB state at a particular path synchronously
///
/// Same as @ref golioth_lightdb_set_int_sync, but for type bool
enum golioth_status golioth_lightdb_set_bool_sync(struct golioth_client *client,
                                                  const char *path,
                                                  bool value,
                                                  int32_t timeout_s);

/// Set a float in LightDB state at a particular path asynchronously
///
/// Same as @ref golioth_lightdb_set_int_async, but for type float
enum golioth_status golioth_lightdb_set_float_async(struct golioth_client *client,
                                                    const char *path,
                                                    float value,
                                                    golioth_set_cb_fn callback,
                                                    void *callback_arg);

enum golioth_status golioth_lightdb_set_float_sync(struct golioth_client *client,
                                                   const char *path,
                                                   float value,
                                                   int32_t timeout_s);

/// Set a string in LightDB state at a particular path asynchronously
///
/// Same as @ref golioth_lightdb_set_int_async, but for type string
enum golioth_status golioth_lightdb_set_string_async(struct golioth_client *client,
                                                     const char *path,
                                                     const char *str,
                                                     size_t str_len,
                                                     golioth_set_cb_fn callback,
                                                     void *callback_arg);

enum golioth_status golioth_lightdb_set_string_sync(struct golioth_client *client,
                                                    const char *path,
                                                    const char *str,
                                                    size_t str_len,
                                                    int32_t timeout_s);

/// Set an object in LightDB state at a particular path asynchronously
///
/// The serialization format of the object is specified by the content_type argument.
/// Currently this is either JSON or CBOR.
///
/// Similar to @ref golioth_lightdb_set_int_async.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to set (e.g. "my_integer")
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
enum golioth_status golioth_lightdb_set_async(struct golioth_client *client,
                                              const char *path,
                                              enum golioth_content_type content_type,
                                              const uint8_t *buf,
                                              size_t buf_len,
                                              golioth_set_cb_fn callback,
                                              void *callback_arg);

/// Set am object in LightDB state at a particular path synchronously
///
/// The serialization format of the object is specified by the content_type argument.
/// Currently this is either JSON or CBOR.
///
/// Similar to @ref golioth_lightdb_set_int_sync.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to set (e.g. "my_integer")
/// @param content_type The serialization format of buf
/// @param buf A buffer containing the object to send
/// @param buf_len Length of buf
/// @param timeout_s The timeout, in seconds, for receiving a server response
enum golioth_status golioth_lightdb_set_sync(struct golioth_client *client,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             const uint8_t *buf,
                                             size_t buf_len,
                                             int32_t timeout_s);

/// Get data in LightDB state at a particular path asynchronously.
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. The callback will be invoked when a response
/// is received or a timeout occurs.
///
/// The data passed into the callback function will be the raw payload bytes from the
/// server response. The callback function can convert the payload to the appropriate
/// type using, e.g. @ref golioth_payload_as_int, or using a parsing library (like cJSON or
/// zCBOR) in the case of a serialized object payload
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to get (e.g. "my_integer")
/// @param content_type The serialization format to request for the path
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
enum golioth_status golioth_lightdb_get_async(struct golioth_client *client,
                                              const char *path,
                                              enum golioth_content_type content_type,
                                              golioth_get_cb_fn callback,
                                              void *callback_arg);

/// Get an integer in LightDB state at a particular path synchronously.
///
/// This function will block until one of three things happen (whichever comes first):
///
/// 1. A response is received from the server
/// 2. The user-provided timeout_s period expires without receiving a response
/// 3. The default GOLIOTH_COAP_RESPONSE_TIMEOUT_S period expires without receiving a response
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to get (e.g. "my_integer")
/// @param value Output parameter, memory allocated by caller, populated with value of integer
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @return GOLIOTH_OK - response received from server, set was successful
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
/// @return GOLIOTH_ERR_TIMEOUT - response not received from server, timeout occurred
enum golioth_status golioth_lightdb_get_int_sync(struct golioth_client *client,
                                                 const char *path,
                                                 int32_t *value,
                                                 int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for type bool
enum golioth_status golioth_lightdb_get_bool_sync(struct golioth_client *client,
                                                  const char *path,
                                                  bool *value,
                                                  int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for type float
enum golioth_status golioth_lightdb_get_float_sync(struct golioth_client *client,
                                                   const char *path,
                                                   float *value,
                                                   int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for type string
enum golioth_status golioth_lightdb_get_string_sync(struct golioth_client *client,
                                                    const char *path,
                                                    char *strbuf,
                                                    size_t strbuf_size,
                                                    int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for objects
enum golioth_status golioth_lightdb_get_sync(struct golioth_client *client,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             uint8_t *buf,
                                             size_t *buf_size,
                                             int32_t timeout_s);

/// Delete a path in LightDB state asynchronously
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. The callback will be invoked when a response
/// is received or a timeout occurs.
///
/// Note: the server responds with success even if the path does not exist in LightDB state.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to delete (e.g. "my_integer")
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @return GOLIOTH_OK - request enqueued
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
enum golioth_status golioth_lightdb_delete_async(struct golioth_client *client,
                                                 const char *path,
                                                 golioth_set_cb_fn callback,
                                                 void *callback_arg);

/// Delete a path in LightDB state synchronously
///
/// This function will block until one of three things happen (whichever comes first):
///
/// 1. A response is received from the server
/// 2. The user-provided timeout_s period expires without receiving a response
/// 3. The default GOLIOTH_COAP_RESPONSE_TIMEOUT_S period expires without receiving a response
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to delete (e.g. "my_integer")
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @return GOLIOTH_OK - response received from server, set was successful
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
/// @return GOLIOTH_ERR_TIMEOUT - response not received from server, timeout occurred
enum golioth_status golioth_lightdb_delete_sync(struct golioth_client *client,
                                                const char *path,
                                                int32_t timeout_s);

/// Observe a path in LightDB state asynchronously
///
/// Observations allow the Golioth server to notify clients of a change in data
/// at a particular path, without the client having to poll for changes.
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. The callback will be invoked whenever the
/// data at path changes.
///
/// The data passed into the callback function will be the raw payload bytes from the
/// server. The callback function can convert the payload to the appropriate
/// type using, e.g. @ref golioth_payload_as_int, or using a JSON parsing library (like cJSON) in
/// the case of JSON payload.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to observe (e.g. "my_integer")
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @return GOLIOTH_OK - request enqueued
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
enum golioth_status golioth_lightdb_observe_async(struct golioth_client *client,
                                                  const char *path,
                                                  golioth_get_cb_fn callback,
                                                  void *callback_arg);

/// @}
