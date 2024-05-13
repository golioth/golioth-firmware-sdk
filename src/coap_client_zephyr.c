/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include "coap_client.h"
#include "golioth_util.h"
#include "mbox.h"

#include "coap_client_zephyr.h"
#include "pathv.h"
#include "zephyr_coap_req.h"
#include "zephyr_coap_utils.h"

#include <zephyr/net/socket.h>
#include <zephyr/posix/sys/eventfd.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/net/tls_credentials.h>

#include <mbedtls/ssl_ciphersuites.h>
#include <golioth_ciphersuites.h>
#include "golioth_openthread.h"

LOG_MODULE_REGISTER(golioth_coap_client_zephyr);

#define GOLIOTH_MAX_IDENTITY_LEN 32
#define GOLIOTH_EMPTY_PACKET_LEN (16 + GOLIOTH_MAX_IDENTITY_LEN)

#define BLOCKSIZE_TO_SZX(blockSize) \
    ((blockSize == 16)         ? 0  \
         : (blockSize == 32)   ? 1  \
         : (blockSize == 64)   ? 2  \
         : (blockSize == 128)  ? 3  \
         : (blockSize == 256)  ? 4  \
         : (blockSize == 512)  ? 5  \
         : (blockSize == 1024) ? 6  \
                               : -1)

enum
{
    FLAG_STOP_CLIENT,
};

#define PING_INTERVAL (CONFIG_GOLIOTH_COAP_CLIENT_PING_INTERVAL_SEC * 1000)
#define RECV_TIMEOUT (CONFIG_GOLIOTH_COAP_CLIENT_RX_TIMEOUT_SEC * 1000)

enum pollfd_type
{
    POLLFD_EVENT,
    POLLFD_SOCKET,
    POLLFD_MBOX,
    NUM_POLLFDS,
};

static atomic_t flags;

static uint8_t rx_buffer[CONFIG_GOLIOTH_COAP_CLIENT_RX_BUF_SIZE];

static struct zsock_pollfd fds[NUM_POLLFDS];

static sec_tag_t sec_tag_list[] = {
    CONFIG_GOLIOTH_COAP_CLIENT_CREDENTIALS_TAG,
};

/* Use mbedTLS macros which are IANA ciphersuite names prepended with MBEDTLS_ */
#define GOLIOTH_CIPHERSUITE_ENTRY(x) _CONCAT(MBEDTLS_, x)

static const int golioth_ciphersuites[] = {
    FOR_EACH_NONEMPTY_TERM(GOLIOTH_CIPHERSUITE_ENTRY, (, ), GOLIOTH_CIPHERSUITES)};

static bool _initialized;

/* Golioth instance */
struct golioth_client _golioth_client;

static inline void client_notify_timeout(void)
{
    eventfd_write(fds[POLLFD_EVENT].fd, 1);
}

static void golioth_client_wakeup(struct golioth_client *client)
{
    eventfd_write(fds[POLLFD_EVENT].fd, 1);
}

static void eventfd_timeout_handle(struct k_work *work)
{
    client_notify_timeout();
}

static K_WORK_DELAYABLE_DEFINE(eventfd_timeout, eventfd_timeout_handle);

static enum coap_content_format golioth_content_type_to_coap_format(
    enum golioth_content_type content_type)
{
    switch (content_type)
    {
        case GOLIOTH_CONTENT_TYPE_JSON:
            return COAP_CONTENT_FORMAT_APP_JSON;
        case GOLIOTH_CONTENT_TYPE_CBOR:
            return COAP_CONTENT_FORMAT_APP_CBOR;
        case GOLIOTH_CONTENT_TYPE_OCTET_STREAM:
            return COAP_CONTENT_FORMAT_APP_OCTET_STREAM;
        default:
            return COAP_CONTENT_FORMAT_APP_OCTET_STREAM;
    }
}

static int golioth_coap_req_append_block1_option(golioth_coap_request_msg_t *req_msg,
                                                 struct golioth_coap_req *req)
{
    unsigned int val = 0;
    if (req->request_wo_block1.data)
    {
        /*
         * Block1 was already appended once, so just copy state before
         * it was done.
         */
        req->request = req->request_wo_block1;
    }
    else
    {
        /*
         * Block1 is about to be appended for the first time, so
         * remember coap_packet state before adding this option.
         */
        req->request_wo_block1 = req->request;
    }
    val |= BLOCKSIZE_TO_SZX(CONFIG_GOLIOTH_BLOCKWISE_UPLOAD_BLOCK_SIZE) & 0x07;
    val |= req_msg->post_block.is_last ? 0x00 : 0x08;
    val |= req_msg->post_block.block_index << 4;
    return coap_append_option_int(&req->request, COAP_OPTION_BLOCK1, val);
}

static int golioth_coap_req_append_block2_option(struct golioth_coap_req *req)
{
    if (req->request_wo_block2.data)
    {
        /*
         * Block2 was already appended once, so just copy state before
         * it was done.
         */
        req->request = req->request_wo_block2;
    }
    else
    {
        /*
         * Block2 is about to be appended for the first time, so
         * remember coap_packet state before adding this option.
         */
        req->request_wo_block2 = req->request;
    }

    return coap_append_block2_option(&req->request, &req->block_ctx);
}

static int golioth_send(struct golioth_client *client, uint8_t *data, size_t len, int flags)
{
    ssize_t sent;

    if (client->sock < 0)
    {
        return -ENOTCONN;
    }

    sent = zsock_send(client->sock, data, len, flags);
    if (sent < 0)
    {
        return -errno;
    }
    else if (sent < len)
    {
        return -EIO;
    }

    return 0;
}

