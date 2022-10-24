/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "golioth_status.h"
#include "golioth_client.h"

/// @defgroup golioth_lightdb golioth_lightdb
/// Functions for interacting with Golioth LightDB state and LightDB stream services.
///
/// https://docs.golioth.io/reference/protocols/coap/lightdb
/// <br>
/// https://docs.golioth.io/reference/protocols/coap/lightdb-stream
/// @{

/// Convert raw byte payload into an int32_t
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @return int32_t value returned from strtol(payload, NULL, 10)
int32_t golioth_payload_as_int(const uint8_t* payload, size_t payload_size);

/// Convert raw byte payload into a float
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @return float value returned from strtof(payload, NULL)
float golioth_payload_as_float(const uint8_t* payload, size_t payload_size);

/// Convert raw byte payload into a bool
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @return true - payload is exactly the string "true"
/// @return false - otherwise
bool golioth_payload_as_bool(const uint8_t* payload, size_t payload_size);

/// Returns true if payload has no contents
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
///
/// @return true - payload is NULL
/// @return true - payload_size is 0
/// @return true - payload is exactly the string "null"
/// @return false - otherwise
bool golioth_payload_is_null(const uint8_t* payload, size_t payload_size);

// TODO - block transfers for large post/get

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
golioth_status_t golioth_lightdb_set_int_async(
        golioth_client_t client,
        const char* path,
        int32_t value,
        golioth_set_cb_fn callback,
        void* callback_arg);

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
golioth_status_t golioth_lightdb_set_int_sync(
        golioth_client_t client,
        const char* path,
        int32_t value,
        int32_t timeout_s);

