/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>  // size_t
#include "golioth_status.h"
#include "golioth_sys.h"

/// @defgroup golioth_client golioth_client
/// Functions for creating a Golioth client and managing client lifetime.
///
/// Used to connect to Golioth cloud using the CoAP protocol over DTLS.
///
/// For authentication, you can use either pre-shared key (PSK) or
/// X.509 certificates (aka Public Key Infrastructure, PKI).
///
/// https://docs.golioth.io/reference/protocols/coap
/// <br>
/// https://docs.golioth.io/reference/protocols/coap/auth
/// <br>
/// https://docs.golioth.io/reference/protocols/device-auth
/// @{

/// @brief Opaque handle to the Golioth client
typedef void* golioth_client_t;

/// Golioth client events
typedef enum {
    /// Client was previously not connected, and is now connected
    GOLIOTH_CLIENT_EVENT_CONNECTED,
    /// Client was previously connected, and is now disconnected
    GOLIOTH_CLIENT_EVENT_DISCONNECTED,
} golioth_client_event_t;

/// Response status and CoAP class/code
typedef struct {
    /// Status to indicate whether a response was received
    ///
    /// One of:
    ///      GOLIOTH_ERR_TIMEOUT (no response received from server)
    ///      GOLIOTH_OK (2.XX)
    ///      GOLIOTH_ERR_FAIL (anything other than 2.XX)
    golioth_status_t status;
    /// the 2 in 2.XX
    uint8_t class;
    /// the 03 in 4.03
    uint8_t code;
} golioth_response_t;

/// TLS authentication type
typedef enum {
    /// Authenticate with pre-shared key (psk-id and psk)
    GOLIOTH_TLS_AUTH_TYPE_PSK,
    /// Authenticate with PKI certificates (CA cert, public client cert, private client key)
    GOLIOTH_TLS_AUTH_TYPE_PKI,
} golioth_tls_auth_type_t;

/// Pre-Shared Key (PSK) credentials.
///
/// All memory is owned by user and must persist for the lifetime
/// of the golioth client.
typedef struct {
    /// PSK Identifier (e.g. "devicename@projectname")
    const char* psk_id;
    size_t psk_id_len;

    /// Pre-shared key, secret password
    const char* psk;
    size_t psk_len;
} golioth_psk_credentials_t;

/// Public Key Infrastructure (PKI) credentials (aka "certificates").
///
/// All memory is owned by user and must persist for the lifetime
/// of the golioth client.
typedef struct {
    // PEM Common CA cert
    const uint8_t* ca_cert;
    size_t ca_cert_len;

    /// PEM Public client cert
    const uint8_t* public_cert;
    size_t public_cert_len;

    /// PEM Private client key
    const uint8_t* private_key;
    size_t private_key_len;
} golioth_pki_credentials_t;

/// TLS Authentication Credentials
typedef struct {
    golioth_tls_auth_type_t auth_type;
    union {
        golioth_psk_credentials_t psk;
        golioth_pki_credentials_t pki;
    };
} golioth_tls_credentials_t;

/// Golioth client configuration, passed into golioth_client_create
typedef struct {
    golioth_tls_credentials_t credentials;
} golioth_client_config_t;

/// Callback function type for client events
///
/// @param client The client handle
/// @param event The event that occurred
/// @param arg User argument, copied from @ref golioth_client_register_event_callback. Can be NULL.
typedef void (*golioth_client_event_cb_fn)(
        golioth_client_t client,
        golioth_client_event_t event,
        void* arg);

/// Callback function type for all asynchronous get and observe requests
///
/// Will be called when a response is received or on timeout (i.e. response never received).
/// The callback function should check response->status to determine which case it is (response
/// received or timeout).
///
/// @param client The client handle from the original request.
/// @param response Response status and class/code
/// @param path The path from the original request
/// @param payload The application layer payload in the response packet. Can be NULL.
/// @param payload_size The size of payload, in bytes
/// @param arg User argument, copied from the original request. Can be NULL.
typedef void (*golioth_get_cb_fn)(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg);
typedef void (*golioth_get_block_cb_fn)(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        bool is_last,
        void* arg);

