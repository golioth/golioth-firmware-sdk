/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cJSON.h>
#include "golioth_status.h"
#include "golioth_client.h"
#include "golioth_config.h"

/// @defgroup golioth_rpc golioth_rpc
/// Functions for interacting with the Golioth Remote Procedure Call service
/// @{

/// Enumeration of RPC status codes, sent in the RPC response
typedef enum {
    RPC_OK = 0,
    RPC_CANCELED = 1,
    RPC_UNKNOWN = 2,
    RPC_INVALID_ARGUMENT = 3,
    RPC_DEADLINE_EXCEEDED = 4,
    RPC_NOT_FOUND = 5,
    RPC_ALREADYEXISTS = 6,
    RPC_PERMISSION_DENIED = 7,
    RPC_RESOURCE_EXHAUSTED = 8,
    RPC_FAILED_PRECONDITION = 9,
    RPC_ABORTED = 10,
    RPC_OUT_OF_RANGE = 11,
    RPC_UNIMPLEMENTED = 12,
    RPC_INTERNAL = 13,
    RPC_UNAVAILABLE = 14,
    RPC_DATA_LOSS = 15,
    RPC_UNAUTHENTICATED = 16,
} golioth_rpc_status_t;

/// Callback function type for remote procedure call
///
/// Example of a callback function that implements the "double" method, which
/// takes one integer parameter, and doubles it:
///
/// @code{.c}
/// static golioth_rpc_status_t on_double(
///         const char* method,
///         const cJSON* params,
///         uint8_t* detail,
///         size_t detail_size,
///         void* callback_arg) {
///     if (cJSON_GetArraySize(params) != 1) {
///         return RPC_INVALID_ARGUMENT;
///     }
///     int num_to_double = cJSON_GetArrayItem(params, 0)->valueint;
///     snprintf((char*)detail, detail_size, "{ \"value\": %d }", 2 * num_to_double);
///     return RPC_OK;
/// }
/// @endcode
///
/// @param method The RPC method name, NULL-terminated
/// @param params A cJSON* handle of the "params" array. Callback functions
///         can use cJSON_GetArrayItem(params, index)->valueX to extract individual
///         parameters.
/// @param detail Output buffer for additional JSON detail (optional).
///         Can be populated with string-encoded JSON to return one or more
///         values from the method.
/// @param detail_size Size of the detail buffer, in bytes
/// @param callback_arg callback_arg, unchanged from callback_arg of @ref golioth_rpc_register
///
/// @return RPC_OK - if method was called successfully
/// @return RPC_INVALID_ARGUMENT - if params were invalid
/// @return otherwise - method failure
typedef golioth_rpc_status_t (*golioth_rpc_cb_fn)(
        const char* method,
        const cJSON* params,
        uint8_t* detail,
        size_t detail_size,
        void* callback_arg);

/// Register an RPC method
///
/// @param client Golioth client handle
/// @param method The name of the method to register
/// @param callback The callback to be invoked, when an RPC request with matching method name
///         is received by the client.
/// @param callback_arg User data forwarded to callback when invoked. Optional, can be NULL.
///
/// @return GOLIOTH_OK - RPC method successfully registered
/// @return otherwise - Error registering RPC method
golioth_status_t golioth_rpc_register(
        golioth_client_t client,
        const char* method,
        golioth_rpc_cb_fn callback,
        void* callback_arg);

/// Private struct to contain data about a single registered method
typedef struct {
    const char* method;
    golioth_rpc_cb_fn callback;
    void* callback_arg;
} golioth_rpc_method_t;

/// Private struct to contain RPC state data, stored in
/// the golioth_coap_client_t struct.
typedef struct {
    golioth_rpc_method_t rpcs[CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS];
    int num_rpcs;
} golioth_rpc_t;

/// @}