static int golioth_send_coap_empty(struct golioth_client *client)
{
    struct coap_packet packet;
    uint8_t buffer[GOLIOTH_EMPTY_PACKET_LEN];
    int err;

    err = coap_packet_init(&packet,
                           buffer,
                           sizeof(buffer),
                           COAP_VERSION_1,
                           COAP_TYPE_NON_CON,
                           0,
                           NULL,
                           COAP_CODE_EMPTY,
                           coap_next_id());
    if (err)
    {
        return err;
    }

    return golioth_send(client, packet.data, packet.offset, 0);
}

static enum golioth_status golioth_err_to_status(int err)
{
    switch (err)
    {
        case 0:
            return GOLIOTH_OK;
        case -ENOMEM:
            return GOLIOTH_ERR_MEM_ALLOC;
        case -EIO:
            return GOLIOTH_ERR_IO;
        case -ETIMEDOUT:
            return GOLIOTH_ERR_TIMEOUT;
        case -EINVAL:
        case -EPERM:
            return GOLIOTH_ERR_NOT_ALLOWED;
        case -ENODATA:
            return GOLIOTH_ERR_NO_MORE_DATA;
        case -ENOSYS:
            return GOLIOTH_ERR_NOT_IMPLEMENTED;
        case -EFAULT:
            return GOLIOTH_ERR_BAD_REQUEST;
    }

    return GOLIOTH_ERR_FAIL;
}

static int golioth_coap_cb(struct golioth_req_rsp *rsp)
{
    golioth_coap_request_msg_t *req = rsp->user_data;
    struct golioth_client *client = req->client;
    struct golioth_response response = {
        .status = GOLIOTH_OK,
    };
    int err = 0;

    if (rsp->err)
    {
        if (req->request_complete_event)
        {
            *req->status = golioth_err_to_status(rsp->err);

            golioth_event_group_set_bits(req->request_complete_event, RESPONSE_RECEIVED_EVENT_BIT);

            // Wait for user thread to receive the event.
            golioth_sys_sem_take(req->request_complete_ack_sem, GOLIOTH_SYS_WAIT_FOREVER);

            // Now it's safe to delete the event and semaphore.
            golioth_event_group_destroy(req->request_complete_event);
            golioth_sys_sem_destroy(req->request_complete_ack_sem);
        }

        err = rsp->err;
        if (req->type == GOLIOTH_COAP_REQUEST_OBSERVE)
        {
            // don't free observations so we can reestablish later
            return err;
        }
        goto free_req;
    }

    switch (req->type)
    {
        case GOLIOTH_COAP_REQUEST_EMPTY:
        case GOLIOTH_COAP_REQUEST_OBSERVE_RELEASE:
            goto free_req;
        case GOLIOTH_COAP_REQUEST_GET:
            if (req->get.callback)
            {
                req->get.callback(client, &response, req->path, rsp->data, rsp->len, req->get.arg);
            }
            break;
        case GOLIOTH_COAP_REQUEST_GET_BLOCK:
            if (req->get_block.callback)
            {
                req->get_block.callback(client,
                                        &response,
                                        req->path,
                                        rsp->data,
                                        rsp->len,
                                        rsp->is_last,
                                        req->get_block.arg);
            }
            break;
        case GOLIOTH_COAP_REQUEST_POST:
            if (req->post.callback)
            {
                req->post.callback(client, &response, req->path, req->post.arg);
            }
            break;
        case GOLIOTH_COAP_REQUEST_POST_BLOCK:
            if (req->post_block.callback)
            {
                req->post_block.callback(client, &response, req->path, req->post_block.arg);
            }
            break;
        case GOLIOTH_COAP_REQUEST_DELETE:
            if (req->delete.callback)
            {
                req->delete.callback(client, &response, req->path, req->delete.arg);
            }
            break;
        case GOLIOTH_COAP_REQUEST_OBSERVE:
            if (req->observe.callback)
            {
                req->observe
                    .callback(client, &response, req->path, rsp->data, rsp->len, req->observe.arg);
            }
            /* There is no synchronous version of observe request */
            return 0;
    }

    if (req->request_complete_event)
    {
        golioth_event_group_set_bits(req->request_complete_event, RESPONSE_RECEIVED_EVENT_BIT);

        // Wait for user thread to receive the event.
        golioth_sys_sem_take(req->request_complete_ack_sem, GOLIOTH_SYS_WAIT_FOREVER);

        // Now it's safe to delete the event and semaphore.
        golioth_event_group_destroy(req->request_complete_event);
        golioth_sys_sem_destroy(req->request_complete_ack_sem);
    }

free_req:
    free(req);

    return err;
}

