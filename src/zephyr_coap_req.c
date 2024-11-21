/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(golioth_coap_client);

#include <stdlib.h>

#include <zephyr/random/random.h>

#include "coap_client.h"
#include "zephyr_coap_req.h"
#include "zephyr_coap_utils.h"

static const int64_t COAP_OBSERVE_TS_DIFF_NEWER = 128 * (int64_t) MSEC_PER_SEC;

#define RESEND_REPORT_TIMEFRAME_S 10

#define COAP_RESPONSE_CODE_CLASS(code) ((code) >> 5)

void golioth_coap_reqs_init(struct golioth_client *client)
{
    sys_dlist_init(&client->coap_reqs);
    client->coap_reqs_connected = false;
    k_mutex_init(&client->coap_reqs_lock);
}

static int golioth_coap_req_send(struct golioth_coap_req *req)
{
    return golioth_send_coap(req->client, &req->request);
}

static void golioth_coap_pending_init(struct golioth_coap_pending *pending, uint8_t retries)
{
    pending->t0 = k_uptime_get_32();
    pending->timeout = 0;
    pending->retries = retries;
}

static int __golioth_coap_req_submit(struct golioth_coap_req *req)
{
    struct golioth_client *client = req->client;

    if (!client->coap_reqs_connected)
    {
        return -ENETDOWN;
    }

    sys_dlist_append(&client->coap_reqs, &req->node);

    return 0;
}

static int golioth_coap_req_submit(struct golioth_coap_req *req)
{
    struct golioth_client *client = req->client;
    int err;

    k_mutex_lock(&client->coap_reqs_lock, K_FOREVER);
    err = __golioth_coap_req_submit(req);
    k_mutex_unlock(&client->coap_reqs_lock);

    return err;
}

static void golioth_coap_req_cancel(struct golioth_coap_req *req)
{
    sys_dlist_remove(&req->node);
}

static void golioth_coap_req_cancel_and_free(struct golioth_coap_req *req)
{
    LOG_DBG("cancel and free req %p data %p", (void *) req, (void *) req->request.data);

    golioth_coap_req_cancel(req);
    free(req->request.data);
    free(req);
}

static int golioth_coap_code_to_posix(uint8_t code)
{
    switch (COAP_RESPONSE_CODE_CLASS(code))
    {
        case 2:
            return 0;
        case 4:
            switch (code)
            {
                case COAP_RESPONSE_CODE_BAD_REQUEST:
                    return -EFAULT;
                case COAP_RESPONSE_CODE_UNAUTHORIZED:
                    return -EACCES;
                case COAP_RESPONSE_CODE_BAD_OPTION:
                    return -EINVAL;
                case COAP_RESPONSE_CODE_FORBIDDEN:
                    return -EACCES;
                case COAP_RESPONSE_CODE_NOT_FOUND:
                    return -ENOENT;
                case COAP_RESPONSE_CODE_NOT_ALLOWED:
                    return -EACCES;
                case COAP_RESPONSE_CODE_NOT_ACCEPTABLE:
                    return -EACCES;
                case COAP_RESPONSE_CODE_INCOMPLETE:
                    return -EINVAL;
                case COAP_RESPONSE_CODE_CONFLICT:
                    return -EBUSY;
                case COAP_RESPONSE_CODE_PRECONDITION_FAILED:
                    return -EACCES;
                case COAP_RESPONSE_CODE_REQUEST_TOO_LARGE:
                    return -E2BIG;
                case COAP_RESPONSE_CODE_UNSUPPORTED_CONTENT_FORMAT:
                    return -ENOTSUP;
                case COAP_RESPONSE_CODE_UNPROCESSABLE_ENTITY:
                    return -EBADMSG;
                case COAP_RESPONSE_CODE_TOO_MANY_REQUESTS:
                    return -EBUSY;
            }
            __fallthrough;
        case 5:
            return -EBADMSG;
        default:
            LOG_ERR("Unknown CoAP response code class (%u)",
                    (unsigned int) COAP_RESPONSE_CODE_CLASS(code));
            return -EBADMSG;
    }
}

