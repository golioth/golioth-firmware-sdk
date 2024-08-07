/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <string.h>
#include <netdb.h>      // struct addrinfo
#include <sys/param.h>  // MIN
#include <time.h>
#include <coap3/coap.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include "coap_client.h"
#include "golioth_util.h"
#include "mbox.h"
#include "coap_client_libcoap.h"

LOG_TAG_DEFINE(golioth_coap_client_libcoap);

static bool _initialized;

static bool token_matches_request(const golioth_coap_request_msg_t *req, const coap_pdu_t *pdu)
{
    coap_bin_const_t rcvd_token = coap_pdu_get_token(pdu);
    bool len_matches = (rcvd_token.length == req->token_len);
    return (len_matches && (0 == memcmp(rcvd_token.s, req->token, req->token_len)));
}

static void notify_observers(const coap_pdu_t *received,
                             struct golioth_client *client,
                             const uint8_t *data,
                             size_t data_len,
                             const struct golioth_response *response)
{
    // scan observations, check for token match
    for (int i = 0; i < CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS; i++)
    {
        const golioth_coap_observe_info_t *obs_info = &client->observations[i];
        golioth_get_cb_fn callback = obs_info->req.observe.callback;

        if (!obs_info->in_use || !callback)
        {
            continue;
        }

        coap_bin_const_t rcvd_token = coap_pdu_get_token(received);
        bool len_matches = (rcvd_token.length == obs_info->req.token_len);
        if (len_matches
            && (0 == memcmp(rcvd_token.s, obs_info->req.token, obs_info->req.token_len)))
        {
            callback(client,
                     response,
                     obs_info->req.path,
                     data,
                     data_len,
                     obs_info->req.observe.arg);
        }
    }
}

static coap_response_t coap_response_handler(coap_session_t *session,
                                             const coap_pdu_t *sent,
                                             const coap_pdu_t *received,
                                             const coap_mid_t mid)
{
    coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);
    coap_pdu_type_t rcv_type = coap_pdu_get_type(received);
    uint8_t class = rcvd_code >> 5;
    uint8_t code = rcvd_code & 0x1F;

    if (rcv_type == COAP_MESSAGE_RST)
    {
        GLTH_LOGW(TAG, "Got RST");
        return COAP_RESPONSE_OK;
    }

    struct golioth_response response = {
        .status =
            (class == 2 ? GOLIOTH_OK : (code == 0 ? GOLIOTH_ERR_BAD_REQUEST : GOLIOTH_ERR_FAIL)),
        .status_class = class,
        .status_code = code,
    };

    assert(session);
    coap_context_t *coap_context = coap_session_get_context(session);
    assert(coap_context);
    struct golioth_client *client = coap_get_app_data(coap_context);
    assert(client);

    const uint8_t *data = NULL;
    size_t data_len = 0;
    coap_get_data(received, &data_len, &data);

    // Get the original/pending request info
    golioth_coap_request_msg_t *req = client->pending_req;

    if (req)
    {
        if (req->status)
        {
            *req->status = response.status;
        }

        if (req->type == GOLIOTH_COAP_REQUEST_EMPTY)
        {
            GLTH_LOGD(TAG, "%d.%02d (empty req), len %" PRIu32, class, code, (uint32_t) data_len);
        }
        else if (class != 2)
        {  // not 2.XX, i.e. not success
            GLTH_LOGW(TAG,
                      "%d.%02d (req type: %d, path: %s%s), len %" PRIu32,
                      class,
                      code,
                      req->type,
                      req->path_prefix,
                      req->path,
                      (uint32_t) data_len);

            // In case of 4.xx, the payload often has a useful error message
            GLTH_LOG_BUFFER_HEXDUMP(TAG, data, min(data_len, 256), GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);
        }
        else
        {
            GLTH_LOGD(TAG,
                      "%d.%02d (req type: %d, path: %s%s), len %" PRIu32,
                      class,
                      code,
                      req->type,
                      req->path_prefix,
                      req->path,
                      (uint32_t) data_len);
        }
    }
    else
    {
        GLTH_LOGD(TAG, "%d.%02d (unsolicited), len %" PRIu32, class, code, (uint32_t) data_len);
    }

    if (req && token_matches_request(req, received))
    {
        req->got_response = true;

        if (CONFIG_GOLIOTH_COAP_KEEPALIVE_INTERVAL_S > 0)
        {
            if (!golioth_sys_timer_reset(client->keepalive_timer))
            {
                GLTH_LOGW(TAG, "Failed to reset keepalive timer");
            }
        }

        if (golioth_sys_now_ms() > req->ageout_ms)
        {
            GLTH_LOGW(TAG, "Ignoring response from old request, type %d", req->type);
        }
        else
        {
            if (req->type == GOLIOTH_COAP_REQUEST_GET)
            {
                if (req->get.callback)
                {
                    req->get.callback(client, &response, req->path, data, data_len, req->get.arg);
                }
            }
            else if (req->type == GOLIOTH_COAP_REQUEST_GET_BLOCK)
            {
                coap_opt_iterator_t opt_iter;
                coap_opt_t *block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);

                // Note: If there's only one block, then the server will not include the BLOCK2
                // option in the response. So block_opt may be NULL here.

                uint32_t opt_block_index = block_opt ? coap_opt_block_num(block_opt) : 0;
                bool is_last = block_opt ? (COAP_OPT_BLOCK_MORE(block_opt) == 0) : true;
                size_t szx = block_opt ? COAP_OPT_BLOCK_SZX(block_opt) : 0;

                if (block_opt && (opt_block_index == 0)
                    && BLOCKSIZE_TO_SZX(req->get_block.block_size) > szx)
                {

                    GLTH_LOGW(TAG, "Received SZX from server for block index 0: %zu", szx);
                    GLTH_LOGW(TAG,
                              "Requested blocksize was %zu, updating",
                              req->get_block.block_size);
                    req->get_block.block_size = SZX_TO_BLOCKSIZE(szx);
                }

                GLTH_LOGD(TAG,
                          "Request block index = %" PRIu32 ", response block index = %" PRIu32
                          ", offset 0x%08" PRIX32,
                          (uint32_t) req->get_block.block_index,
                          (uint32_t) opt_block_index,
                          opt_block_index * szx);

                GLTH_LOG_BUFFER_HEXDUMP(TAG,
                                        data,
                                        min(32, data_len),
                                        GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

                if (req->get_block.callback)
                {
                    req->get_block.callback(client,
                                            &response,
                                            req->path,
                                            data,
                                            data_len,
                                            is_last,
                                            req->get_block.arg);
                }
            }
            else if (req->type == GOLIOTH_COAP_REQUEST_POST)
            {
                if (req->post.callback)
                {
                    req->post.callback(client, &response, req->path, req->post.arg);
                }
            }
            else if (req->type == GOLIOTH_COAP_REQUEST_POST_BLOCK)
            {
                if (req->post_block.callback)
                {
                    req->post_block.callback(client, &response, req->path, req->post_block.arg);
                }
            }
            else if (req->type == GOLIOTH_COAP_REQUEST_DELETE)
            {
                if (req->delete.callback)
                {
                    req->delete.callback(client, &response, req->path, req->delete.arg);
                }
            }
        }
    }

    notify_observers(received, client, data, data_len, &response);

    return COAP_RESPONSE_OK;
}