static int golioth_coap_get_block(golioth_coap_request_msg_t *req)
{
    const uint8_t **pathv = PATHV(req->path_prefix, req->path);
    size_t path_len = coap_pathv_estimate_alloc_len(pathv);
    bool first = req->get_block.block_index == 0;
    struct golioth_coap_req *coap_req;
    struct golioth_client *client = req->client;
    int err;

    err = golioth_coap_req_new(&coap_req,
                               client,
                               COAP_METHOD_GET,
                               COAP_TYPE_CON,
                               GOLIOTH_COAP_MAX_NON_PAYLOAD_LEN + path_len,
                               golioth_coap_cb,
                               req);
    if (err)
    {
        return err;
    }

    if (first)
    {
        /* Save token for subsequent block requests */
        client->block_token_len = coap_header_get_token(&coap_req->request, client->block_token);
    }
    else
    {
        /* Override tokeń with the one geenrated in first block request */
        memcpy(&coap_req->request.data[4], client->block_token, client->block_token_len);
    }

    err = coap_packet_append_uri_path_from_pathv(&coap_req->request, pathv);
    if (err)
    {
        LOG_ERR("Unable add uri path to packet");
        goto free_req;
    }

    coap_req->block_ctx.current = (req->get_block.block_index * req->get_block.block_size);

    err = golioth_coap_req_append_block2_option(coap_req);
    if (err)
    {
        LOG_ERR("Unable to append block2: %d", err);
        goto free_req;
    }

    err = golioth_coap_req_schedule(coap_req);
    if (err)
    {
        LOG_ERR("Failed to schedule CoAP GET BLOCK: %d", err);
        goto free_req;
    }

    return 0;

free_req:
    golioth_coap_req_free(coap_req);

    return err;
}

static int golioth_coap_post_block(golioth_coap_request_msg_t *req)
{
    const uint8_t **pathv = PATHV(req->path_prefix, req->path);
    size_t path_len = coap_pathv_estimate_alloc_len(pathv);
    bool first = req->post_block.block_index == 0;
    struct golioth_coap_req *coap_req;
    struct golioth_client *client = req->client;
    int err;
    size_t buffer_len = GOLIOTH_COAP_MAX_NON_PAYLOAD_LEN + path_len + req->post_block.payload_size;

    err = golioth_coap_req_new(&coap_req,
                               client,
                               COAP_METHOD_POST,
                               COAP_TYPE_CON,
                               buffer_len,
                               golioth_coap_cb,
                               req);
    if (err)
    {
        return err;
    }

    if (first)
    {
        /* Save token for subsequent block requests */
        client->block_token_len = coap_header_get_token(&coap_req->request, client->block_token);
    }
    else
    {
        /* Override token with the one geenrated in first block request */
        memcpy(&coap_req->request.data[4], client->block_token, client->block_token_len);
    }

    err = coap_packet_append_uri_path_from_pathv(&coap_req->request, pathv);
    if (err)
    {
        LOG_ERR("Unable add uri path to packet");
        goto free_req;
    }

    coap_req->block_ctx.current = (req->post_block.block_index * req->post_block.payload_size);

    err = golioth_coap_req_append_block1_option(req, coap_req);
    if (err)
    {
        LOG_ERR("Unable to append block1: %d", err);
        goto free_req;
    }

    err = coap_append_option_int(&coap_req->request,
                                 COAP_OPTION_CONTENT_FORMAT,
                                 golioth_content_type_to_coap_format(req->post_block.content_type));
    if (err)
    {
        LOG_ERR("Unable to add content type to packet, err: %d", err);
        goto free_req;
    }

    err = coap_packet_append_payload_marker(&coap_req->request);
    if (err)
    {
        LOG_ERR("Unable to add payload marker to packet");
        goto free_req;
    }

    err = coap_packet_append_payload(&coap_req->request,
                                     req->post_block.payload,
                                     req->post_block.payload_size);
    if (err)
    {
        LOG_ERR("Unable to add payload to packet");
        goto free_req;
    }

    err = golioth_coap_req_schedule(coap_req);
    if (err)
    {
        LOG_ERR("Failed to schedule CoAP POST BLOCK: %d", err);
        goto free_req;
    }

    return 0;

free_req:
    golioth_coap_req_free(coap_req);

    return err;
}

static int golioth_coap_observe(golioth_coap_request_msg_t *req, struct golioth_client *client)
{
    int err = golioth_coap_req_cb(req->client,
                                  COAP_METHOD_GET,
                                  PATHV(req->path_prefix, req->path),
                                  golioth_content_type_to_coap_format(req->observe.content_type),
                                  NULL,
                                  0,
                                  golioth_coap_cb,
                                  req,
                                  GOLIOTH_COAP_REQ_OBSERVE);
    if (err)
    {
        LOG_ERR("Failed to schedule CoAP OBSERVE: %d", err);
        return err;
    }

    return err;
}

static int add_observation(golioth_coap_request_msg_t *req, struct golioth_client *client)
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

    /* Store request message in Golioth client */
    memcpy(&obs_info->req, req, sizeof(obs_info->req));

    /* Use observation slot in client as the request message */
    int err = golioth_coap_observe(&obs_info->req, client);

    if (err)
    {
        return err;
    }

    /* Successfully established observation */
    obs_info->in_use = true;

    return 0;
}

void golioth_cancel_all_observations_by_prefix(struct golioth_client *client, const char *prefix)
{
    for (int i = 0; i < CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS; i++)
    {
        golioth_coap_observe_info_t *obs_info = &client->observations[i];
        if (obs_info->in_use)
        {
            if ((prefix != NULL) && (strcmp(prefix, obs_info->req.path_prefix) != 0))
            {
                continue;
            }

            int err = golioth_coap_req_find_and_cancel_observation(client, &obs_info->req);
            if (err)
            {
                LOG_WRN("Error sending eager release for observation: %d", err);
            }
            obs_info->in_use = false;
        }
    }
}

void golioth_cancel_all_observations(struct golioth_client *client)
{
    golioth_cancel_all_observations_by_prefix(client, NULL);
}

