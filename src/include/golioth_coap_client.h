/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <coap3/coap.h>  // COAP_MEDIATYPE_*
#include "golioth_client.h"
#include "golioth_lightdb.h"
#include "golioth_settings.h"
#include "golioth_rpc.h"
#include "golioth_config.h"
#include "golioth_sys.h"
#include "golioth_event_group.h"

/// Event group bits for request_complete_event
#define RESPONSE_RECEIVED_EVENT_BIT (1 << 0)
#define RESPONSE_TIMEOUT_EVENT_BIT (1 << 1)

typedef struct {
    // Must be one of:
    //   COAP_MEDIATYPE_APPLICATION_JSON
    //   COAP_MEDIATYPE_APPLICATION_CBOR
    uint32_t content_type;
    // CoAP payload assumed to be dynamically allocated before enqueue
    // and freed after dequeue.
    uint8_t* payload;
    // Size of payload, in bytes
    size_t payload_size;
    golioth_set_cb_fn callback;
    void* arg;
} golioth_coap_post_params_t;

typedef struct {
    uint32_t content_type;
    golioth_get_cb_fn callback;
    void* arg;
} golioth_coap_get_params_t;

typedef struct {
    uint32_t content_type;
    size_t block_index;
    size_t block_size;
    golioth_get_block_cb_fn callback;
    void* arg;
} golioth_coap_get_block_params_t;

typedef struct {
    golioth_set_cb_fn callback;
    void* arg;
} golioth_coap_delete_params_t;

typedef struct {
    uint32_t content_type;
    golioth_get_cb_fn callback;
    void* arg;
} golioth_coap_observe_params_t;

typedef enum {
    GOLIOTH_COAP_REQUEST_EMPTY,
    GOLIOTH_COAP_REQUEST_GET,
    GOLIOTH_COAP_REQUEST_GET_BLOCK,
    GOLIOTH_COAP_REQUEST_POST,
    // TODO - support for POST_BLOCK
    GOLIOTH_COAP_REQUEST_DELETE,
    GOLIOTH_COAP_REQUEST_OBSERVE,
} golioth_coap_request_type_t;

typedef struct {
    // The CoAP path string (everything after coaps://coap.golioth.io/).
    // Assumption: path_prefix is a string literal (i.e. we don't need to strcpy).
    const char* path_prefix;
    char path[CONFIG_GOLIOTH_COAP_MAX_PATH_LEN + 1];
    uint8_t token[8];
    size_t token_len;
    golioth_coap_request_type_t type;
    union {
        golioth_coap_get_params_t get;
        golioth_coap_get_block_params_t get_block;
        golioth_coap_post_params_t post;
        golioth_coap_delete_params_t delete;
        golioth_coap_observe_params_t observe;
    };
    /// Time (since boot) in milliseconds when request is no longer valid.
    /// This is checked when reqeusts are pulled out of the queue and when responses are received.
    /// Primarily intended to be used for synchronous requests, to avoid blocking forever.
    uint64_t ageout_ms;
    bool got_response;
    bool got_nack;

    /// (sync request only) Notification from coap thread to user sync function that
    /// request is completed.
    /// Created in user sync function, deleted by coap thread.
    ///
    /// Bit 0: response received
    /// Bit 1: timeout
    golioth_event_group_t request_complete_event;

    /// (sync request only) Notification from user sync function to coap thread, acknowledge it
    /// received request_complete_event.
    ///
    /// Created in user sync function, deleted by coap thread.
    ///
    /// Used by the coap thread to know when it's safe
    /// to delete request_complete_event and this semaphore.
    golioth_sys_sem_t request_complete_ack_sem;
} golioth_coap_request_msg_t;

typedef struct {
    bool in_use;
    golioth_coap_request_msg_t req;
} golioth_coap_observe_info_t;

golioth_status_t golioth_coap_client_empty(
        golioth_client_t client,
        bool is_synchronous,
        int32_t timeout_s);

golioth_status_t golioth_coap_client_set(
        golioth_client_t client,
        const char* path_prefix,
        const char* path,
        uint32_t content_type,
        const uint8_t* payload,
        size_t payload_size,
        golioth_set_cb_fn callback,
        void* callback_arg,
        bool is_synchronous,
        int32_t timeout_s);

golioth_status_t golioth_coap_client_delete(
        golioth_client_t client,
        const char* path_prefix,
        const char* path,
        golioth_set_cb_fn callback,
        void* callback_arg,
        bool is_synchronous,
        int32_t timeout_s);

golioth_status_t golioth_coap_client_get(
        golioth_client_t client,
        const char* path_prefix,
        const char* path,
        uint32_t content_type,
        golioth_get_cb_fn callback,
        void* callback_arg,
        bool is_synchronous,
        int32_t timeout_s);

golioth_status_t golioth_coap_client_get_block(
        golioth_client_t client,
        const char* path_prefix,
        const char* path,
        uint32_t content_type,
        size_t block_index,
        size_t block_size,
        golioth_get_block_cb_fn callback,
        void* callback_arg,
        bool is_synchronous,
        int32_t timeout_s);

golioth_status_t golioth_coap_client_observe_async(
        golioth_client_t client,
        const char* path_prefix,
        const char* path,
        uint32_t content_type,
        golioth_get_cb_fn callback,
        void* callback_arg);

/// Getters, for internal SDK code to access data within the
/// coap client struct.
golioth_settings_t* golioth_coap_client_get_settings(golioth_client_t client);
golioth_rpc_t* golioth_coap_client_get_rpc(golioth_client_t client);
golioth_sys_thread_t golioth_coap_client_get_thread(golioth_client_t client);
