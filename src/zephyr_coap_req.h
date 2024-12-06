/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#include "coap_client.h"
#include "coap_client_zephyr.h"

/**
 *  @defgroup golioth_coap_req_flags CoAP request flags
 *  @{
 */

/** CoAP request is an observation */
#define GOLIOTH_COAP_REQ_OBSERVE BIT(0)
/** CoAP request does not expect response with payload */
#define GOLIOTH_COAP_REQ_NO_RESP_BODY BIT(1)

/** @} */

/**
 * @brief Information about a request awaiting for an acknowledgment (ACK).
 *
 * @note Modeled after #coap_pending
 */
struct golioth_coap_pending
{
    uint32_t t0;
    uint32_t timeout;
    uint8_t retries;
};

/**
 * @brief Stores information about pending reply of a request.
 *
 * @note Modeled after #coap_reply
 */
struct golioth_coap_reply
{
    int seq;    /* needed for observations only */
    int64_t ts; /* needed for observations only */
};

/**
 * @brief Information about pending CoAP request
 */
struct golioth_coap_req
{
    sys_dnode_t node;
    struct coap_packet request;
    // TODO: Can we use request_wo_block_option for both request_wo_block1 and 2?
    struct coap_packet request_wo_block1;
    struct coap_packet request_wo_block2;
    // TODO: block_ctx for both block1 and block2?
    struct coap_block_context block_ctx;
    struct golioth_coap_reply reply;

    struct golioth_coap_pending pending;
    bool is_observe;
    bool is_pending;

    struct golioth_client *client;

    golioth_req_cb_t cb;
    void *user_data;
};

/**
 * @brief Initialize CoAP requests for client
 *
 * Initializes CoAP requests handling for Golioth client instance.
 *
 * @param[inout] client Client instance
 */
void golioth_coap_reqs_init(struct golioth_client *client);

/**
 * @brief Allocate and initialize new CoAP request
 *
 * Allocates new CoAP request, buffer for data (according to @p buffer_len) and initializes it, so
 * it is ready to be filled in (e.g. with coap_packet_append_option()) and scheduled for sending
 * with golioth_coap_req_schedule().
 *
 * @param[out] req CoAP request, allocated and initialized
 * @param[in] client Client instance
 * @param[in] method CoAP request method
 * @param[in] msg_type CoAP message type
 * @param[in] buffer_len Length of buffer for CoAP packet
 * @param[in] cb Callback executed on response received, timeout or error
 * @param[in] user_data User data passed to @p cb
 *
 * @retval 0 On success
 * @retval <0 On failure
 */
int golioth_coap_req_new(struct golioth_coap_req **req,
                         struct golioth_client *client,
                         enum coap_method method,
                         enum coap_msgtype msg_type,
                         size_t buffer_len,
                         golioth_req_cb_t cb,
                         void *user_data);

/**
 * @brief Free CoAP request
 *
 * @param[in] req CoAP request to be freed
 */
void golioth_coap_req_free(struct golioth_coap_req *req);

/**
 * @brief Schedule CoAP request for sending
 *
 * Schedule CoAP request for sending and
 *
 * @param[in] req CoAP request to be scheduled for sending
 *
 * @retval 0 On success
 * @retval <0 On failure
 */
int golioth_coap_req_schedule(struct golioth_coap_req *req);

/**
 * @brief Cancel a CoAP observation
 *
 * Search the client coap_reqs array for a request that has a user_data pointer that matches the
 * provided goloth_coap_request_msg_t pointer. Enqueue an observation release request to be sent to
 * the server. Remove the request from the client coap_reqs list and free the memory.
 *
 * @param[in] client Client instance
 * @param[in] cancel_req_msg pointer to request message used to match with CoAP request
 *
 * @retval 0 On success
 * @retval <0 On failure
 */
int golioth_coap_req_find_and_cancel_observation(struct golioth_client *client,
                                                 struct golioth_coap_request_msg *cancel_req_msg);