static int golioth_deregister_observation(golioth_coap_request_msg_t *req,
                                          struct golioth_client *client)
{
    struct coap_packet packet;
    const uint8_t **pathv = PATHV(req->path_prefix, req->path);
    size_t path_len = coap_pathv_estimate_alloc_len(pathv);
    size_t packet_len = GOLIOTH_COAP_MAX_NON_PAYLOAD_LEN + path_len;
    uint8_t buffer[packet_len];
    int err;

    err = coap_packet_init(&packet,
                           buffer,
                           sizeof(buffer),
                           COAP_VERSION_1,
                           COAP_TYPE_CON,
                           req->token_len,
                           req->token,
                           COAP_METHOD_GET,
                           coap_next_id());
    if (err)
    {
        return err;
    }

    err = coap_append_option_int(&packet, COAP_OPTION_OBSERVE, 1); /* deregister */

    if (err)
    {
        LOG_ERR("Unable add observe deregister option");
        return err;
    }

    err = coap_packet_append_uri_path_from_pathv(&packet, pathv);
    if (err)
    {
        LOG_ERR("Unable add path option");
        return err;
    }

    err = coap_append_option_int(&packet, COAP_OPTION_ACCEPT, req->observe.content_type);
    if (err)
    {
        LOG_ERR("Unable to add content format to packet");
        return err;
    }

    err = golioth_send_coap(client, &packet);
    if (err)
    {
        LOG_ERR("Unable to send observe deregister packet");
        return err;
    }

    return err;
}

static void reestablish_observations(struct golioth_client *client)
{
    golioth_coap_observe_info_t *obs_info = NULL;
    for (int i = 0; i < CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS; i++)
    {
        obs_info = &client->observations[i];
        if (obs_info->in_use)
        {
            golioth_coap_observe(&obs_info->req, client);
        }
    }
}

static enum golioth_status coap_io_loop_once(struct golioth_client *client)
{
    golioth_coap_request_msg_t *req;
    int err = 0;

    req = calloc(1, sizeof(*req));
    if (!req)
    {
        err = -ENOMEM;
        goto free_req;
    }

    // Wait for request message, with timeout
    bool got_request_msg =
        golioth_mbox_recv(client->request_queue, req, CONFIG_GOLIOTH_COAP_REQUEST_QUEUE_TIMEOUT_MS);
    if (!got_request_msg)
    {
        // No requests, so process other pending IO (e.g. observations)
        goto free_req;
    }

    // Make sure the request isn't too old
    if (golioth_sys_now_ms() > req->ageout_ms)
    {
        LOG_WRN("Ignoring request that has aged out, type %d, path %s",
                req->type,
                (req->path ? req->path : "N/A"));

        if (req->type == GOLIOTH_COAP_REQUEST_POST && req->post.payload_size > 0)
        {
            golioth_sys_free(req->post.payload);
        }

        if (req->type == GOLIOTH_COAP_REQUEST_POST_BLOCK && req->post_block.payload_size > 0)
        {
            golioth_sys_free(req->post_block.payload);
        }

        if (req->request_complete_event)
        {
            golioth_event_group_destroy(req->request_complete_event);
            golioth_sys_sem_destroy(req->request_complete_ack_sem);
        }

        goto free_req;
    }

    req->client = client;

    // Handle message and send request to server
    switch (req->type)
    {
        case GOLIOTH_COAP_REQUEST_EMPTY:
            LOG_DBG("Handle EMPTY");
            err = golioth_send_coap_empty(req->client);
            goto free_req;
        case GOLIOTH_COAP_REQUEST_GET:
            LOG_DBG("Handle GET %s", req->path);
            err = golioth_coap_req_cb(req->client,
                                      COAP_METHOD_GET,
                                      PATHV(req->path_prefix, req->path),
                                      golioth_content_type_to_coap_format(req->get.content_type),
                                      NULL,
                                      0,
                                      golioth_coap_cb,
                                      req,
                                      0);
            break;
        case GOLIOTH_COAP_REQUEST_GET_BLOCK:
            LOG_DBG("Handle GET_BLOCK %s", req->path);
            err = golioth_coap_get_block(req);
            break;
        case GOLIOTH_COAP_REQUEST_POST:
            LOG_DBG("Handle POST %s", req->path);
            err = golioth_coap_req_cb(req->client,
                                      COAP_METHOD_POST,
                                      PATHV(req->path_prefix, req->path),
                                      golioth_content_type_to_coap_format(req->post.content_type),
                                      req->post.payload,
                                      req->post.payload_size,
                                      golioth_coap_cb,
                                      req,
                                      0);
            golioth_sys_free(req->post.payload);
            break;
        case GOLIOTH_COAP_REQUEST_POST_BLOCK:
            LOG_DBG("Handle POST_BLOCK %s", req->path);
            err = golioth_coap_post_block(req);
            golioth_sys_free(req->post_block.payload);
            break;
        case GOLIOTH_COAP_REQUEST_DELETE:
            LOG_DBG("Handle DELETE %s", req->path);
            err = golioth_coap_req_cb(req->client,
                                      COAP_METHOD_DELETE,
                                      PATHV(req->path_prefix, req->path),
                                      COAP_CONTENT_FORMAT_APP_OCTET_STREAM /* not used */,
                                      NULL,
                                      0,
                                      golioth_coap_cb,
                                      req,
                                      0);
            break;
        case GOLIOTH_COAP_REQUEST_OBSERVE:
            LOG_DBG("Handle OBSERVE %s", req->path);
            err = add_observation(req, client);
            if (err == GOLIOTH_ERR_QUEUE_FULL)
            {
                /* Observations are full, free the req but don't treat as a coap error */
                err = GOLIOTH_OK;
                goto free_req;
            }
            /* Need to free local req message; observations use client slots for req messages */
            goto free_req;
            break;
        case GOLIOTH_COAP_REQUEST_OBSERVE_RELEASE:
            LOG_DBG("Handle OBSERVE RELEASE %s", req->path);
            err = golioth_deregister_observation(req, client);
            break;
        default:
            LOG_WRN("Unknown request_msg type: %u", req->type);
            err = -EINVAL;
            goto free_req;
    }