/* Reordering according to RFC7641 section 3.4 */
static inline bool sequence_number_is_newer(int v1, int v2)
{
    return (v1 < v2 && v2 - v1 < (1 << 23)) || (v1 > v2 && v1 - v2 > (1 << 23));
}

static bool golioth_coap_reply_is_newer(struct golioth_coap_reply *reply, int seq, int64_t uptime)
{
    return (uptime > reply->ts + COAP_OBSERVE_TS_DIFF_NEWER
            || sequence_number_is_newer(reply->seq, seq));
}

static int golioth_coap_req_reply_handler(struct golioth_coap_req *req,
                                          const struct coap_packet *response)
{
    uint16_t payload_len;
    uint8_t code;
    const uint8_t *payload;
    int block2;
    int err;

    code = coap_header_get_code(response);

    LOG_DBG("CoAP response code: 0x%x (class %u detail %u)",
            (unsigned int) code,
            (unsigned int) (code >> 5),
            (unsigned int) (code & 0x1f));

    if (code == COAP_RESPONSE_CODE_BAD_REQUEST)
    {
        LOG_WRN("Server reports CoAP Bad Request. (Check payload formatting)");
    }

    err = golioth_coap_code_to_posix(code);
    if (err)
    {
        struct golioth_req_rsp rsp = {
            .user_data = req->user_data,
            .err = err,
        };

        (void) req->cb(&rsp);

        LOG_DBG("cancel and free req: %p", (void *) req);

        goto cancel_and_free;
    }

    payload = coap_packet_get_payload(response, &payload_len);

    block2 = coap_get_option_int(response, COAP_OPTION_BLOCK2);
    if (block2 != -ENOENT)
    {
        size_t want_offset = req->block_ctx.current;
        size_t cur_offset;
        size_t new_offset;

        err = coap_update_from_block(response, &req->block_ctx);
        if (err)
        {
            struct golioth_req_rsp rsp = {
                .user_data = req->user_data,
                .err = -EBADMSG,
            };

            LOG_ERR("Failed to parse get response: %d", err);

            (void) req->cb(&rsp);

            err = -EBADMSG;
            goto cancel_and_free;
        }

        cur_offset = req->block_ctx.current;
        if (cur_offset < want_offset)
        {
            LOG_WRN("Block at %zu already received, ignoring", cur_offset);

            req->block_ctx.current = want_offset;

            return 0;
        }

        new_offset = coap_next_block_for_option(response, &req->block_ctx, COAP_OPTION_BLOCK2);
        if (new_offset < 0)
        {
            struct golioth_req_rsp rsp = {
                .user_data = req->user_data,
                .err = -EBADMSG,
            };

            LOG_ERR("Failed to move to next block: %zu", new_offset);

            (void) req->cb(&rsp);

            err = -EBADMSG;
            goto cancel_and_free;
        }
        else if (new_offset == 0)
        {
            struct golioth_req_rsp rsp = {
                .data = payload,
                .len = payload_len,
                .off = cur_offset,

                .total = req->block_ctx.total_size,

                .is_last = true,

                .user_data = req->user_data,
            };

            LOG_DBG("Blockwise transfer is finished!");

            (void) req->cb(&rsp);

            goto cancel_and_free;
        }
        else
        {
            struct golioth_req_rsp rsp = {
                .data = payload,
                .len = payload_len,
                .off = cur_offset,

                .total = req->block_ctx.total_size,

                .is_last = false,

                .user_data = req->user_data,

                .err = req->is_observe ? -EMSGSIZE : 0,
            };

            err = req->cb(&rsp);
            if (err)
            {
                goto cancel_and_free;
            }

            if (req->is_observe)
            {
                LOG_ERR("TODO: blockwise observe is not supported");
                err = -ENOTSUP;
            }

            goto cancel_and_free;
        }
    }
    else
    {
        if (coap_get_option_int(response, COAP_OPTION_BLOCK1) >= 0)
        {
            /* This response has block1 */
            if (coap_update_from_block(response, &req->block_ctx) == 0)
            {
                golioth_coap_request_msg_t *rmsg = req->user_data;

                if (req->block_ctx.block_size < rmsg->post_block.block_szx)
                {

                    LOG_DBG("Server wants blocksize: %i intead of: %i",
                            coap_block_size_to_bytes(req->block_ctx.block_size),
                            coap_block_size_to_bytes(rmsg->post_block.block_szx));

                    rmsg->post_block.block_szx = req->block_ctx.block_size;
                }
            }
        }

        struct golioth_req_rsp rsp = {
            .data = payload,
            .len = payload_len,
            .off = 0,

            /* Is it the same as 'req->block_ctx.total_size' ? */
            .total = payload_len,

            .is_last = true,

            .user_data = req->user_data,
        };

        (void) req->cb(&rsp);

        goto cancel_and_free;
    }

cancel_and_free:
    if (req->is_observe && !err)
    {
        req->is_pending = false;
    }
    else
    {
        golioth_coap_req_cancel_and_free(req);
    }

    return 0;
}