static int event_handler(coap_session_t *session, const coap_event_t event)
{
    if (event == COAP_EVENT_MSG_RETRANSMITTED)
    {
        GLTH_LOGW(TAG, "CoAP message retransmitted");
    }
    else
    {
        GLTH_LOGD(TAG, "event: 0x%04X", event);
    }
    return 0;
}

static void nack_handler(coap_session_t *session,
                         const coap_pdu_t *sent,
                         const coap_nack_reason_t reason,
                         const coap_mid_t id)
{
    coap_context_t *context = coap_session_get_context(session);
    struct golioth_client *client = coap_get_app_data(context);
    golioth_coap_request_msg_t *req = client->pending_req;

    switch (reason)
    {
        case COAP_NACK_TOO_MANY_RETRIES:
            GLTH_LOGE(TAG, "Received nack reason: COAP_NACK_TOO_MANY_RETRIES");
            break;
        case COAP_NACK_NOT_DELIVERABLE:
            GLTH_LOGE(TAG, "Received nack reason: COAP_NACK_NOT_DELIVERABLE");
            break;
        case COAP_NACK_TLS_FAILED:
            GLTH_LOGE(TAG, "Received nack reason: COAP_NACK_TLS_FAILED");
            break;
        default:
            GLTH_LOGE(TAG, "Received nack reason: %d", reason);
    }

    if (req)
    {
        req->got_nack = true;
    }
}

#if GOLIOTH_OVERRIDE_LIBCOAP_LOG_HANDLER
static void coap_log_handler(coap_log_t level, const char *message)
{
    if (level <= COAP_LOG_ERR)
    {
        GLTH_LOGE("libcoap", "%s", message);
    }
    else if (level <= COAP_LOG_WARN)
    {
        GLTH_LOGW("libcoap", "%s", message);
    }
    else if (level <= COAP_LOG_INFO)
    {
        GLTH_LOGI("libcoap", "%s", message);
    }
    else
    {
        GLTH_LOGD("libcoap", "%s", message);
    }
}
#endif /* GOLIOTH_OVERRIDE_LIBCOAP_LOG_HANDLER */

// DNS lookup of host_uri
static enum golioth_status get_coap_dst_address(const coap_uri_t *host_uri,
                                                coap_address_t *dst_addr)
{
    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
        .ai_family = AF_UNSPEC,
    };
    struct addrinfo *ainfo = NULL;
    const char *hostname = (const char *) host_uri->host.s;
    int error = getaddrinfo(hostname, NULL, &hints, &ainfo);
    if (error != 0)
    {
        GLTH_LOGE(TAG, "DNS lookup failed for destination ainfo %s. error: %d", hostname, error);
        return GOLIOTH_ERR_DNS_LOOKUP;
    }
    if (!ainfo)
    {
        GLTH_LOGE(TAG, "DNS lookup %s did not return any addresses", hostname);
        return GOLIOTH_ERR_DNS_LOOKUP;
    }

    coap_address_init(dst_addr);

    switch (ainfo->ai_family)
    {
        case AF_INET:
            memcpy(&dst_addr->addr.sin, ainfo->ai_addr, sizeof(dst_addr->addr.sin));
            dst_addr->addr.sin.sin_port = htons(host_uri->port);
            dst_addr->size = sizeof(dst_addr->addr.sin);
            break;
        case AF_INET6:
            memcpy(&dst_addr->addr.sin6, ainfo->ai_addr, sizeof(dst_addr->addr.sin6));
            dst_addr->addr.sin6.sin6_port = htons(host_uri->port);
            dst_addr->size = sizeof(dst_addr->addr.sin6);
            break;
        default:
            GLTH_LOGE(TAG, "DNS lookup response failed");
            freeaddrinfo(ainfo);
            return GOLIOTH_ERR_DNS_LOOKUP;
    }
    freeaddrinfo(ainfo);

    return GOLIOTH_OK;
}

static void golioth_coap_add_token(coap_pdu_t *req_pdu,
                                   golioth_coap_request_msg_t *req,
                                   coap_session_t *session)
{
    coap_session_new_token(session, &req->token_len, req->token);
    coap_add_token(req_pdu, req->token_len, req->token);
}

static void golioth_coap_add_path(coap_pdu_t *request, const char *path_prefix, const char *path)
{
    if (!path_prefix)
    {
        path_prefix = "";
    }
    assert(path);

    char fullpath[64] = {};
    snprintf(fullpath, sizeof(fullpath), "%s%s", path_prefix, path);

    size_t fullpathlen = strlen(fullpath);
    unsigned char buf[64];
    unsigned char *pbuf = buf;
    size_t buflen = sizeof(buf);
    int nsegments = coap_split_path((const uint8_t *) fullpath, fullpathlen, pbuf, &buflen);
    while (nsegments--)
    {
        if (coap_opt_length(pbuf) > 0)
        {
            coap_add_option(request,
                            COAP_OPTION_URI_PATH,
                            coap_opt_length(pbuf),
                            coap_opt_value(pbuf));
        }
        pbuf += coap_opt_size(pbuf);
    }
}