    if (err)
    {
        goto free_req;
    }

    return GOLIOTH_OK;

free_req:
    free(req);

    return golioth_err_to_status(err);
}

static void on_keepalive(golioth_sys_timer_t timer, void *arg)
{
    struct golioth_client *client = arg;
    if (client->is_running && golioth_client_num_items_in_request_queue(client) == 0)
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

static int golioth_close(int sock)
{
    int ret;

    if (sock < 0)
    {
        return -ENOTCONN;
    }

    ret = zsock_close(sock);

    if (ret < 0)
    {
        return -errno;
    }

    return 0;
}

void golioth_init(struct golioth_client *client)
{
    memset(client, 0, sizeof(*client));

    client->sock = -1;

    golioth_coap_reqs_init(client);
}

static int golioth_setsockopt_dtls(struct golioth_client *client, int sock, const char *host)
{
    int ret;

    if (client->tls.sec_tag_list && client->tls.sec_tag_count)
    {
        ret = zsock_setsockopt(sock,
                               SOL_TLS,
                               TLS_SEC_TAG_LIST,
                               client->tls.sec_tag_list,
                               client->tls.sec_tag_count * sizeof(*client->tls.sec_tag_list));
        if (ret < 0)
        {
            return -errno;
        }
    }

    if (IS_ENABLED(CONFIG_GOLIOTH_HOSTNAME_VERIFICATION))
    {
        /*
         * NOTE: At the time of implementation, mbedTLS supported only DNS entries in X509
         * Subject Alternative Name, so providing string representation of IP address will
         * fail (during handshake). If this is the case, you can can still connect if you
         * set CONFIG_GOLIOTH_HOSTNAME_VERIFICATION_SKIP=y, which disables hostname
         * verification.
         */
        ret = zsock_setsockopt(sock,
                               SOL_TLS,
                               TLS_HOSTNAME,
                               COND_CODE_1(CONFIG_GOLIOTH_HOSTNAME_VERIFICATION_SKIP,
                                           (NULL, 0),
                                           (host, strlen(host) + 1)));
        if (ret < 0)
        {
            return -errno;
        }
    }

    if (sizeof(golioth_ciphersuites) > 0)
    {
        ret = zsock_setsockopt(sock,
                               SOL_TLS,
                               TLS_CIPHERSUITE_LIST,
                               golioth_ciphersuites,
                               sizeof(golioth_ciphersuites));
        if (ret < 0)
        {
            return -errno;
        }
    }

    return 0;
}

static int golioth_connect_sockaddr(struct golioth_client *client,
                                    const char *host,
                                    struct sockaddr *addr,
                                    socklen_t addrlen)
{
    int sock;
    int ret;
    int err = 0;

    sock = zsock_socket(addr->sa_family, SOCK_DGRAM, IPPROTO_DTLS_1_2);
    if (sock < 0)
    {
        LOG_ERR("Failed to create socket: %d", -errno);
        return -errno;
    }

    err = golioth_setsockopt_dtls(client, sock, host);
    if (err)
    {
        LOG_ERR("Failed to set DTLS socket options: %d", err);
        goto close_sock;
    }

    ret = zsock_connect(sock, addr, addrlen);
    if (ret < 0)
    {
        err = -errno;
        LOG_ERR("Failed to connect to socket: %d", err);
        goto close_sock;
    }

    client->sock = sock;

    /* Send empty packet to start TLS handshake */
    err = golioth_send_coap_empty(client);
    if (err)
    {
        LOG_ERR("Failed to send empty CoAP message: %d", err);
        client->sock = -1;
    }

close_sock:
    if (err)
    {
        golioth_close(sock);
    }

    return err;
}

#if CONFIG_GOLIOTH_LOG_LEVEL >= LOG_LEVEL_DBG
#define LOG_SOCKADDR(fmt, addr)                                                    \
    do                                                                             \
    {                                                                              \
        char buf[NET_IPV6_ADDR_LEN];                                               \
                                                                                   \
        if (addr->sa_family == AF_INET6)                                           \
        {                                                                          \
            net_addr_ntop(AF_INET6, &net_sin6(addr)->sin6_addr, buf, sizeof(buf)); \
        }                                                                          \
        else if (addr->sa_family == AF_INET)                                       \
        {                                                                          \
            net_addr_ntop(AF_INET, &net_sin(addr)->sin_addr, buf, sizeof(buf));    \
        }                                                                          \
                                                                                   \
        LOG_DBG(fmt, buf);                                                         \
    } while (0)
#else
#define LOG_SOCKADDR(fmt, addr)
#endif

static int golioth_connect_host_port(struct golioth_client *client,
                                     const char *host,
                                     const char *port)
{
    struct zsock_addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = IPPROTO_UDP,
    };
    struct zsock_addrinfo *addrs, *addr;
    int ret;
    int err = -ENOENT;

    ret = zsock_getaddrinfo(host, port, &hints, &addrs);
    if (ret < 0)
    {
        LOG_ERR("Fail to get address (%s %s) %d", host, port, ret);
        return -EAGAIN;
    }

    for (addr = addrs; addr != NULL; addr = addr->ai_next)
    {
        LOG_SOCKADDR("Trying addr '%s'", addr->ai_addr);

        err = golioth_connect_sockaddr(client, host, addr->ai_addr, addr->ai_addrlen);
        if (!err)
        {
            /* Ready to go */
            break;
        }
    }

    zsock_freeaddrinfo(addrs);

    return err;
}