/**
 * @brief Create and schedule CoAP request for sending
 *
 * This is a combination of golioth_coap_req_new() and golioth_coap_req_schedule() with most common
 * CoAP options (controlled/selected by @p flags) and request body appended to allocated CoAP
 * packet.
 *
 * @param[in] client Client instance
 * @param[in] method CoAP request method
 * @param[in] pathv Array of CoAP path components
 * @param[in] format Content type
 * @param[in] data CoAP request payload (NULL if no payload should be appended)
 * @param[in] data_len Length of CoAP request payload
 * @param[in] cb Callback executed on response received, timeout or error. Can be NULL.
 * @param[in] user_data User data passed to @p cb
 * @param[in] flags Flags (@sa golioth_coap_req_flags)
 *
 * @retval 0 On success
 * @retval <0 On failure
 */
int golioth_coap_req_cb(struct golioth_client *client,
                        enum coap_method method,
                        const uint8_t **pathv,
                        enum coap_content_format format,
                        const uint8_t *data,
                        size_t data_len,
                        golioth_req_cb_t cb,
                        void *user_data,
                        int flags);

/**
 * @brief Schedule CoAP request and synchronously wait for response
 *
 * Synchronous version of golioth_coap_req_cb(). It waits for response/timeout/error and returns
 * status as return value.
 *
 * @param[in] client Client instance
 * @param[in] method CoAP request method
 * @param[in] pathv Array of CoAP path components
 * @param[in] format Content type
 * @param[in] data CoAP request payload (NULL if no payload should be appended)
 * @param[in] data_len Length of CoAP request payload
 * @param[in] cb Callback executed on response received, timeout or error
 * @param[in] user_data User data passed to @p cb
 * @param[in] flags Flags (@sa golioth_coap_req_flags)
 *
 * @retval 0 On success
 * @retval <0 On failure
 */
int golioth_coap_req_sync(struct golioth_client *client,
                          enum coap_method method,
                          const uint8_t **pathv,
                          enum coap_content_format format,
                          const uint8_t *data,
                          size_t data_len,
                          golioth_req_cb_t cb,
                          void *user_data,
                          int flags);

/**
 * @brief Handle CoAP packets (re)transmission and timeout
 *
 * Handles timeout of the next CoAP request retransmission, in case it was not responded to. If
 * maximum number of retries was reached, then request is dropped with notification using
 * #golioth_coap_req_cb_t callback. Since all created requests are scheduled with timeout of 0, it
 * means that this function also handles sending of the request for the first time.
 *
 * @param[in] client Client instance
 * @param[in] now Timestamp in msec of current event loop (usually output of k_uptime_get())
 *
 * @retval INT64_MAX Infinite timeout (in case request reached maximum retranmissions and was
 *                   dropped)
 * @retval <INT64_MAX Timeout in msec starting from @p now when next retransmission will happen (or
 *                    when packet will be dropped due to reaching maximum number of
 *                    retransmissions).
 */
int64_t golioth_coap_reqs_poll_prepare(struct golioth_client *client, int64_t now);

/**
 * @brief Process received CoAP packet
 *
 * Iterates through all pending CoAP requests and checks if received packet is a response to one of
 * those requests. Calls #golioth_req_coap_cb_t callback in case of received response.
 *
 * @param[in] client Client instance
 * @param[in] rx Received CoAP packet
 */
void golioth_coap_req_process_rx(struct golioth_client *client, const struct coap_packet *rx);

/**
 * @brief Process "connect" event in CoAP requests module
 *
 * @param[in] client Client instance
 */
void golioth_coap_reqs_on_connect(struct golioth_client *client);

/**
 * @brief Process "disconnect" event in CoAP requests module
 *
 * @param[in] client Client instance
 */
void golioth_coap_reqs_on_disconnect(struct golioth_client *client);