static uint32_t golioth_content_type_to_coap_type(enum golioth_content_type content_type)
{
    switch (content_type)
    {
        case GOLIOTH_CONTENT_TYPE_JSON:
            return COAP_MEDIATYPE_APPLICATION_JSON;
        case GOLIOTH_CONTENT_TYPE_CBOR:
            return COAP_MEDIATYPE_APPLICATION_CBOR;
        case GOLIOTH_CONTENT_TYPE_OCTET_STREAM:
            return COAP_MEDIATYPE_APPLICATION_OCTET_STREAM;
        default:
            return COAP_MEDIATYPE_APPLICATION_OCTET_STREAM;
    }
}

static void golioth_coap_add_content_type(coap_pdu_t *request,
                                          enum golioth_content_type content_type)
{
    unsigned char typebuf[4];
    uint32_t coap_type = golioth_content_type_to_coap_type(content_type);
    coap_add_option(request,
                    COAP_OPTION_CONTENT_TYPE,
                    coap_encode_var_safe(typebuf, sizeof(typebuf), coap_type),
                    typebuf);
}

static void golioth_coap_add_accept(coap_pdu_t *request, enum golioth_content_type content_type)
{
    unsigned char typebuf[4];
    uint32_t coap_type = golioth_content_type_to_coap_type(content_type);
    coap_add_option(request,
                    COAP_OPTION_ACCEPT,
                    coap_encode_var_safe(typebuf, sizeof(typebuf), coap_type),
                    typebuf);
}

static void golioth_coap_add_block1(coap_pdu_t *request,
                                    size_t block_index,
                                    size_t block_size,
                                    bool is_last)
{
    size_t szx = BLOCKSIZE_TO_SZX(block_size);
    assert(szx != -1);
    coap_block_t block = {
        .num = block_index,
        .m = !is_last,
        .szx = szx,
    };

    unsigned char buf[4];
    unsigned int opt_length =
        coap_encode_var_safe(buf, sizeof(buf), (block.num << 4 | block.m << 3 | block.szx));
    coap_add_option(request, COAP_OPTION_BLOCK1, opt_length, buf);
}

static void golioth_coap_add_block2(coap_pdu_t *request, size_t block_index, size_t block_size)
{
    size_t szx = BLOCKSIZE_TO_SZX(block_size);
    assert(szx != -1);
    coap_block_t block = {
        .num = block_index,
        .m = 0,
        .szx = szx,
    };

    unsigned char buf[4];
    unsigned int opt_length =
        coap_encode_var_safe(buf, sizeof(buf), (block.num << 4 | block.m << 3 | block.szx));
    coap_add_option(request, COAP_OPTION_BLOCK2, opt_length, buf);
}

static void golioth_coap_empty(golioth_coap_request_msg_t *req, coap_session_t *session)
{
    // Note: libcoap has keepalive functionality built in, but we're not using because
    // it doesn't seem to work correctly. The server responds to the keepalive message,
    // but libcoap is disconnecting the session after the response is received:
    //
    //     DTLS: session disconnected (reason 1)
    //
    // Instead, we will send an empty DELETE request
    coap_pdu_t *req_pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_DELETE, session);
    if (!req_pdu)
    {
        GLTH_LOGE(TAG, "coap_new_pdu() delete failed");
        return;
    }

    golioth_coap_add_token(req_pdu, req, session);
    coap_send(session, req_pdu);
}

static void golioth_coap_get(golioth_coap_request_msg_t *req, coap_session_t *session)
{
    coap_pdu_t *req_pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET, session);
    if (!req_pdu)
    {
        GLTH_LOGE(TAG, "coap_new_pdu() get failed");
        return;
    }

    golioth_coap_add_token(req_pdu, req, session);
    golioth_coap_add_path(req_pdu, req->path_prefix, req->path);
    golioth_coap_add_accept(req_pdu, req->get.content_type);
    coap_send(session, req_pdu);
}

static void golioth_coap_get_block(golioth_coap_request_msg_t *req,
                                   struct golioth_client *client,
                                   coap_session_t *session)
{
    coap_pdu_t *req_pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET, session);
    if (!req_pdu)
    {
        GLTH_LOGE(TAG, "coap_new_pdu() get failed");
        return;
    }

    if (req->get_block.block_index == 0)
    {
        // Save this token for further blocks
        golioth_coap_add_token(req_pdu, req, session);
        memcpy(client->block_token, req->token, req->token_len);
        client->block_token_len = req->token_len;
    }
    else
    {
        coap_add_token(req_pdu, client->block_token_len, client->block_token);

        // Copy block token into the current req_pdu token, since this is what
        // is checked in coap_response_handler to verify the response has been received.
        memcpy(req->token, client->block_token, client->block_token_len);
        req->token_len = client->block_token_len;
    }

    golioth_coap_add_path(req_pdu, req->path_prefix, req->path);
    golioth_coap_add_accept(req_pdu, req->get_block.content_type);
    golioth_coap_add_block2(req_pdu, req->get_block.block_index, req->get_block.block_size);
    coap_send(session, req_pdu);
}

static void golioth_coap_post(golioth_coap_request_msg_t *req, coap_session_t *session)
{
    coap_pdu_t *req_pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_POST, session);
    if (!req_pdu)
    {
        GLTH_LOGE(TAG, "coap_new_pdu() post failed");
        return;
    }

    golioth_coap_add_token(req_pdu, req, session);
    golioth_coap_add_path(req_pdu, req->path_prefix, req->path);
    golioth_coap_add_content_type(req_pdu, req->post.content_type);
    coap_add_data(req_pdu, req->post.payload_size, (unsigned char *) req->post.payload);
    coap_send(session, req_pdu);
}