static int golioth_connect(struct golioth_client *client)
{
    char uri[] = CONFIG_GOLIOTH_COAP_HOST_URI;
    char *host = &uri[sizeof("coaps://") - 1];
    char ipv6_addr[40];
    const char *port = "5684";
    char *colon;
    int err;

    if (client->sock >= 0)
    {
        return -EALREADY;
    }

    if (strncmp(uri, "coaps://", sizeof("coaps://") - 1) != 0)
    {
        return -EINVAL;
    }

    colon = strchr(host, ':');
    if (colon)
    {
        *colon = '\0';
        port = colon + 1;
    }

    if (IS_ENABLED(CONFIG_NET_L2_OPENTHREAD))
    {
        err = golioth_ot_synthesize_ipv6_address(host, ipv6_addr);
        if (err)
        {
            LOG_ERR("Failed to synthesize Golioth Server IPv6 address: %d", err);
            return err;
        }

        host = ipv6_addr;
    }

    err = golioth_connect_host_port(client, host, port);
    if (err)
    {
        LOG_ERR("Failed to connect: %d", err);
        return err;
    }

    golioth_coap_reqs_on_connect(client);

    if (client->on_connect)
    {
        client->on_connect(client);
    }

    fds[POLLFD_SOCKET].fd = client->sock;
    fds[POLLFD_SOCKET].events = ZSOCK_POLLIN;

    return 0;
}

static void golioth_disconnect(struct golioth_client *client)
{
    struct k_work_sync sync;

    golioth_coap_reqs_on_disconnect(client);

    golioth_close(client->sock);
    client->sock = -1;

    k_work_cancel_delayable_sync(&eventfd_timeout, &sync);
}

static void golioth_poll_prepare(struct golioth_client *client,
                                 int64_t now,
                                 int *fd,
                                 int64_t *timeout)
{
    if (fd)
    {
        *fd = client->sock;
    }

    if (timeout)
    {
        *timeout = golioth_coap_reqs_poll_prepare(client, now);
    }
}

int golioth_send_coap(struct golioth_client *client, struct coap_packet *packet)
{
    return golioth_send(client, packet->data, packet->offset, 0);
}

static int golioth_ping(struct golioth_client *client)
{
    struct coap_packet packet;
    uint8_t buffer[GOLIOTH_EMPTY_PACKET_LEN];
    int err;

    err = coap_packet_init(&packet,
                           buffer,
                           sizeof(buffer),
                           COAP_VERSION_1,
                           COAP_TYPE_CON,
                           0,
                           NULL,
                           COAP_CODE_EMPTY,
                           coap_next_id());
    if (err)
    {
        return err;
    }

    return golioth_send_coap(client, &packet);
}

static int golioth_ack_packet(struct golioth_client *client, struct coap_packet *rx)
{
    struct coap_packet tx;
    uint8_t buffer[GOLIOTH_EMPTY_PACKET_LEN + COAP_TOKEN_MAX_LEN];
    int err;

    err = coap_ack_init(&tx, rx, buffer, sizeof(buffer), COAP_CODE_EMPTY);
    if (err)
    {
        return err;
    }

    return golioth_send_coap(client, &tx);
}

static int golioth_process_rx_data(struct golioth_client *client, uint8_t *data, size_t len)
{
    int err;
    uint8_t type;

    err = coap_packet_parse(&client->rx_packet, data, len, NULL, 0);
    if (err)
    {
        return err;
    }

    golioth_coap_req_process_rx(client, &client->rx_packet);

    type = coap_header_get_type(&client->rx_packet);
    if (type == COAP_TYPE_CON)
    {
        golioth_ack_packet(client, &client->rx_packet);
    }

    return 0;
}

static int golioth_process_rx_ping(struct golioth_client *client, uint8_t *data, size_t len)
{
    int err;

    err = coap_packet_parse(&client->rx_packet, data, len, NULL, 0);
    if (err)
    {
        return err;
    }

    return golioth_ack_packet(client, &client->rx_packet);
}

static int golioth_recv(struct golioth_client *client, uint8_t *data, size_t len, int flags)
{
    ssize_t rcvd;

    if (client->sock < 0)
    {
        return -ENOTCONN;
    }

    rcvd = zsock_recv(client->sock, data, len, flags);
    if (rcvd < 0)
    {
        return -errno;
    }
    else if (rcvd == 0)
    {
        return -ENOTCONN;
    }

    return rcvd;
}

