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

#define GOLIOTH_COAP_TOKEN_LEN 8

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

/// Internal callback for blockwise get requests
///
/// Will be called repeatedly, once for each block received from the server, on timeout (i.e.
/// response never received), or when the request is cancelled (e.g. when the Golioth client is
/// stopped). The callback function should check \p status to determine which case triggered the
/// callback.
///
/// The CoAP response code is available by reading code_class (2.xx) and code_detail (x.00) from \p
/// coap_rsp_code:
/// - When \p status is GOLIOTH_OK, \p coap_rsp_code->code_class will be a 2.XX success code.
/// - When \p status is GOLIOTH_COAP_RESPONSE_CODE, check \p coap_rsp_code->code_class for a
/// non-success code (4.XX, etc.)
/// - All other \p status values indicate an error in which a response was not received. The \p
/// coap_rsp_code is undefined and will be NULL.
///
/// @param client The client handle from the original request.
/// @param status Golioth status code.
/// @param coap_rsp_code CoAP response code received from Golioth. Can be NULL.
/// @param path The path from the original request
/// @param payload The application layer payload in the response packet. Can be NULL.
/// @param payload_size The size of payload, in bytes
/// @param is_last True if this is the final block of the get request
/// @param arg User argument, copied from the original request. Can be NULL.
///
typedef void (*coap_get_block_cb_fn)(struct golioth_client *client,
                                     enum golioth_status status,
                                     const struct golioth_coap_rsp_code *coap_rsp_code,
                                     const char *path,
                                     const uint8_t *payload,
                                     size_t payload_size,
                                     bool is_last,
                                     void *arg);
struct golioth_coap_post_params
{
    enum golioth_content_type content_type;
    // CoAP payload assumed to be dynamically allocated before enqueue
    // and freed after dequeue.
    uint8_t *payload;
    // Size of payload, in bytes
    size_t payload_size;
    union
    {
        golioth_set_cb_fn callback_set;
        golioth_post_cb_fn callback_post;
    };
    void *arg;
    bool callback_is_post;
};

struct golioth_coap_post_block_params
{
    bool is_last;
    enum golioth_content_type content_type;
    size_t block_index;
    size_t block_szx;
    // CoAP payload assumed to be dynamically allocated before enqueue
    // and freed after dequeue.
    uint8_t *payload;
    // Size of payload, in bytes
    size_t payload_size;
    golioth_set_block_cb_fn callback;
    coap_get_block_cb_fn rsp_callback;
    void *arg;
    void *rsp_arg;
};

struct golioth_coap_get_params
{
    enum golioth_content_type content_type;
    golioth_get_cb_fn callback;
    void *arg;
};

struct golioth_coap_get_block_params
{
    enum golioth_content_type content_type;
    size_t block_index;
    size_t block_size;
    coap_get_block_cb_fn callback;
    void *arg;
};

struct golioth_coap_delete_params
{
    golioth_set_cb_fn callback;
    void *arg;
};

struct golioth_coap_observe_params
{
    enum golioth_content_type content_type;
    golioth_get_cb_fn callback;
    void *arg;
};

enum golioth_coap_request_type
{
    GOLIOTH_COAP_REQUEST_EMPTY,
    GOLIOTH_COAP_REQUEST_GET,
    GOLIOTH_COAP_REQUEST_GET_BLOCK,
    GOLIOTH_COAP_REQUEST_POST,
    GOLIOTH_COAP_REQUEST_POST_BLOCK,
    GOLIOTH_COAP_REQUEST_POST_BLOCK_RSP,
    GOLIOTH_COAP_REQUEST_DELETE,
    GOLIOTH_COAP_REQUEST_OBSERVE,
    GOLIOTH_COAP_REQUEST_OBSERVE_RELEASE,
};

struct golioth_coap_request_msg
{
    struct golioth_client *client;

    // The CoAP path string (everything after coaps://coap.golioth.io/).
    // Assumption: path_prefix is a string literal (i.e. we don't need to strcpy).
    const char *path_prefix;
    char path[CONFIG_GOLIOTH_COAP_MAX_PATH_LEN + 1];
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    enum golioth_coap_request_type type;
    union
    {
        struct golioth_coap_get_params get;
        struct golioth_coap_get_block_params get_block;
        struct golioth_coap_post_params post;
        struct golioth_coap_post_block_params post_block;
        struct golioth_coap_delete_params delete;
        struct golioth_coap_observe_params observe;
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
};

struct golioth_coap_observe_info
{
    bool in_use;
    struct golioth_coap_request_msg req;
};

/// Create the mutex that makes CoAP token generation thread-safe.
void golioth_coap_token_mutex_create(void);

/// Generate a unique CoAP token.
///
/// @param token byte array where new token will be stored.
void golioth_coap_next_token(uint8_t token[GOLIOTH_COAP_TOKEN_LEN]);

enum golioth_status golioth_coap_client_empty(struct golioth_client *client,
                                              bool is_synchronous,
                                              int32_t timeout_s);

enum golioth_status golioth_coap_client_post(struct golioth_client *client,
                                             const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                             const char *path_prefix,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             const uint8_t *payload,
                                             size_t payload_size,
                                             golioth_post_cb_fn callback,
                                             void *callback_arg,
                                             bool is_synchronous,
                                             int32_t timeout_s);

enum golioth_status golioth_coap_client_set(struct golioth_client *client,
                                            const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
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
                                                  const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                                  const char *path_prefix,
                                                  const char *path,
                                                  bool is_last,
                                                  enum golioth_content_type content_type,
                                                  size_t block_index,
                                                  size_t block_szx,
                                                  const uint8_t *payload,
                                                  size_t payload_size,
                                                  golioth_set_block_cb_fn callback,
                                                  void *callback_arg,
                                                  coap_get_block_cb_fn rsp_callback,
                                                  void *rsp_cb_arg,
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
                                            const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                            const char *path_prefix,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            golioth_get_cb_fn callback,
                                            void *callback_arg,
                                            bool is_synchronous,
                                            int32_t timeout_s);

enum golioth_status golioth_coap_client_get_block(struct golioth_client *client,
                                                  const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                                  const char *path_prefix,
                                                  const char *path,
                                                  enum golioth_content_type content_type,
                                                  size_t block_index,
                                                  size_t block_size,
                                                  coap_get_block_cb_fn callback,
                                                  void *callback_arg,
                                                  bool is_synchronous,
                                                  int32_t timeout_s);

enum golioth_status golioth_coap_client_get_rsp_block(struct golioth_client *client,
                                                      const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                                      const char *path_prefix,
                                                      const char *path,
                                                      enum golioth_content_type content_type,
                                                      size_t block_index,
                                                      size_t block_size,
                                                      coap_get_block_cb_fn callback,
                                                      void *arg,
                                                      bool is_synchronous,
                                                      int32_t timeout_s);

enum golioth_status golioth_coap_client_observe(struct golioth_client *client,
                                                const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                                const char *path_prefix,
                                                const char *path,
                                                enum golioth_content_type content_type,
                                                golioth_get_cb_fn callback,
                                                void *callback_arg);

enum golioth_status golioth_coap_client_observe_release(struct golioth_client *client,
                                                        const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                                        const char *path_prefix,
                                                        const char *path,
                                                        enum golioth_content_type content_type,
                                                        void *arg);

void golioth_coap_client_cancel_all_observations(struct golioth_client *client);

void golioth_coap_client_cancel_observations_by_prefix(struct golioth_client *client,
                                                       const char *prefix);

/// Getters, for internal SDK code to access data within the
/// coap client struct.
golioth_sys_thread_t golioth_coap_client_get_thread(struct golioth_client *client);