static void golioth_coap_post_block(golioth_coap_request_msg_t *req,
                                    struct golioth_client *client,
                                    coap_session_t *session)
{
    coap_pdu_t *req_pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_POST, session);
    if (!req_pdu)
    {
        GLTH_LOGE(TAG, "coap_new_pdu() post failed");
        return;
    }

    if (req->post_block.block_index == 0)
    {
        // Save this token for further blocks
        golioth_coap_add_token(req_pdu, req, session);
        memcpy(client->block_token, req->token, req->token_len);
        client->block_token_len = req->token_len;
    }
    else
    {
        coap_add_token(req_pdu, client->block_token_len, client->block_token);

        // Copy block token into the current req_pdu token, since this is what
        // is checked in coap_response_handler to verify the response has been received.
        memcpy(req->token, client->block_token, client->block_token_len);
        req->token_len = client->block_token_len;
    }

    golioth_coap_add_path(req_pdu, req->path_prefix, req->path);
    golioth_coap_add_content_type(req_pdu, req->post_block.content_type);
    golioth_coap_add_block1(req_pdu,
                            req->post_block.block_index,
                            CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_MAX_BLOCK_SIZE,
                            req->post_block.is_last);
    coap_add_data(req_pdu, req->post_block.payload_size, (unsigned char *) req->post_block.payload);
    coap_send(session, req_pdu);
}

static void golioth_coap_delete(golioth_coap_request_msg_t *req, coap_session_t *session)
{
    coap_pdu_t *req_pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_DELETE, session);
    if (!req_pdu)
    {
        GLTH_LOGE(TAG, "coap_new_pdu() delete failed");
        return;
    }

    golioth_coap_add_token(req_pdu, req, session);
    golioth_coap_add_path(req_pdu, req->path_prefix, req->path);
    coap_send(session, req_pdu);
}

static enum golioth_status golioth_coap_observe(golioth_coap_request_msg_t *req,
                                                struct golioth_client *client,
                                                coap_session_t *session,
                                                bool eager_release)
{
    // GET with an OBSERVE option
    coap_pdu_t *req_pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET, session);
    if (!req_pdu)
    {
        GLTH_LOGE(TAG, "coap_new_pdu() get failed");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    unsigned char optbuf[4] = {};

    if (eager_release)
    {
        coap_add_token(req_pdu, req->token_len, req->token);

        coap_add_option(req_pdu,
                        COAP_OPTION_OBSERVE,
                        coap_encode_var_safe(optbuf, sizeof(optbuf), COAP_OBSERVE_CANCEL),
                        optbuf);
    }
    else
    {
        golioth_coap_add_token(req_pdu, req, session);

        coap_add_option(req_pdu,
                        COAP_OPTION_OBSERVE,
                        coap_encode_var_safe(optbuf, sizeof(optbuf), COAP_OBSERVE_ESTABLISH),
                        optbuf);
    }

    golioth_coap_add_path(req_pdu, req->path_prefix, req->path);
    golioth_coap_add_accept(req_pdu, req->observe.content_type);

    int err = coap_send(session, req_pdu);
    if (err == COAP_INVALID_MID)
    {
        /* The coap_send() function returns the CoAP message ID on success or COAP_INVALID_MID on
         * failure. */
        GLTH_LOGE(TAG, "Unable to observe path %s, cannot send CoAP PDU", req->path);
        return GOLIOTH_ERR_BAD_REQUEST;
    }
    else
    {
        return GOLIOTH_OK;
    }
}