void golioth_coap_req_process_rx(struct golioth_client *client, const struct coap_packet *rx)
{
    struct golioth_coap_req *req;
    uint8_t rx_token[COAP_TOKEN_MAX_LEN];
    uint16_t rx_id;
    uint8_t rx_tkl;

    rx_id = coap_header_get_id(rx);
    rx_tkl = coap_header_get_token(rx, rx_token);

    k_mutex_lock(&client->coap_reqs_lock, K_FOREVER);

    SYS_DLIST_FOR_EACH_CONTAINER(&client->coap_reqs, req, node)
    {
        uint16_t req_id = coap_header_get_id(&req->request);
        uint8_t req_token[COAP_TOKEN_MAX_LEN];
        uint8_t req_tkl = coap_header_get_token(&req->request, req_token);
        int observe_seq;

        if (req_id == 0U && req_tkl == 0U)
        {
            continue;
        }

        /* Piggybacked must match id when token is empty */
        if (req_id != rx_id && rx_tkl == 0U)
        {
            continue;
        }

        if (rx_tkl > 0 && memcmp(req_token, rx_token, rx_tkl))
        {
            continue;
        }

        observe_seq = coap_get_option_int(rx, COAP_OPTION_OBSERVE);

        if (observe_seq == -ENOENT)
        {
            golioth_coap_req_reply_handler(req, rx);
        }
        else
        {
            int64_t uptime = k_uptime_get();

            /* handle observed requests only if received in order */
            if (golioth_coap_reply_is_newer(&req->reply, observe_seq, uptime))
            {
                req->reply.seq = observe_seq;
                req->reply.ts = uptime;
                golioth_coap_req_reply_handler(req, rx);
            }
        }

        break;
    }

    k_mutex_unlock(&client->coap_reqs_lock);
}

/**
 * @brief Default response handler
 *
 * Default response handler, which generates error logs in case of errors and debug logs in case of
 * success.
 *
 * @param rsp Response information
 *
 * @retval 0 In every case
 */
static int golioth_req_rsp_default_handler(struct golioth_req_rsp *rsp)
{
    const char *info = rsp->user_data;

    if (rsp->err)
    {
        LOG_ERR("Error response (%s): %d", info ? info : "app", rsp->err);
        return 0;
    }

    LOG_HEXDUMP_DBG(rsp->data, rsp->len, info ? info : "RSP");

    return 0;
}

static enum coap_block_size max_block_size_from_payload_len(uint16_t payload_len)
{
    enum coap_block_size block_size = COAP_BLOCK_16;

    payload_len /= 16;

    while (payload_len > 1 && block_size < COAP_BLOCK_1024)
    {
        block_size++;
        payload_len /= 2;
    }

    return block_size;
}

static enum coap_block_size golioth_estimated_coap_block_size(struct golioth_client *client)
{
    return max_block_size_from_payload_len(client->rx_buffer_len);
}