static int golioth_process_rx(struct golioth_client *client)
{
    int flags =
        ZSOCK_MSG_DONTWAIT | (IS_ENABLED(CONFIG_GOLIOTH_RECV_USE_MSG_TRUNC) ? ZSOCK_MSG_TRUNC : 0);
    int ret;
    int err;

    ret = golioth_recv(client, client->rx_buffer, client->rx_buffer_len, flags);
    if (ret == -EAGAIN || ret == -EWOULDBLOCK)
    {
        /* no pending data */
        return 0;
    }
    else if (ret < 0)
    {
        return ret;
    }

    client->rx_received = ret;

    if (ret > client->rx_buffer_len)
    {
        LOG_WRN("Truncated packet (%zu -> %zu)", (size_t) ret, client->rx_buffer_len);
        ret = client->rx_buffer_len;
    }

    err = coap_data_check_rx_packet_type(client->rx_buffer, ret);
    if (err == -ENOMSG)
    {
        /* ping */
        return golioth_process_rx_ping(client, client->rx_buffer, ret);
    }
    else if (err)
    {
        return err;
    }

    return golioth_process_rx_data(client, client->rx_buffer, ret);
}

static void golioth_coap_client_thread(void *arg)
{
    struct golioth_client *client = arg;
    bool event_occurred;
    int timeout;
    int64_t recv_expiry = 0;
    int64_t ping_expiry = 0;
    int64_t golioth_timeout;
    eventfd_t eventfd_value;
    int err;
    int ret;

    fds[POLLFD_MBOX].fd = golioth_sys_sem_get_fd(client->request_queue->fill_count_sem);
    fds[POLLFD_MBOX].events = ZSOCK_POLLIN;

    while (1)
    {
        client->session_connected = false;

        client->is_running = false;
        LOG_DBG("Waiting for the \"run\" signal");
        k_poll(&client->run_event, 1, K_FOREVER);
        client->run_event.state = K_POLL_STATE_NOT_READY;
        LOG_DBG("Received \"run\" signal");
        client->is_running = true;

        /* Flush pending events */
        (void) eventfd_read(fds[POLLFD_EVENT].fd, &eventfd_value);

        err = golioth_connect(client);
        if (err)
        {
            LOG_WRN("Failed to connect: %d", err);
            k_sleep(K_SECONDS(5));
            continue;
        }

        LOG_INF("Golioth CoAP client connected");
        client->session_connected = true;

        golioth_sys_client_connected(client);
        if (client->event_callback)
        {
            client->event_callback(client,
                                   GOLIOTH_CLIENT_EVENT_CONNECTED,
                                   client->event_callback_arg);
        }

        recv_expiry = k_uptime_get() + RECV_TIMEOUT;
        ping_expiry = k_uptime_get() + PING_INTERVAL;

        // Enqueue an asynchronous EMPTY request immediately.
        //
        // This is done so we can determine quickly whether we are connected
        // to the cloud or not.
        if (golioth_client_num_items_in_request_queue(client) == 0)
        {
            golioth_coap_client_empty(client, false, GOLIOTH_SYS_WAIT_FOREVER);
        }

        // If we are re-connecting and had prior observations, set
        // them up again now (tokens will be updated).
        reestablish_observations(client);

        LOG_INF("Entering CoAP I/O loop");
        while (true)
        {
            event_occurred = false;

            golioth_poll_prepare(client, k_uptime_get(), NULL, &golioth_timeout);

            timeout = MIN(recv_expiry, ping_expiry) - k_uptime_get();
            timeout = MIN(timeout, golioth_timeout);

            if (timeout < 0)
            {
                timeout = 0;
            }

            LOG_DBG("Next timeout: %d", timeout);

            k_work_reschedule(&eventfd_timeout, K_MSEC(timeout));

            ret = zsock_poll(fds, ARRAY_SIZE(fds), -1);

            if (ret < 0)
            {
                LOG_ERR("Error in poll:%d", errno);
                break;
            }

            if (ret == 0)
            {
                LOG_DBG("Timeout in poll");
                event_occurred = true;
            }

            if (fds[POLLFD_EVENT].revents)
            {
                (void) eventfd_read(fds[POLLFD_EVENT].fd, &eventfd_value);
                LOG_DBG("Event in eventfd");
                event_occurred = true;
            }

            if (event_occurred)
            {
                bool stop_request = atomic_test_and_clear_bit(&flags, FLAG_STOP_CLIENT);
                bool receive_timeout = (recv_expiry <= k_uptime_get());

                /*
                 * Stop requests are handled similar to recv timeout.
                 */
                if (receive_timeout || stop_request)
                {
                    if (stop_request)
                    {
                        LOG_INF("Stop request");
                    }
                    else
                    {
                        LOG_WRN("Receive timeout");
                    }

                    break;
                }

                if (ping_expiry <= k_uptime_get())
                {
                    LOG_DBG("Sending PING");
                    (void) golioth_ping(client);

                    ping_expiry = k_uptime_get() + PING_INTERVAL;
                }
            }

            if (fds[POLLFD_SOCKET].revents)
            {
                recv_expiry = k_uptime_get() + RECV_TIMEOUT;
                ping_expiry = k_uptime_get() + PING_INTERVAL;

                err = golioth_process_rx(client);
                if (err)
                {
                    LOG_ERR("Failed to receive: %d", err);
                    break;
                }
            }

            if (fds[POLLFD_MBOX].revents)
            {
                if (coap_io_loop_once(client) != GOLIOTH_OK)
                {
                    break;
                }
            }
        }

        LOG_INF("Ending session");

        golioth_sys_client_disconnected(client);
        if (client->event_callback && client->session_connected)
        {
            client->event_callback(client,
                                   GOLIOTH_CLIENT_EVENT_DISCONNECTED,
                                   client->event_callback_arg);
        }
        client->session_connected = false;

        golioth_disconnect(client);

        // Small delay before starting a new session
        golioth_sys_msleep(1000);
    }
}