static enum golioth_status add_observation(golioth_coap_request_msg_t *req,
                                           struct golioth_client *client,
                                           coap_session_t *session)
{
    // scan for available (not used) observation slot
    golioth_coap_observe_info_t *obs_info = NULL;
    bool found_slot = false;
    for (int i = 0; i < CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS; i++)
    {
        obs_info = &client->observations[i];
        if (!obs_info->in_use)
        {
            found_slot = true;
            break;
        }
    }

    if (!found_slot)
    {
        GLTH_LOGE(TAG, "Unable to observe path %s, no slots available", req->path);
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    int err = golioth_coap_observe(req, client, session, false);
    if (err)
    {
        return err;
    }

    obs_info->in_use = true;
    memcpy(&obs_info->req, req, sizeof(obs_info->req));

    return GOLIOTH_OK;
}


void golioth_cancel_all_observations_by_prefix(struct golioth_client *client, const char *prefix)
{
    golioth_coap_observe_info_t *obs_info = NULL;
    for (int i = 0; i < CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS; i++)
    {
        obs_info = &client->observations[i];
        if (obs_info->in_use)
        {
            if ((prefix != NULL) && (strcmp(prefix, obs_info->req.path_prefix) != 0))
            {
                continue;
            }

            obs_info->in_use = false;
            golioth_coap_client_observe_release(client,
                                                obs_info->req.path_prefix,
                                                obs_info->req.path,
                                                obs_info->req.observe.content_type,
                                                obs_info->req.token,
                                                obs_info->req.token_len,
                                                NULL);
        }
    }
}

void golioth_cancel_all_observations(struct golioth_client *client)
{
    golioth_cancel_all_observations_by_prefix(client, NULL);
}

static void reestablish_observations(struct golioth_client *client, coap_session_t *session)
{
    golioth_coap_observe_info_t *obs_info = NULL;
    for (int i = 0; i < CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS; i++)
    {
        obs_info = &client->observations[i];
        if (obs_info->in_use)
        {
            golioth_coap_observe(&obs_info->req, client, session, false);
        }
    }
}

static enum golioth_status create_context(struct golioth_client *client, coap_context_t **context)
{
    *context = coap_new_context(NULL);
    if (!*context)
    {
        GLTH_LOGE(TAG, "Failed to create CoAP context");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    // Store our client pointer in the context, since it's needed in the reponse handler
    // we register below.
    coap_set_app_data(*context, client);

    // Register handlers
    coap_register_response_handler(*context, coap_response_handler);
    coap_register_event_handler(*context, event_handler);
    coap_register_nack_handler(*context, nack_handler);

    return GOLIOTH_OK;
}

static int validate_cn_call_back(const char *cn,
                                 const uint8_t *asn1_public_cert,
                                 size_t asn1_length,
                                 coap_session_t *session,
                                 unsigned depth,
                                 int validated,
                                 void *arg)
{
    GLTH_LOGI(TAG,
              "Server Cert: Depth = %u, Len = %" PRIu32 ", Valid = %d",
              depth,
              (uint32_t) asn1_length,
              validated);
    return 1;
}

static enum golioth_status create_session(struct golioth_client *client,
                                          coap_context_t *context,
                                          coap_session_t **session)
{
    // Split URI for host
    coap_uri_t host_uri = {};
    int uri_status = coap_split_uri((const uint8_t *) CONFIG_GOLIOTH_COAP_HOST_URI,
                                    strlen(CONFIG_GOLIOTH_COAP_HOST_URI),
                                    &host_uri);
    if (uri_status < 0)
    {
        GLTH_LOGE(TAG, "CoAP host URI invalid: %s", CONFIG_GOLIOTH_COAP_HOST_URI);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }

    // Get destination address of host
    coap_address_t dst_addr = {};
    GOLIOTH_STATUS_RETURN_IF_ERROR(get_coap_dst_address(&host_uri, &dst_addr));

    GLTH_LOGI(TAG, "Start CoAP session with host: %s", CONFIG_GOLIOTH_COAP_HOST_URI);

    char client_sni[256] = {};
    memcpy(client_sni, host_uri.host.s, MIN(host_uri.host.length, sizeof(client_sni) - 1));

    enum golioth_auth_type auth_type = client->config.credentials.auth_type;

    if (auth_type == GOLIOTH_TLS_AUTH_TYPE_PSK)
    {
        struct golioth_psk_credential psk_creds = client->config.credentials.psk;

        GLTH_LOGI(TAG, "Session PSK-ID: %.*s", (int) psk_creds.psk_id_len, psk_creds.psk_id);

        coap_dtls_cpsk_t dtls_psk = {
            .version = COAP_DTLS_CPSK_SETUP_VERSION,
            .client_sni = client_sni,
            .psk_info.identity.s = (const uint8_t *) psk_creds.psk_id,
            .psk_info.identity.length = psk_creds.psk_id_len,
            .psk_info.key.s = (const uint8_t *) psk_creds.psk,
            .psk_info.key.length = psk_creds.psk_len,
        };
        *session =
            coap_new_client_session_psk2(context, NULL, &dst_addr, COAP_PROTO_DTLS, &dtls_psk);
    }
    else if (auth_type == GOLIOTH_TLS_AUTH_TYPE_PKI)
    {
        struct golioth_pki_credential pki_creds = client->config.credentials.pki;

        coap_dtls_pki_t dtls_pki = {
            .version = COAP_DTLS_PKI_SETUP_VERSION,
            .verify_peer_cert = 1,
            .check_common_ca = 1,
            .allow_self_signed = 0,
            .allow_expired_certs = 0,
            .cert_chain_validation = 1,
            .cert_chain_verify_depth = 3,
            .check_cert_revocation = 1,
            .allow_no_crl = 1,
            .allow_expired_crl = 0,
            .allow_bad_md_hash = 0,
            .allow_short_rsa_length = 1,
            .is_rpk_not_cert = 0,
            .validate_cn_call_back = validate_cn_call_back,
            .client_sni = client_sni,
            .pki_key =
                {
                    .key_type = COAP_PKI_KEY_PEM_BUF,
                    .key.pem_buf =
                        {
                            .ca_cert = pki_creds.ca_cert,
                            .ca_cert_len = pki_creds.ca_cert_len,
                            .public_cert = pki_creds.public_cert,
                            .public_cert_len = pki_creds.public_cert_len,
                            .private_key = pki_creds.private_key,
                            .private_key_len = pki_creds.private_key_len,
                        },
                },
        };
        *session =
            coap_new_client_session_pki(context, NULL, &dst_addr, COAP_PROTO_DTLS, &dtls_pki);
    }
    else
    {
        GLTH_LOGE(TAG, "Invalid TLS auth type: %d", auth_type);
        return GOLIOTH_ERR_NOT_ALLOWED;
    }

    if (!*session)
    {
        GLTH_LOGE(TAG, "coap_new_client_session() failed");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    return GOLIOTH_OK;
}

static enum golioth_status coap_io_loop_once(struct golioth_client *client,
                                             coap_context_t *context,
                                             coap_session_t *session)
{
    golioth_coap_request_msg_t request_msg = {};
    int mbox_fd = golioth_sys_sem_get_fd(client->request_queue->fill_count_sem);

    if (mbox_fd >= 0)
    {
        fd_set readfds;

        FD_ZERO(&readfds);
        FD_SET(mbox_fd, &readfds);

        coap_io_process_with_fds(context, COAP_IO_WAIT, mbox_fd + 1, &readfds, NULL, NULL);

        if (!FD_ISSET(mbox_fd, &readfds))
        {
            return GOLIOTH_OK;
        }

        bool got_request_msg = golioth_mbox_recv(client->request_queue, &request_msg, 0);
        if (!got_request_msg)
        {
            GLTH_LOGE(TAG, "Failed to get request_message from mbox");
            return GOLIOTH_ERR_IO;
        }
    }
    else
    {
        // Wait for request message, with timeout
        bool got_request_msg = golioth_mbox_recv(client->request_queue,
                                                 &request_msg,
                                                 CONFIG_GOLIOTH_COAP_REQUEST_QUEUE_TIMEOUT_MS);
        if (!got_request_msg)
        {
            // No requests, so process other pending IO (e.g. observations)
            GLTH_LOGV(TAG, "Idle io process start");
            coap_io_process(context, COAP_IO_NO_WAIT);
            GLTH_LOGV(TAG, "Idle io process end");
            return GOLIOTH_OK;
        }
    }

    // Make sure the request isn't too old
    if (golioth_sys_now_ms() > request_msg.ageout_ms)
    {
        GLTH_LOGW(TAG,
                  "Ignoring request that has aged out, type %d, path %s",
                  request_msg.type,
                  (request_msg.path ? request_msg.path : "N/A"));

        if (request_msg.type == GOLIOTH_COAP_REQUEST_POST && request_msg.post.payload_size > 0)
        {
            golioth_sys_free(request_msg.post.payload);
        }

        if (request_msg.type == GOLIOTH_COAP_REQUEST_POST_BLOCK
            && request_msg.post_block.payload_size > 0)
        {
            golioth_sys_free(request_msg.post_block.payload);
        }

        if (request_msg.request_complete_event)
        {
            assert(request_msg.request_complete_ack_sem);
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
        }
        return GOLIOTH_OK;
    }

    int err;
    // Handle message and send request to server
    bool request_is_valid = true;
    switch (request_msg.type)
    {
        case GOLIOTH_COAP_REQUEST_EMPTY:
            GLTH_LOGD(TAG, "Handle EMPTY");
            golioth_coap_empty(&request_msg, session);
            break;
        case GOLIOTH_COAP_REQUEST_GET:
            GLTH_LOGD(TAG, "Handle GET %s", request_msg.path);
            golioth_coap_get(&request_msg, session);
            break;
        case GOLIOTH_COAP_REQUEST_GET_BLOCK:
            GLTH_LOGD(TAG, "Handle GET_BLOCK %s", request_msg.path);
            golioth_coap_get_block(&request_msg, client, session);
            break;
        case GOLIOTH_COAP_REQUEST_POST:
            GLTH_LOGD(TAG, "Handle POST %s", request_msg.path);
            golioth_coap_post(&request_msg, session);
            assert(request_msg.post.payload);
            golioth_sys_free(request_msg.post.payload);
            break;
        case GOLIOTH_COAP_REQUEST_POST_BLOCK:
            GLTH_LOGD(TAG, "Handle POST_BLOCK %s", request_msg.path);
            golioth_coap_post_block(&request_msg, client, session);
            assert(request_msg.post_block.payload);
            golioth_sys_free(request_msg.post_block.payload);
            break;
        case GOLIOTH_COAP_REQUEST_DELETE:
            GLTH_LOGD(TAG, "Handle DELETE %s", request_msg.path);
            golioth_coap_delete(&request_msg, session);
            break;
        case GOLIOTH_COAP_REQUEST_OBSERVE:
            GLTH_LOGD(TAG, "Handle OBSERVE %s", request_msg.path);
            err = add_observation(&request_msg, client, session);
            if (err)
            {
                GLTH_LOGE(TAG, "Error adding observation: %d", err);
                request_is_valid = false;
            }
            break;
        case GOLIOTH_COAP_REQUEST_OBSERVE_RELEASE:
            GLTH_LOGD(TAG, "Handle OBSERVE CANCEL %s", request_msg.path);
            err = golioth_coap_observe(&request_msg, client, session, true);
            if (err == COAP_INVALID_MID)
            {
                GLTH_LOGE(TAG,
                          "Unable to release observed path %s, cannot send CoAP PDU",
                          request_msg.path);
                request_is_valid = false;
            }
            break;
        default:
            GLTH_LOGW(TAG, "Unknown request_msg type: %u", request_msg.type);
            request_is_valid = false;
            break;
    }

    if (!request_is_valid)
    {
        return GOLIOTH_OK;
    }

    // If we get here, then a confirmable request has been sent to the server,
    // and we should wait for a response.
    client->pending_req = &request_msg;
    request_msg.got_response = false;
    int32_t time_spent_waiting_ms = 0;
    int32_t timeout_ms = CONFIG_GOLIOTH_COAP_RESPONSE_TIMEOUT_S * 1000;

    if (request_msg.ageout_ms != GOLIOTH_SYS_WAIT_FOREVER)
    {
        int32_t time_till_ageout_ms = (int32_t) (request_msg.ageout_ms - golioth_sys_now_ms());
        timeout_ms = min(timeout_ms, time_till_ageout_ms);
    }

    bool io_error = false;
    while (time_spent_waiting_ms < timeout_ms)
    {
        int32_t remaining_ms = timeout_ms - time_spent_waiting_ms;
        int32_t wait_ms = min(1000, remaining_ms);
        int32_t num_ms = coap_io_process(context, wait_ms);
        if (num_ms < 0)
        {
            io_error = true;
            break;
        }
        else
        {
            time_spent_waiting_ms += num_ms;
            if (request_msg.got_response)
            {
                GLTH_LOGD(TAG, "Received response in %" PRId32 " ms", time_spent_waiting_ms);
                break;
            }
            else if (request_msg.got_nack)
            {
                GLTH_LOGE(TAG, "Got NACKed request");
                break;
            }
            else
            {
                // During normal operation, there will be other kinds of IO to process,
                // in which case we will get here.
                // Since we haven't received the response yet, just keep waiting.
            }
        }
    }
    client->pending_req = NULL;

    if (request_msg.request_complete_event)
    {
        assert(request_msg.request_complete_ack_sem);

        if (request_msg.got_response)
        {
            golioth_event_group_set_bits(request_msg.request_complete_event,
                                         RESPONSE_RECEIVED_EVENT_BIT);
        }
        else
        {
            golioth_event_group_set_bits(request_msg.request_complete_event,
                                         RESPONSE_TIMEOUT_EVENT_BIT);
        }

        // Wait for user thread to receive the event.
        golioth_sys_sem_take(request_msg.request_complete_ack_sem, GOLIOTH_SYS_WAIT_FOREVER);

        // Now it's safe to delete the event and semaphore.
        golioth_event_group_destroy(request_msg.request_complete_event);
        golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
    }

    if (io_error)
    {
        GLTH_LOGE(TAG, "Error in coap_io_process");
        return GOLIOTH_ERR_IO;
    }

    if (request_msg.got_nack)
    {
        return GOLIOTH_ERR_NACK;
    }

    if (time_spent_waiting_ms >= timeout_ms)
    {
        GLTH_LOGE(TAG, "Timeout: never got a response from the server");

        if (coap_session_get_state(session) == COAP_SESSION_STATE_HANDSHAKE)
        {
            // TODO - customize error message based on PSK vs cert usage
            GLTH_LOGE(TAG, "DTLS handshake failed. Maybe your PSK-ID or PSK is incorrect?");
        }

        // Call user's callback with GOLIOTH_ERR_TIMEOUT
        // TODO - simplify, put callback directly in request which removes if/else branches
        struct golioth_response response = {};
        response.status = GOLIOTH_ERR_TIMEOUT;
        if (request_msg.type == GOLIOTH_COAP_REQUEST_GET && request_msg.get.callback)
        {
            request_msg.get
                .callback(client, &response, request_msg.path, NULL, 0, request_msg.get.arg);
        }
        else if (request_msg.type == GOLIOTH_COAP_REQUEST_GET_BLOCK
                 && request_msg.get_block.callback)
        {
            request_msg.get_block.callback(client,
                                           &response,
                                           request_msg.path,
                                           NULL,
                                           0,
                                           false,
                                           request_msg.get_block.arg);
        }
        else if (request_msg.type == GOLIOTH_COAP_REQUEST_POST && request_msg.post.callback)
        {
            request_msg.post.callback(client, &response, request_msg.path, request_msg.post.arg);
        }
        else if (request_msg.type == GOLIOTH_COAP_REQUEST_POST_BLOCK
                 && request_msg.post_block.callback)
        {
            request_msg.post_block.callback(client,
                                            &response,
                                            request_msg.path,
                                            request_msg.post_block.arg);
        }
        else if (request_msg.type == GOLIOTH_COAP_REQUEST_DELETE && request_msg.delete.callback)
        {
            request_msg.delete.callback(client,
                                        &response,
                                        request_msg.path,
                                        request_msg.delete.arg);
        }

        golioth_sys_client_disconnected(client);
        if (client->event_callback && client->session_connected)
        {
            client->event_callback(client,
                                   GOLIOTH_CLIENT_EVENT_DISCONNECTED,
                                   client->event_callback_arg);
        }
        client->session_connected = false;
        return GOLIOTH_ERR_TIMEOUT;
    }

    if (!client->session_connected)
    {
        // Transitioned from not connected to connected
        GLTH_LOGI(TAG, "Golioth CoAP client connected");
        golioth_sys_client_connected(client);
        if (client->event_callback)
        {
            client->event_callback(client,
                                   GOLIOTH_CLIENT_EVENT_CONNECTED,
                                   client->event_callback_arg);
        }
    }

    client->session_connected = true;
    return GOLIOTH_OK;
}

static void on_keepalive(golioth_sys_timer_t timer, void *arg)
{
    struct golioth_client *client = arg;
    if (client->is_running && golioth_client_num_items_in_request_queue(client) == 0
        && !client->pending_req)
    {
        golioth_coap_client_empty(client, false, GOLIOTH_SYS_WAIT_FOREVER);
    }
}

bool golioth_client_is_running(struct golioth_client *client)
{
    if (!client)
    {
        return false;
    }
    return client->is_running;
}

// Note: libcoap is not thread safe, so all rx/tx I/O for the session must be
// done in this thread.
static void golioth_coap_client_thread(void *arg)
{
    struct golioth_client *client = arg;
    assert(client);

    while (1)
    {
        coap_context_t *coap_context = NULL;
        coap_session_t *coap_session = NULL;

        client->end_session = false;
        client->session_connected = false;

        client->is_running = false;
        GLTH_LOGD(TAG, "Waiting for the \"run\" signal");
        golioth_sys_sem_take(client->run_sem, GOLIOTH_SYS_WAIT_FOREVER);
        golioth_sys_sem_give(client->run_sem);
        GLTH_LOGD(TAG, "Received \"run\" signal");
        client->is_running = true;

        if (create_context(client, &coap_context) != GOLIOTH_OK)
        {
            goto cleanup;
        }

        if (create_session(client, coap_context, &coap_session) != GOLIOTH_OK)
        {
            goto cleanup;
        }

        // Seed the session token generator
        uint8_t seed_token[8];
        size_t seed_token_len;
        uint32_t randint = golioth_sys_rand();
        seed_token_len = coap_encode_var_safe8(seed_token, sizeof(seed_token), randint);
        coap_session_init_token(coap_session, seed_token_len, seed_token);

        // Enqueue an asynchronous EMPTY request immediately.
        //
        // This is done so we can determine quickly whether we are connected
        // to the cloud or not (libcoap does not tell us when it's connected
        // for some reason, so this is a workaround for that).
        if (golioth_client_num_items_in_request_queue(client) == 0)
        {
            golioth_coap_client_empty(client, false, GOLIOTH_SYS_WAIT_FOREVER);
        }

        // If we are re-connecting and had prior observations, set
        // them up again now (tokens will be updated).
        reestablish_observations(client, coap_session);

        GLTH_LOGI(TAG, "Entering CoAP I/O loop");
        int iteration = 0;
        while (!client->end_session)
        {
            // Check if we should still run (non-blocking)
            if (!golioth_sys_sem_take(client->run_sem, 0))
            {
                GLTH_LOGI(TAG, "Stopping");
                break;
            }
            golioth_sys_sem_give(client->run_sem);

            if (coap_io_loop_once(client, coap_context, coap_session) != GOLIOTH_OK)
            {
                client->end_session = true;
            }
            iteration++;
        }

    cleanup:
        GLTH_LOGI(TAG, "Ending session");

        golioth_sys_client_disconnected(client);
        if (client->event_callback && client->session_connected)
        {
            client->event_callback(client,
                                   GOLIOTH_CLIENT_EVENT_DISCONNECTED,
                                   client->event_callback_arg);
        }
        client->session_connected = false;

        if (coap_session)
        {
            coap_session_release(coap_session);
        }
        if (coap_context)
        {
            coap_free_context(coap_context);
        }

        // Small delay before starting a new session
        golioth_sys_msleep(1000);
    }
}

struct golioth_client *golioth_client_create(const struct golioth_client_config *config)
{
    if (!_initialized)
    {
        // Initialize libcoap prior to any coap_* function calls.
        coap_startup();

#if GOLIOTH_OVERRIDE_LIBCOAP_LOG_HANDLER
        // Connect logs from libcoap to the ESP logger
        coap_set_log_handler(coap_log_handler);
        coap_set_log_level(COAP_LOG_INFO);
#endif /* GOLIOTH_OVERRIDE_LIBCOAP_LOG_HANDLER */

        // Seed the random number generator. Used for token generation.
        time_t t;
        golioth_sys_srand(time(&t));

        _initialized = true;
    }

    struct golioth_client *new_client = golioth_sys_malloc(sizeof(struct golioth_client));
    if (!new_client)
    {
        GLTH_LOGE(TAG, "Failed to allocate memory for client");
        goto error;
    }
    memset(new_client, 0, sizeof(struct golioth_client));

    new_client->config = *config;

    new_client->run_sem = golioth_sys_sem_create(1, 0);
    if (!new_client->run_sem)
    {
        GLTH_LOGE(TAG, "Failed to create run semaphore");
        goto error;
    }
    golioth_sys_sem_give(new_client->run_sem);

    new_client->request_queue = golioth_mbox_create(CONFIG_GOLIOTH_COAP_REQUEST_QUEUE_MAX_ITEMS,
                                                    sizeof(golioth_coap_request_msg_t));
    if (!new_client->request_queue)
    {
        GLTH_LOGE(TAG, "Failed to create request queue");
        goto error;
    }

    struct golioth_thread_config thread_cfg = {
        .name = "coap_client",
        .fn = golioth_coap_client_thread,
        .user_arg = new_client,
        .stack_size = CONFIG_GOLIOTH_COAP_THREAD_STACK_SIZE,
        .prio = CONFIG_GOLIOTH_COAP_THREAD_PRIORITY,
    };

    new_client->coap_thread_handle = golioth_sys_thread_create(&thread_cfg);
    if (!new_client->coap_thread_handle)
    {
        GLTH_LOGE(TAG, "Failed to create client thread");
        goto error;
    }

    struct golioth_timer_config keepalive_timer_cfg = {
        .name = "keepalive",
        .expiration_ms = max(1000, 1000 * CONFIG_GOLIOTH_COAP_KEEPALIVE_INTERVAL_S),
        .fn = on_keepalive,
        .user_arg = new_client};

    new_client->keepalive_timer = golioth_sys_timer_create(&keepalive_timer_cfg);
    if (!new_client->keepalive_timer)
    {
        GLTH_LOGE(TAG, "Failed to create keepalive timer");
        goto error;
    }

    if (CONFIG_GOLIOTH_COAP_KEEPALIVE_INTERVAL_S > 0)
    {
        if (!golioth_sys_timer_start(new_client->keepalive_timer))
        {
            GLTH_LOGE(TAG, "Failed to start keepalive timer");
            goto error;
        }
    }

    new_client->is_running = true;

    golioth_debug_set_client(new_client);

    return new_client;

error:
    golioth_client_destroy(new_client);
    return NULL;
}

static void purge_request_mbox(golioth_mbox_t request_mbox)
{
    golioth_coap_request_msg_t request_msg = {};
    size_t num_messages = golioth_mbox_num_messages(request_mbox);

    for (size_t i = 0; i < num_messages; i++)
    {
        bool ok = golioth_mbox_recv(request_mbox, &request_msg, 0);

        assert(ok);
        (void) ok;

        if (request_msg.type == GOLIOTH_COAP_REQUEST_POST)
        {
            // free dynamically allocated user payload copy
            golioth_sys_free(request_msg.post.payload);
        }
        else if (request_msg.type == GOLIOTH_COAP_REQUEST_POST_BLOCK)
        {
            // free dynamically allocated user payload copy
            golioth_sys_free(request_msg.post_block.payload);
        }
    }
}

void golioth_client_destroy(struct golioth_client *client)
{
    if (!client)
    {
        return;
    }
    if (client->is_running)
    {
        golioth_client_stop(client);
    }
    if (client->keepalive_timer)
    {
        golioth_sys_timer_destroy(client->keepalive_timer);
    }
    if (client->coap_thread_handle)
    {
        golioth_sys_thread_destroy(client->coap_thread_handle);
    }
    if (client->request_queue)
    {
        purge_request_mbox(client->request_queue);
        golioth_mbox_destroy(client->request_queue);
    }
    if (client->run_sem)
    {
        golioth_sys_sem_destroy(client->run_sem);
    }
    golioth_sys_free(client);
}

enum golioth_status golioth_client_start(struct golioth_client *client)
{
    if (!client)
    {
        return GOLIOTH_ERR_NULL;
    }
    golioth_sys_sem_give(client->run_sem);
    return GOLIOTH_OK;
}

enum golioth_status golioth_client_stop(struct golioth_client *client)
{
    if (!client)
    {
        return GOLIOTH_ERR_NULL;
    }

    GLTH_LOGI(TAG, "Attempting to stop client");
    golioth_sys_sem_take(client->run_sem, GOLIOTH_SYS_WAIT_FOREVER);

    // Wait for client to be fully stopped
    while (golioth_client_is_running(client))
    {
        golioth_sys_msleep(100);
    }

    return GOLIOTH_OK;
}

void golioth_client_set_packet_loss_percent(uint8_t percent)
{
    if (percent > 100)
    {
        GLTH_LOGE(TAG, "Invalid percent %u, must be 0 to 100", percent);
        return;
    }
    static char buf[16] = {};
    snprintf(buf, sizeof(buf), "%u%%", percent);
    GLTH_LOGI(TAG, "Setting packet loss to %s", buf);
    coap_debug_set_packet_loss(buf);
}