static int golioth_coap_req_init(struct golioth_coap_req *req,
                                 struct golioth_client *client,
                                 enum coap_method method,
                                 enum coap_msgtype msg_type,
                                 uint8_t *buffer,
                                 size_t buffer_len,
                                 golioth_req_cb_t cb,
                                 void *user_data)
{
    int err;

    err = coap_packet_init(&req->request,
                           buffer,
                           buffer_len,
                           COAP_VERSION_1,
                           msg_type,
                           COAP_TOKEN_MAX_LEN,
                           coap_next_token(),
                           method,
                           coap_next_id());
    if (err)
    {
        return err;
    }

    req->client = client;
    req->cb = (cb ? cb : golioth_req_rsp_default_handler);
    req->user_data = user_data;
    req->request_wo_block2.data = NULL;
    req->request_wo_block1.data = NULL;
    req->reply.seq = 0;
    req->reply.ts = -COAP_OBSERVE_TS_DIFF_NEWER;

    coap_block_transfer_init(&req->block_ctx, golioth_estimated_coap_block_size(client), 0);

    return 0;
}

int golioth_coap_req_schedule(struct golioth_coap_req *req)
{
    struct golioth_client *client = req->client;
    int err;

    golioth_coap_pending_init(&req->pending, 3);

    err = golioth_coap_req_submit(req);
    if (err)
    {
        return err;
    }

    if (client->wakeup)
    {
        client->wakeup(client);
    }

    return 0;
}

int golioth_coap_req_new(struct golioth_coap_req **req,
                         struct golioth_client *client,
                         enum coap_method method,
                         enum coap_msgtype msg_type,
                         size_t buffer_len,
                         golioth_req_cb_t cb,
                         void *user_data)
{
    uint8_t *buffer;
    int err;

    *req = calloc(1, sizeof(**req));
    if (!(*req))
    {
        LOG_ERR("Failed to allocate request");
        return -ENOMEM;
    }

    buffer = malloc(buffer_len);
    if (!buffer)
    {
        LOG_ERR("Failed to allocate packet buffer");
        err = -ENOMEM;
        goto free_req;
    }

    err = golioth_coap_req_init(*req, client, method, msg_type, buffer, buffer_len, cb, user_data);
    if (err)
    {
        LOG_ERR("Failed to initialize CoAP GET request: %d", err);
        goto free_buffer;
    }

    return 0;

free_buffer:
    free(buffer);

free_req:
    free(*req);

    return err;
}

void golioth_coap_req_free(struct golioth_coap_req *req)
{
    free(req->request.data); /* buffer */
    free(req);
}

int golioth_coap_req_cb(struct golioth_client *client,
                        enum coap_method method,
                        const uint8_t **pathv,
                        enum coap_content_format format,
                        const uint8_t *data,
                        size_t data_len,
                        golioth_req_cb_t cb,
                        void *user_data,
                        int flags)
{
    size_t path_len = coap_pathv_estimate_alloc_len(pathv);
    struct golioth_coap_req *req;
    int err;

    err = golioth_coap_req_new(&req,
                               client,
                               method,
                               COAP_TYPE_CON,
                               GOLIOTH_COAP_MAX_NON_PAYLOAD_LEN + path_len + data_len,
                               cb,
                               user_data);
    if (err)
    {
        LOG_ERR("Failed to create new CoAP GET request: %d", err);
        return err;
    }

    if (method == COAP_METHOD_GET && (flags & GOLIOTH_COAP_REQ_OBSERVE))
    {
        req->is_observe = true;
        req->is_pending = true;

        err = coap_append_option_int(&req->request, COAP_OPTION_OBSERVE, 0 /* register */);
        if (err)
        {
            LOG_ERR("Unable add observe option");
            goto free_req;
        }
    }

    err = coap_packet_append_uri_path_from_pathv(&req->request, pathv);
    if (err)
    {
        LOG_ERR("Unable add uri path to packet");
        goto free_req;
    }

    if (method != COAP_METHOD_GET && method != COAP_METHOD_DELETE)
    {
        err = coap_append_option_int(&req->request, COAP_OPTION_CONTENT_FORMAT, format);
        if (err)
        {
            LOG_ERR("Unable add content format to packet");
            goto free_req;
        }
    }

    if (!(flags & GOLIOTH_COAP_REQ_NO_RESP_BODY))
    {
        err = coap_append_option_int(&req->request, COAP_OPTION_ACCEPT, format);
        if (err)
        {
            LOG_ERR("Unable add content format to packet");
            goto free_req;
        }
    }

    if (data && data_len)
    {
        err = coap_packet_append_payload_marker(&req->request);
        if (err)
        {
            LOG_ERR("Unable add payload marker to packet");
            goto free_req;
        }

        err = coap_packet_append_payload(&req->request, data, data_len);
        if (err)
        {
            LOG_ERR("Unable add payload to packet");
            goto free_req;
        }
    }

    err = golioth_coap_req_schedule(req);
    if (err)
    {
        goto free_req;
    }

    return 0;

free_req:
    golioth_coap_req_free(req);

    return err;
}

