/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <golioth/golioth_status.h>
#include <golioth/client.h>
#include <golioth/config.h>

/// @defgroup golioth_rpc golioth_rpc
/// Functions for interacting with the Golioth Remote Procedure Call service
/// @{

/// Enumeration of RPC status codes, sent in the RPC response
enum golioth_rpc_status {
    GOLIOTH_RPC_OK = 0,
    GOLIOTH_RPC_CANCELED = 1,
    GOLIOTH_RPC_UNKNOWN = 2,
    GOLIOTH_RPC_INVALID_ARGUMENT = 3,
    GOLIOTH_RPC_DEADLINE_EXCEEDED = 4,
    GOLIOTH_RPC_NOT_FOUND = 5,
    GOLIOTH_RPC_ALREADYEXISTS = 6,
    GOLIOTH_RPC_PERMISSION_DENIED = 7,
    GOLIOTH_RPC_RESOURCE_EXHAUSTED = 8,
    GOLIOTH_RPC_FAILED_PRECONDITION = 9,
    GOLIOTH_RPC_ABORTED = 10,
    GOLIOTH_RPC_OUT_OF_RANGE = 11,
    GOLIOTH_RPC_UNIMPLEMENTED = 12,
    GOLIOTH_RPC_INTERNAL = 13,
    GOLIOTH_RPC_UNAVAILABLE = 14,
    GOLIOTH_RPC_DATA_LOSS = 15,
    GOLIOTH_RPC_UNAUTHENTICATED = 16,
};

/// Callback function type for remote procedure call
///
/// Example of a callback function that implements the "multiply" method, which
/// takes two float parameters, and multiplies them:
///
/// @code{.c}
/// static enum golioth_rpc_status on_multiply(zcbor_state_t *request_params_array,
///                                         zcbor_state_t *response_detail_map,
///                                         void *callback_arg)
/// {
///      double a, b;
///      double value;
///      bool ok;
///
///      ok = zcbor_float_decode(request_params_array, &a) &&
///           zcbor_float_decode(request_params_array, &b);
///      if (!ok) {
///            GLTH_LOGE(TAG, "Failed to decode array items");
///            return GOLIOTH_RPC_INVALID_ARGUMENT;
///      }
///
///      value = a * b;
///
///      ok = zcbor_tstr_put_lit(response_detail_map, "value") &&
///           zcbor_float64_put(response_detail_map, value);
///      if (!ok) {
///            GLTH_LOGE(TAG, "Failed to encode value");
///            return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
///      }
///
///      return GOLIOTH_RPC_OK;
/// }
/// @endcode
///
/// @param request_params_array zcbor decode state, inside of the RPC request params array
/// @param response_detail_map zcbor encode state, inside of the RPC response detail map
/// @param callback_arg callback_arg, unchanged from callback_arg of @ref golioth_rpc_register
///
/// @return GOLIOTH_RPC_OK - if method was called successfully
/// @return GOLIOTH_RPC_INVALID_ARGUMENT - if params were invalid
/// @return otherwise - method failure
typedef enum golioth_rpc_status (*golioth_rpc_cb_fn)(
        zcbor_state_t* request_params_array,
        zcbor_state_t* response_detail_map,
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
enum golioth_status golioth_rpc_register(
        struct golioth_client* client,
        const char* method,
        golioth_rpc_cb_fn callback,
        void* callback_arg);

/// Private struct to contain data about a single registered method
struct golioth_rpc_method {
    const char* method;
    golioth_rpc_cb_fn callback;
    void* callback_arg;
};

/// Private struct to contain RPC state data, stored in
/// the golioth_coap_client_t struct.
struct golioth_rpc {
    struct golioth_rpc_method rpcs[CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS];
    int num_rpcs;
};

/// @}