static int credentials_set_psk(const struct golioth_psk_credential *psk)
{
    int err;

    err = tls_credential_delete(sec_tag_list[0], TLS_CREDENTIAL_PSK_ID);
    if ((err) && (err != -ENOENT))
    {
        LOG_ERR("Failed to delete PSK ID: %d", err);
    }

    err = tls_credential_add(sec_tag_list[0], TLS_CREDENTIAL_PSK_ID, psk->psk_id, psk->psk_id_len);
    if (err)
    {
        LOG_ERR("Failed to register PSK ID: %d", err);
        return err;
    }

    err = tls_credential_delete(sec_tag_list[0], TLS_CREDENTIAL_PSK);
    if ((err) && (err != -ENOENT))
    {
        LOG_ERR("Failed to delete PSK: %d", err);
    }

    err = tls_credential_add(sec_tag_list[0], TLS_CREDENTIAL_PSK, psk->psk, psk->psk_len);
    if (err)
    {
        LOG_ERR("Failed to register PSK: %d", err);
        return err;
    }

    return 0;
}

static int credentials_set_pki(const struct golioth_pki_credential *pki)
{
    int err;

    err = tls_credential_add(sec_tag_list[0],
                             TLS_CREDENTIAL_CA_CERTIFICATE,
                             pki->ca_cert,
                             pki->ca_cert_len);
    if (err)
    {
        return 0;
    }

    err = tls_credential_add(sec_tag_list[0],
                             TLS_CREDENTIAL_SERVER_CERTIFICATE,
                             pki->public_cert,
                             pki->public_cert_len);
    if (err)
    {
        return 0;
    }

    err = tls_credential_add(sec_tag_list[0],
                             TLS_CREDENTIAL_PRIVATE_KEY,
                             pki->private_key,
                             pki->private_key_len);
    if (err)
    {
        return 0;
    }

    return 1;
}

static int credentials_set_tag(int tag)
{
    sec_tag_list[0] = tag;

    return 0;
}

static int credentials_set(const struct golioth_client_config *config)
{
    const struct golioth_credential *credentials = &config->credentials;

    switch (credentials->auth_type)
    {
        case GOLIOTH_TLS_AUTH_TYPE_PSK:
            return credentials_set_psk(&credentials->psk);
        case GOLIOTH_TLS_AUTH_TYPE_PKI:
            return credentials_set_pki(&credentials->pki);
        case GOLIOTH_TLS_AUTH_TYPE_TAG:
            return credentials_set_tag(credentials->tag);
    }

    return -ENOTSUP;
}

struct golioth_client *golioth_client_create(const struct golioth_client_config *config)
{
    if (!_initialized)
    {
        _initialized = true;
    }

    struct golioth_client *new_client = golioth_sys_malloc(sizeof(struct golioth_client));
    if (!new_client)
    {
        LOG_ERR("Failed to allocate memory for client");
        goto error;
    }
    memset(new_client, 0, sizeof(struct golioth_client));

    new_client->config = *config;

    credentials_set(&new_client->config);

    golioth_init(new_client);
    new_client->rx_buffer = rx_buffer;
    new_client->rx_buffer_len = sizeof(rx_buffer);

    new_client->wakeup = golioth_client_wakeup;

    new_client->tls.sec_tag_list = sec_tag_list;
    new_client->tls.sec_tag_count = ARRAY_SIZE(sec_tag_list);

    fds[POLLFD_EVENT].fd = eventfd(0, EFD_NONBLOCK);
    fds[POLLFD_EVENT].events = ZSOCK_POLLIN;

    k_sem_init(&new_client->run_sem, 1, 1);
    k_poll_event_init(&new_client->run_event,
                      K_POLL_TYPE_SEM_AVAILABLE,
                      K_POLL_MODE_NOTIFY_ONLY,
                      &new_client->run_sem);

    new_client->request_queue = golioth_mbox_create(CONFIG_GOLIOTH_COAP_REQUEST_QUEUE_MAX_ITEMS,
                                                    sizeof(golioth_coap_request_msg_t));
    if (!new_client->request_queue)
    {
        LOG_ERR("Failed to create request queue");
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
        LOG_ERR("Failed to create client thread");
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
        LOG_ERR("Failed to create keepalive timer");
        goto error;
    }

    if (CONFIG_GOLIOTH_COAP_KEEPALIVE_INTERVAL_S > 0)
    {
        if (!golioth_sys_timer_start(new_client->keepalive_timer))
        {
            LOG_ERR("Failed to start keepalive timer");
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
    }
}

void golioth_client_destroy(struct golioth_client *client)
{
    if (!client)
    {
        return;
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
    golioth_sys_free(client);
}

enum golioth_status golioth_client_start(struct golioth_client *client)
{
    if (!client)
    {
        return GOLIOTH_ERR_NULL;
    }
    atomic_clear_bit(&flags, FLAG_STOP_CLIENT);
    k_sem_give(&client->run_sem);
    return GOLIOTH_OK;
}

enum golioth_status golioth_client_stop(struct golioth_client *client)
{
    if (!client)
    {
        return GOLIOTH_ERR_NULL;
    }

    GLTH_LOGI(TAG, "Attempting to stop client");

    k_sem_take(&client->run_sem, K_NO_WAIT);

    atomic_set_bit(&flags, FLAG_STOP_CLIENT);

    golioth_client_wakeup(client);

    // Wait for client to be fully stopped
    while (golioth_client_is_running(client))
    {
        golioth_sys_msleep(100);
    }

    return GOLIOTH_OK;
}