static uint32_t init_ack_timeout(void)
{
#if defined(CONFIG_COAP_RANDOMIZE_ACK_TIMEOUT)
    const uint32_t max_ack = CONFIG_COAP_INIT_ACK_TIMEOUT_MS * CONFIG_COAP_ACK_RANDOM_PERCENT / 100;
    const uint32_t min_ack = CONFIG_COAP_INIT_ACK_TIMEOUT_MS;

    /* Randomly generated initial ACK timeout
     * ACK_TIMEOUT < INIT_ACK_TIMEOUT < ACK_TIMEOUT * ACK_RANDOM_FACTOR
     * Ref: https://tools.ietf.org/html/rfc7252#section-4.8
     */
    return min_ack + (sys_rand32_get() % (max_ack - min_ack));
#else
    return CONFIG_COAP_INIT_ACK_TIMEOUT_MS;
#endif /* defined(CONFIG_COAP_RANDOMIZE_ACK_TIMEOUT) */
}

static bool golioth_coap_pending_cycle(struct golioth_coap_pending *pending)
{
    if (pending->timeout == 0)
    {
        /* Initial transmission. */
        pending->timeout = init_ack_timeout();

        return true;
    }

    if (pending->retries == 0)
    {
        return false;
    }

    pending->t0 += pending->timeout;
    pending->timeout = pending->timeout << 1;
    pending->retries--;

    return true;
}

static int64_t golioth_coap_req_poll_prepare(struct golioth_coap_req *req, uint32_t now)
{
    int64_t timeout;
    bool send = false;
    bool resend = (req->pending.timeout != 0);
    int err;

    while (true)
    {
        timeout = (int32_t) (req->pending.t0 + req->pending.timeout) - (int32_t) now;

        if (timeout > 0)
        {
            /* Return timeout when packet still waits for response/ack */
            break;
        }

        send = golioth_coap_pending_cycle(&req->pending);
        if (!send)
        {
            struct golioth_req_rsp rsp = {
                .user_data = req->user_data,
                .err = -ETIMEDOUT,
            };

            LOG_WRN("Packet %p (reply %p) was not replied to", (void *) req, (void *) &req->reply);

            (void) req->cb(&rsp);

            golioth_coap_req_cancel_and_free(req);

            return INT64_MAX;
        }
    }

    if (send)
    {
        if (resend)
        {
            LOG_DBG("Resending request %p (reply %p) (retries %d)",
                    (void *) req,
                    (void *) &req->reply,
                    (int) req->pending.retries);

            req->client->resend_report_count++;
        }

        err = golioth_coap_req_send(req);
        if (err)
        {
            LOG_ERR("Send error: %d", err);
        }
    }

    if (req->client->resend_report_count
        && ((now - req->client->resend_report_last_ms) >= (RESEND_REPORT_TIMEFRAME_S * 1000)))
    {
        LOG_WRN("%u resends in last %d seconds",
                req->client->resend_report_count,
                RESEND_REPORT_TIMEFRAME_S);
        req->client->resend_report_last_ms = now;
        req->client->resend_report_count = 0;
    }

    return timeout;
}

static int64_t __golioth_coap_reqs_poll_prepare(struct golioth_client *client, int64_t now)
{
    struct golioth_coap_req *req, *next;
    int64_t min_timeout = INT64_MAX;

    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&client->coap_reqs, req, next, node)
    {
        if (req->is_observe && !req->is_pending)
        {
            continue;
        }

        int64_t req_timeout = golioth_coap_req_poll_prepare(req, now);

        min_timeout = MIN(min_timeout, req_timeout);
    }

    return min_timeout;
}

