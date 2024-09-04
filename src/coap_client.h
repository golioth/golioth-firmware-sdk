/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/client.h>
#include <golioth/config.h>
#include <golioth/golioth_sys.h>
#include "event_group.h"

/// Event group bits for request_complete_event
#define RESPONSE_RECEIVED_EVENT_BIT (1 << 0)
#define RESPONSE_TIMEOUT_EVENT_BIT (1 << 1)

#define BLOCKSIZE_TO_SZX(blockSize) \
    ((blockSize == 16)         ? 0  \
         : (blockSize == 32)   ? 1  \
         : (blockSize == 64)   ? 2  \
         : (blockSize == 128)  ? 3  \
         : (blockSize == 256)  ? 4  \
         : (blockSize == 512)  ? 5  \
         : (blockSize == 1024) ? 6  \
                               : -1)

#define SZX_TO_BLOCKSIZE(szx) ((size_t) (1 << (szx + 4)))

typedef struct
{
    enum golioth_content_type content_type;
    // CoAP payload assumed to be dynamically allocated before enqueue
    // and freed after dequeue.
    uint8_t *payload;
    // Size of payload, in bytes
    size_t payload_size;
    golioth_set_cb_fn callback;
    void *arg;
} golioth_coap_post_params_t;

typedef struct
{
    bool is_last;
    enum golioth_content_type content_type;
    size_t block_index;
    // CoAP payload assumed to be dynamically allocated before enqueue
    // and freed after dequeue.
    uint8_t *payload;
    // Size of payload, in bytes
    size_t payload_size;
    golioth_set_cb_fn callback;
    void *arg;
} golioth_coap_post_block_params_t;

typedef struct
{
    enum golioth_content_type content_type;
    golioth_get_cb_fn callback;
    void *arg;
} golioth_coap_get_params_t;

typedef struct
{
    enum golioth_content_type content_type;
    size_t block_index;
    size_t block_size;
    golioth_get_block_cb_fn callback;
    void *arg;
} golioth_coap_get_block_params_t;

typedef struct
{
    golioth_set_cb_fn callback;
    void *arg;
} golioth_coap_delete_params_t;

typedef struct
{
    enum golioth_content_type content_type;
    golioth_get_cb_fn callback;
    void *arg;
} golioth_coap_observe_params_t;

typedef enum
{
    GOLIOTH_COAP_REQUEST_EMPTY,
    GOLIOTH_COAP_REQUEST_GET,
    GOLIOTH_COAP_REQUEST_GET_BLOCK,
    GOLIOTH_COAP_REQUEST_POST,
    GOLIOTH_COAP_REQUEST_POST_BLOCK,
    GOLIOTH_COAP_REQUEST_DELETE,
    GOLIOTH_COAP_REQUEST_OBSERVE,
    GOLIOTH_COAP_REQUEST_OBSERVE_RELEASE,
} golioth_coap_request_type_t;

typedef struct
{
    struct golioth_client *client;

    // The CoAP path string (everything after coaps://coap.golioth.io/).
    // Assumption: path_prefix is a string literal (i.e. we don't need to strcpy).
    const char *path_prefix;
    char path[CONFIG_GOLIOTH_COAP_MAX_PATH_LEN + 1];
    uint8_t token[8];
    size_t token_len;
    golioth_coap_request_type_t type;
    union
    {
        golioth_coap_get_params_t get;
        golioth_coap_get_block_params_t get_block;
        golioth_coap_post_params_t post;
        golioth_coap_post_block_params_t post_block;
        golioth_coap_delete_params_t delete;
        golioth_coap_observe_params_t observe;
    };
    /// Time (since boot) in milliseconds when request is no longer valid.
    /// This is checked when reqeusts are pulled out of the queue and when responses are received.
    /// Primarily intended to be used for synchronous requests, to avoid blocking forever.
    uint64_t ageout_ms;
    bool got_response;
    bool got_nack;
    enum golioth_status *status;

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

typedef struct
{
    bool in_use;
    golioth_coap_request_msg_t req;
} golioth_coap_observe_info_t;

enum golioth_status golioth_coap_client_empty(struct golioth_client *client,
                                              bool is_synchronous,
                                              int32_t timeout_s);

enum golioth_status golioth_coap_client_set(struct golioth_client *client,
                                            const char *path_prefix,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            const uint8_t *payload,
                                            size_t payload_size,
                                            golioth_set_cb_fn callback,
                                            void *callback_arg,
                                            bool is_synchronous,
                                            int32_t timeout_s);

enum golioth_status golioth_coap_client_set_block(struct golioth_client *client,
                                                  const char *path_prefix,
                                                  const char *path,
                                                  bool is_last,
                                                  enum golioth_content_type content_type,
                                                  size_t block_index,
                                                  const uint8_t *payload,
                                                  size_t payload_size,
                                                  golioth_set_cb_fn callback,
                                                  void *callback_arg,
                                                  bool is_synchronous,
                                                  int32_t timeout_s);

enum golioth_status golioth_coap_client_delete(struct golioth_client *client,
                                               const char *path_prefix,
                                               const char *path,
                                               golioth_set_cb_fn callback,
                                               void *callback_arg,
                                               bool is_synchronous,
                                               int32_t timeout_s);

enum golioth_status golioth_coap_client_get(struct golioth_client *client,
                                            const char *path_prefix,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            golioth_get_cb_fn callback,
                                            void *callback_arg,
                                            bool is_synchronous,
                                            int32_t timeout_s);

enum golioth_status golioth_coap_client_get_block(struct golioth_client *client,
                                                  const char *path_prefix,
                                                  const char *path,
                                                  enum golioth_content_type content_type,
                                                  size_t block_index,
                                                  size_t block_size,
                                                  golioth_get_block_cb_fn callback,
                                                  void *callback_arg,
                                                  bool is_synchronous,
                                                  int32_t timeout_s);

enum golioth_status golioth_coap_client_observe(struct golioth_client *client,
                                                const char *path_prefix,
                                                const char *path,
                                                enum golioth_content_type content_type,
                                                golioth_get_cb_fn callback,
                                                void *callback_arg);

enum golioth_status golioth_coap_client_observe_release(struct golioth_client *client,
                                                        const char *path_prefix,
                                                        const char *path,
                                                        enum golioth_content_type content_type,
                                                        uint8_t *token,
                                                        size_t token_len,
                                                        void *arg);

void golioth_coap_client_cancel_all_observations(struct golioth_client *client);

void golioth_coap_client_cancel_observations_by_prefix(struct golioth_client *client,
                                                       const char *prefix);

/// Getters, for internal SDK code to access data within the
/// coap client struct.
golioth_sys_thread_t golioth_coap_client_get_thread(struct golioth_client *client);