/// Set a bool in LightDB state at a particular path asynchronously
///
/// Same as @ref golioth_lightdb_set_int_async, but for type bool
golioth_status_t golioth_lightdb_set_bool_async(
        golioth_client_t client,
        const char* path,
        bool value,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Set a bool in LightDB state at a particular path synchronously
///
/// Same as @ref golioth_lightdb_set_int_sync, but for type bool
golioth_status_t golioth_lightdb_set_bool_sync(
        golioth_client_t client,
        const char* path,
        bool value,
        int32_t timeout_s);

/// Set a float in LightDB state at a particular path asynchronously
///
/// Same as @ref golioth_lightdb_set_int_async, but for type float
golioth_status_t golioth_lightdb_set_float_async(
        golioth_client_t client,
        const char* path,
        float value,
        golioth_set_cb_fn callback,
        void* callback_arg);

golioth_status_t golioth_lightdb_set_float_sync(
        golioth_client_t client,
        const char* path,
        float value,
        int32_t timeout_s);

/// Set a string in LightDB state at a particular path asynchronously
///
/// Same as @ref golioth_lightdb_set_int_async, but for type string
golioth_status_t golioth_lightdb_set_string_async(
        golioth_client_t client,
        const char* path,
        const char* str,
        size_t str_len,
        golioth_set_cb_fn callback,
        void* callback_arg);

golioth_status_t golioth_lightdb_set_string_sync(
        golioth_client_t client,
        const char* path,
        const char* str,
        size_t str_len,
        int32_t timeout_s);

/// Set a JSON object (encoded as a string) in LightDB state at a particular path asynchronously
///
/// Similar to @ref golioth_lightdb_set_int_async.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to set (e.g. "my_integer")
/// @param json_str A JSON object encoded as a string (e.g. "{ \"string_key\": \"value\"}")
/// @param json_str_len Length of json_str, not including NULL terminator
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @return GOLIOTH_OK - request enqueued
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_MEM_ALLOC - memory allocation error
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
golioth_status_t golioth_lightdb_set_json_async(
        golioth_client_t client,
        const char* path,
        const char* json_str,
        size_t json_str_len,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Set a JSON object (encoded as a string) in LightDB state at a particular path synchronously
///
/// Similar to @ref golioth_lightdb_set_int_sync.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to set (e.g. "my_integer")
/// @param json_str A JSON object encoded as a string (e.g. "{ \"string_key\": \"value\"}")
/// @param json_str_len Length of json_str, not including NULL terminator
/// @param timeout_s The timeout, in seconds, for receiving a server response
golioth_status_t golioth_lightdb_set_json_sync(
        golioth_client_t client,
        const char* path,
        const char* json_str,
        size_t json_str_len,
        int32_t timeout_s);

/// Get data in LightDB state at a particular path asynchronously.
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. The callback will be invoked when a response
/// is received or a timeout occurs.
///
/// The data passed into the callback function will be the raw payload bytes from the
/// server response. The callback function can convert the payload to the appropriate
/// type using, e.g. @ref golioth_payload_as_int, or using a JSON parsing library (like cJSON) in
/// the case of JSON payload.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in LightDB state to get (e.g. "my_integer")
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
golioth_status_t golioth_lightdb_get_async(
        golioth_client_t client,
        const char* path,
        golioth_get_cb_fn callback,
        void* callback_arg);

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
golioth_status_t golioth_lightdb_get_int_sync(
        golioth_client_t client,
        const char* path,
        int32_t* value,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for type bool
golioth_status_t golioth_lightdb_get_bool_sync(
        golioth_client_t client,
        const char* path,
        bool* value,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for type float
golioth_status_t golioth_lightdb_get_float_sync(
        golioth_client_t client,
        const char* path,
        float* value,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for type string
golioth_status_t golioth_lightdb_get_string_sync(
        golioth_client_t client,
        const char* path,
        char* strbuf,
        size_t strbuf_size,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_get_int_sync, but for string-encoded JSON objects
golioth_status_t golioth_lightdb_get_json_sync(
        golioth_client_t client,
        const char* path,
        char* strbuf,
        size_t strbuf_size,
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
golioth_status_t golioth_lightdb_delete_async(
        golioth_client_t client,
        const char* path,
        golioth_set_cb_fn callback,
        void* callback_arg);

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
golioth_status_t golioth_lightdb_delete_sync(
        golioth_client_t client,
        const char* path,
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
golioth_status_t golioth_lightdb_observe_async(
        golioth_client_t client,
        const char* path,
        golioth_get_cb_fn callback,
        void* callback_arg);

//-------------------------------------------------------------------------------
// LightDB Stream
//-------------------------------------------------------------------------------

/// Similar to @ref golioth_lightdb_set_int_async, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_int_async(
        golioth_client_t client,
        const char* path,
        int32_t value,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Similar to @ref golioth_lightdb_set_int_sync, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_int_sync(
        golioth_client_t client,
        const char* path,
        int32_t value,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_set_bool_async, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_bool_async(
        golioth_client_t client,
        const char* path,
        bool value,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Similar to @ref golioth_lightdb_set_bool_sync, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_bool_sync(
        golioth_client_t client,
        const char* path,
        bool value,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_set_float_async, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_float_async(
        golioth_client_t client,
        const char* path,
        float value,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Similar to @ref golioth_lightdb_set_float_sync, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_float_sync(
        golioth_client_t client,
        const char* path,
        float value,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_set_string_async, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_string_async(
        golioth_client_t client,
        const char* path,
        const char* str,
        size_t str_len,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Similar to @ref golioth_lightdb_set_string_sync, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_string_sync(
        golioth_client_t client,
        const char* path,
        const char* str,
        size_t str_len,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_set_json_async, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_json_async(
        golioth_client_t client,
        const char* path,
        const char* json_str,
        size_t json_str_len,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Similar to @ref golioth_lightdb_set_json_sync, but for LightDB stream
golioth_status_t golioth_lightdb_stream_set_json_sync(
        golioth_client_t client,
        const char* path,
        const char* json_str,
        size_t json_str_len,
        int32_t timeout_s);

/// Similar to @ref golioth_lightdb_stream_set_json_async, but for CBOR
golioth_status_t golioth_lightdb_stream_set_cbor_async(
        golioth_client_t client,
        const char* path,
        const uint8_t* cbor_data,
        size_t cbor_data_len,
        golioth_set_cb_fn callback,
        void* callback_arg);

/// Similar to @ref golioth_lightdb_stream_set_json_sync, but for CBOR
golioth_status_t golioth_lightdb_stream_set_cbor_sync(
        golioth_client_t client,
        const char* path,
        const uint8_t* cbor_data,
        size_t cbor_data_len,
        int32_t timeout_s);

/// @}