int64_t golioth_coap_reqs_poll_prepare(struct golioth_client *client, int64_t now)
{
    int64_t timeout;

    k_mutex_lock(&client->coap_reqs_lock, K_FOREVER);
    timeout = __golioth_coap_reqs_poll_prepare(client, now);
    k_mutex_unlock(&client->coap_reqs_lock);

    return timeout;
}

static void golioth_coap_reqs_cancel_all_with_reason(struct golioth_client *client, int reason)
{
    struct golioth_coap_req *req, *next;

    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&client->coap_reqs, req, next, node)
    {
        struct golioth_req_rsp rsp = {
            .user_data = req->user_data,
            .err = reason,
        };

        if (!req->is_observe)
        {
            (void) req->cb(&rsp);
        }

        golioth_coap_req_cancel_and_free(req);
    }
}

static int __golioth_coap_req_find_and_cancel_observation(
    struct golioth_client *client,
    golioth_coap_request_msg_t *cancel_req_msg)
{
    struct golioth_coap_req *req, *next;

    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&client->coap_reqs, req, next, node)
    {
        if ((req->user_data == cancel_req_msg) && (req->is_observe))
        {
            int err;
            uint8_t coap_token[COAP_TOKEN_MAX_LEN];
            size_t coap_token_len = coap_header_get_token(&req->request, coap_token);
            int coap_content_format = coap_get_option_int(&req->request, COAP_OPTION_ACCEPT);

            if (coap_token_len == 0)
            {
                LOG_ERR("Unable to get coap token from request. Got length: %d", coap_token_len);
                err = GOLIOTH_ERR_NO_MORE_DATA;
                goto remove_from_coap_reqs_and_free;
            }

            if (coap_content_format < 0)
            {
                LOG_ERR("Unable to get coap content format from request: %d", coap_content_format);
                err = GOLIOTH_ERR_INVALID_FORMAT;
                goto remove_from_coap_reqs_and_free;
            }

            golioth_coap_request_msg_t *req_msg = req->user_data;

            /* Enqueue an "eager release" request for this observation */
            err = golioth_coap_client_observe_release(client,
                                                      req_msg->path_prefix,
                                                      req_msg->path,
                                                      coap_content_format,
                                                      coap_token,
                                                      coap_token_len,
                                                      NULL);
            if (err)
            {
                LOG_ERR("Error encoding observe release request: %d", err);
            }

        remove_from_coap_reqs_and_free:
            golioth_coap_req_cancel_and_free(req);
            return err;
        }
    }

    return GOLIOTH_ERR_NO_MORE_DATA;
}

int golioth_coap_req_find_and_cancel_observation(struct golioth_client *client,
                                                 golioth_coap_request_msg_t *cancel_req_msg)
{
    k_mutex_lock(&client->coap_reqs_lock, K_FOREVER);
    int err = __golioth_coap_req_find_and_cancel_observation(client, cancel_req_msg);
    k_mutex_unlock(&client->coap_reqs_lock);

    return err;
}

void golioth_coap_reqs_on_connect(struct golioth_client *client)
{
    k_mutex_lock(&client->coap_reqs_lock, K_FOREVER);

    /*
     * client->sock is protected by client->lock, so submitting new coap_req
     * requests would potentially block on other thread currently receiving
     * or sending data using golioth_{recv,send} APIs.
     *
     * Hence use another client->coap_reqs_connected to save information
     * whether we are connected or not.
     */
    client->coap_reqs_connected = true;

    k_mutex_unlock(&client->coap_reqs_lock);
}

void golioth_coap_reqs_on_disconnect(struct golioth_client *client)
{
    k_mutex_lock(&client->coap_reqs_lock, K_FOREVER);

    client->coap_reqs_connected = false;
    golioth_coap_reqs_cancel_all_with_reason(client, -ESHUTDOWN);

    k_mutex_unlock(&client->coap_reqs_lock);
}