/// Callback function type for all asynchronous set and delete requests
///
/// Will be called when a response is received or on timeout (i.e. response never received).
/// The callback function should check response->status to determine which case it is (response
/// received or timeout). If response->status is GOLIOTH_OK, then the set or delete request
/// was successful.
///
/// @param client The client handle from the original request.
/// @param response Response status and class/code
/// @param path The path from the original request
/// @param arg User argument, copied from the original request. Can be NULL.
typedef void (*golioth_set_cb_fn)(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        void* arg);

/// Create a Golioth client
///
/// Dynamically creates a client and returns an opaque handle to the client.
/// The handle is a required parameter for most other Golioth SDK functions.
///
/// An RTOS task and request queue is created and the client is automatically started (no
/// need to call @ref golioth_client_start).
///
/// @param config Client configuration
///
/// @return Non-NULL The client handle (success)
/// @return NULL There was an error creating the client
golioth_client_t golioth_client_create(const golioth_client_config_t* config);

/// Wait (block) until connected to Golioth, or timeout occurs.
///
/// If timeout_ms set to -1, it will wait forever until connected.
///
/// @param client The client handle
/// @param timeout_ms How long to wait, in milliseconds, or -1 to wait forever
///
/// @return True, if connected, false otherwise.
bool golioth_client_wait_for_connect(golioth_client_t client, int timeout_ms);

/// Start the Golioth client
///
/// Does nothing if the client is already started. The client is started after calling
/// @ref golioth_client_create.
///
/// @param client The client handle
///
/// @return GOLIOTH_OK Client started
/// @return GOLIOTH_ERR_NULL Client handle invalid
golioth_status_t golioth_client_start(golioth_client_t client);

/// Stop the Golioth client
///
/// Client will finish the current request (if there is one), then enter a dormant
/// state where no packets will be sent or received with Golioth, and the client task will be in
/// a blocked state.
///
/// This function will block until the client thread is actually stopped.
///
/// Does nothing if the client is already stopped.
///
/// @param client The client handle
///
/// @return GOLIOTH_OK Client stopped
/// @return GOLIOTH_ERR_NULL Client handle invalid
golioth_status_t golioth_client_stop(golioth_client_t client);

/// Destroy a Golioth client
///
/// Frees dynamically created resources from @ref golioth_client_create.
///
/// @param client The handle of the client to destroy
void golioth_client_destroy(golioth_client_t client);

/// Returns whether the client is currently running
///
/// @param client The client handle
///
/// @return true The client is running
/// @return false The client is not running, or the client handle is not valid
bool golioth_client_is_running(golioth_client_t client);

/// Returns whether the client is currently connected to Golioth servers.
///
/// If client requests receive responses, this is the indication of being connected.
/// If requests time out (no response from server), then the client is considered disconnected.
///
/// @param client The client handle
///
/// @return true The client is connected to Golioth
/// @return false The client is not connected, or the client handle is not valid
bool golioth_client_is_connected(golioth_client_t client);

/// Register a callback that will be called on client events (e.g. connected, disconnected)
///
/// @param client The client handle
/// @param callback Callback function to register
/// @param arg Optional argument, forwarded directly to the callback when invoked. Can be NULL.
void golioth_client_register_event_callback(
        golioth_client_t client,
        golioth_client_event_cb_fn callback,
        void* arg);

/// The number of items currently in the client task request queue.
///
/// Will be a number between 0 and GOLIOTH_COAP_REQUEST_QUEUE_MAX_ITEMS.
///
/// @param client The client handle
///
/// @return The number of items currently in the client task request queue.
uint32_t golioth_client_num_items_in_request_queue(golioth_client_t client);

/// Simulate packet loss at a particular percentage (0 to 100).
///
/// Intended for testing and troubleshooting in packet loss scenarios.
/// Does nothing if percent is outside of the range [0, 100].
///
/// @param percent Percent packet loss (0 is no packets lost, 100 is all packets lost)
void golioth_client_set_packet_loss_percent(uint8_t percent);

/// Return whether there exists an allocated resource that has not been freed by the client.
///
/// Intended only for Golioth SDK developers, for test and debug purposes.
///
/// @return true There is at least one allocation leak (turn on debug logs to see which ones)
/// @return false There are no allocation leaks
bool golioth_client_has_allocation_leaks(void);

/// Return the thread handle of the client thread.
///
/// @param client The client handle
///
/// @return The thread handle of the client thread
golioth_sys_thread_t golioth_client_get_thread(golioth_client_t client);

/// @}
