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

/// @defgroup golioth_lightdb_state golioth_lightdb_state
/// Functions for interacting with Golioth LightDB State service.
///
/// https://docs.golioth.io/reference/protocols/coap/lightdb
/// @{

//-------------------------------------------------------------------------------
// LightDB State
//-------------------------------------------------------------------------------

/// Set an integer in LightDB state at a particular path
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
/// @retval GOLIOTH_OK request enqueued
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_MEM_ALLOC memory allocation error
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
enum golioth_status golioth_lightdb_set_int(struct golioth_client *client,
                                            const char *path,
                                            int32_t value,
                                            golioth_set_cb_fn callback,
                                            void *callback_arg);

/// Set a bool in LightDB state at a particular path
///
/// Same as @ref golioth_lightdb_set_int, but for type bool
enum golioth_status golioth_lightdb_set_bool(struct golioth_client *client,
                                             const char *path,
                                             bool value,
                                             golioth_set_cb_fn callback,
                                             void *callback_arg);

/// Set a float in LightDB state at a particular path
///
/// Same as @ref golioth_lightdb_set_int, but for type float
enum golioth_status golioth_lightdb_set_float(struct golioth_client *client,
                                              const char *path,
                                              float value,
                                              golioth_set_cb_fn callback,
                                              void *callback_arg);

/// Set a string in LightDB state at a particular path
///
/// Same as @ref golioth_lightdb_set_int, but for type string
enum golioth_status golioth_lightdb_set_string(struct golioth_client *client,
                                               const char *path,
                                               const char *str,
                                               size_t str_len,
                                               golioth_set_cb_fn callback,
                                               void *callback_arg);

/// Set an object in LightDB state at a particular path
///
/// The serialization format of the object is specified by the content_type argument.
/// Currently this is either JSON or CBOR.
///
/// Similar to @ref golioth_lightdb_set_int.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to set (e.g. "my_integer")
/// @param content_type The serialization format of buf
/// @param buf A buffer containing the object to send
/// @param buf_len Length of buf
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @retval GOLIOTH_OK request enqueued
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_MEM_ALLOC memory allocation error
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
enum golioth_status golioth_lightdb_set(struct golioth_client *client,
                                        const char *path,
                                        enum golioth_content_type content_type,
                                        const uint8_t *buf,
                                        size_t buf_len,
                                        golioth_set_cb_fn callback,
                                        void *callback_arg);

/// Get data in LightDB state at a particular path
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
enum golioth_status golioth_lightdb_get(struct golioth_client *client,
                                        const char *path,
                                        enum golioth_content_type content_type,
                                        golioth_get_cb_fn callback,
                                        void *callback_arg);

/// Delete a path in LightDB state
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
/// @retval GOLIOTH_OK request enqueued
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_MEM_ALLOC memory allocation error
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
enum golioth_status golioth_lightdb_delete(struct golioth_client *client,
                                           const char *path,
                                           golioth_set_cb_fn callback,
                                           void *callback_arg);

/// Observe a path in LightDB state
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
/// @param content_type The serialization format to request for the path
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @retval GOLIOTH_OK request enqueued
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_MEM_ALLOC memory allocation error
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
enum golioth_status golioth_lightdb_observe(struct golioth_client *client,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            golioth_get_cb_fn callback,
                                            void *callback_arg);

/// @}

#ifdef __cplusplus
}
#endif
