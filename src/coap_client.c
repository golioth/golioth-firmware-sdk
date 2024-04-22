/*
 * Copyright (c) 2022-2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <string.h>
#include <golioth/golioth_debug.h>

#ifdef __ZEPHYR__
#include "coap_client_zephyr.h"
#else
#include "coap_client_libcoap.h"
#endif

LOG_TAG_DEFINE(golioth_coap_client);

static void purge_request_mbox(golioth_mbox_t request_mbox)
{
    golioth_coap_request_msg_t request_msg = {};
    size_t num_messages = golioth_mbox_num_messages(request_mbox);

    for (size_t i = 0; i < num_messages; i++)
    {
        assert(golioth_mbox_recv(request_mbox, &request_msg, 0));
        if (request_msg.type == GOLIOTH_COAP_REQUEST_POST)
        {
            // free dynamically allocated user payload copy
            golioth_sys_free(request_msg.post.payload);
        }
    }
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
    if (client->run_sem)
    {
        golioth_sys_sem_destroy(client->run_sem);
    }
    golioth_sys_free(client);
}

bool golioth_client_is_connected(struct golioth_client *client)
{
    if (!client)
    {
        return false;
    }
    return client->session_connected;
}

enum golioth_status golioth_coap_client_empty(struct golioth_client *client,
                                              bool is_synchronous,
                                              int32_t timeout_s)
{
    if (!client)
    {
        return GOLIOTH_ERR_NULL;
    }

    if (!client->is_running)
    {
        GLTH_LOGW(TAG, "Client not running, dropping empty request");
        return GOLIOTH_ERR_INVALID_STATE;
    }

    uint64_t ageout_ms = GOLIOTH_SYS_WAIT_FOREVER;
    if (timeout_s != GOLIOTH_SYS_WAIT_FOREVER)
    {
        ageout_ms = golioth_sys_now_ms() + (1000 * timeout_s);
    }

    golioth_coap_request_msg_t request_msg = {
        .type = GOLIOTH_COAP_REQUEST_EMPTY,
        .ageout_ms = ageout_ms,
    };
    enum golioth_status status = GOLIOTH_OK;

    if (is_synchronous)
    {
        // Created here, deleted by coap thread (or here if fail to enqueue
        request_msg.request_complete_event = golioth_event_group_create();
        request_msg.request_complete_ack_sem = golioth_sys_sem_create(1, 0);
        request_msg.status = &status;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGW(TAG, "Failed to enqueue request, queue full");
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
            request_msg.status = NULL;
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        int32_t tmo_ms = timeout_s * 1000;
        if (timeout_s == GOLIOTH_SYS_WAIT_FOREVER)
        {
            tmo_ms = GOLIOTH_SYS_WAIT_FOREVER;
        }
        uint32_t bits =
            golioth_event_group_wait_bits(request_msg.request_complete_event,
                                          RESPONSE_RECEIVED_EVENT_BIT | RESPONSE_TIMEOUT_EVENT_BIT,
                                          true,  // clear bits after waiting
                                          tmo_ms);

        // Notify CoAP thread that we received the event
        golioth_sys_sem_give(request_msg.request_complete_ack_sem);

        if ((bits == 0) || (bits & RESPONSE_TIMEOUT_EVENT_BIT))
        {
            return GOLIOTH_ERR_TIMEOUT;
        }

        return status;
    }
    return GOLIOTH_OK;
}

enum golioth_status golioth_coap_client_set(struct golioth_client *client,
                                            const char *path_prefix,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            const uint8_t *payload,
                                            size_t payload_size,
                                            golioth_set_cb_fn callback,
                                            void *callback_arg,
                                            bool is_synchronous,
                                            int32_t timeout_s)
{
    if (!client || !path)
    {
        return GOLIOTH_ERR_NULL;
    }

    uint8_t *request_payload = NULL;

    if (!client->is_running)
    {
        GLTH_LOGW(TAG, "Client not running, dropping set request for path %s%s", path_prefix, path);
        return GOLIOTH_ERR_INVALID_STATE;
    }

    if (payload_size > 0)
    {
        // We will allocate memory and copy the payload
        // to avoid payload lifetime and thread-safety issues.
        //
        // This memory will be free'd by the CoAP thread after handling the request,
        // or in this function if we fail to enqueue the request.
        request_payload = (uint8_t *) golioth_sys_malloc(payload_size);
        if (!request_payload)
        {
            GLTH_LOGE(TAG, "Payload alloc failure");
            return GOLIOTH_ERR_MEM_ALLOC;
        }
        memset(request_payload, 0, payload_size);
        memcpy(request_payload, payload, payload_size);
    }

    uint64_t ageout_ms = GOLIOTH_SYS_WAIT_FOREVER;
    if (timeout_s != GOLIOTH_SYS_WAIT_FOREVER)
    {
        ageout_ms = golioth_sys_now_ms() + (1000 * timeout_s);
    }

    golioth_coap_request_msg_t request_msg = {
        .type = GOLIOTH_COAP_REQUEST_POST,
        .path_prefix = path_prefix,
        .post =
            {
                .content_type = content_type,
                .payload = request_payload,
                .payload_size = payload_size,
                .callback = callback,
                .arg = callback_arg,
            },
        .ageout_ms = ageout_ms,
    };
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);
    enum golioth_status status = GOLIOTH_OK;

    if (is_synchronous)
    {
        // Created here, deleted by coap thread (or here if fail to enqueue
        request_msg.request_complete_event = golioth_event_group_create();
        request_msg.request_complete_ack_sem = golioth_sys_sem_create(1, 0);
        request_msg.status = &status;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        /* NOTE: Logging a message here when cloud logging is enabled can cause
         *       a loop where the logging thread attempts to enqueue a message,
         *       the mbox is full, so coap_client writes a log, which the
         *       logging thread attempts to send to the cloud, and so on.
         */
        if (payload_size > 0)
        {
            golioth_sys_free(request_payload);
        }
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
            request_msg.status = NULL;
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        int32_t tmo_ms = timeout_s * 1000;
        if (timeout_s == GOLIOTH_SYS_WAIT_FOREVER)
        {
            tmo_ms = GOLIOTH_SYS_WAIT_FOREVER;
        }
        uint32_t bits =
            golioth_event_group_wait_bits(request_msg.request_complete_event,
                                          RESPONSE_RECEIVED_EVENT_BIT | RESPONSE_TIMEOUT_EVENT_BIT,
                                          true,  // clear bits after waiting
                                          tmo_ms);

        // Notify CoAP thread that we received the event
        golioth_sys_sem_give(request_msg.request_complete_ack_sem);

        if ((bits == 0) || (bits & RESPONSE_TIMEOUT_EVENT_BIT))
        {
            return GOLIOTH_ERR_TIMEOUT;
        }

        return status;
    }
    return GOLIOTH_OK;
}

enum golioth_status golioth_coap_client_delete(struct golioth_client *client,
                                               const char *path_prefix,
                                               const char *path,
                                               golioth_set_cb_fn callback,
                                               void *callback_arg,
                                               bool is_synchronous,
                                               int32_t timeout_s)
{
    if (!client || !path)
    {
        return GOLIOTH_ERR_NULL;
    }

    if (!client->is_running)
    {
        GLTH_LOGW(TAG,
                  "Client not running, dropping delete request for path %s%s",
                  path_prefix,
                  path);
        return GOLIOTH_ERR_INVALID_STATE;
    }

    uint64_t ageout_ms = GOLIOTH_SYS_WAIT_FOREVER;
    if (timeout_s != GOLIOTH_SYS_WAIT_FOREVER)
    {
        ageout_ms = golioth_sys_now_ms() + (1000 * timeout_s);
    }

    golioth_coap_request_msg_t request_msg = {
        .type = GOLIOTH_COAP_REQUEST_DELETE,
        .path_prefix = path_prefix,
        .delete =
            {
                .callback = callback,
                .arg = callback_arg,
            },
        .ageout_ms = ageout_ms,
    };
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);
    enum golioth_status status = GOLIOTH_OK;

    if (is_synchronous)
    {
        // Created here, deleted by coap thread (or here if fail to enqueue
        request_msg.request_complete_event = golioth_event_group_create();
        request_msg.request_complete_ack_sem = golioth_sys_sem_create(1, 0);
        request_msg.status = &status;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGW(TAG, "Failed to enqueue request, queue full");
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
            request_msg.status = NULL;
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        int32_t tmo_ms = timeout_s * 1000;
        if (timeout_s == GOLIOTH_SYS_WAIT_FOREVER)
        {
            tmo_ms = GOLIOTH_SYS_WAIT_FOREVER;
        }
        uint32_t bits =
            golioth_event_group_wait_bits(request_msg.request_complete_event,
                                          RESPONSE_RECEIVED_EVENT_BIT | RESPONSE_TIMEOUT_EVENT_BIT,
                                          true,  // clear bits after waiting
                                          tmo_ms);

        // Notify CoAP thread that we received the event
        golioth_sys_sem_give(request_msg.request_complete_ack_sem);

        if ((bits == 0) || (bits & RESPONSE_TIMEOUT_EVENT_BIT))
        {
            return GOLIOTH_ERR_TIMEOUT;
        }

        return status;
    }
    return GOLIOTH_OK;
}

static enum golioth_status golioth_coap_client_get_internal(struct golioth_client *client,
                                                            const char *path_prefix,
                                                            const char *path,
                                                            golioth_coap_request_type_t type,
                                                            void *request_params,
                                                            bool is_synchronous,
                                                            int32_t timeout_s)
{
    if (!client || !path)
    {
        return GOLIOTH_ERR_NULL;
    }

    if (!client->is_running)
    {
        GLTH_LOGW(TAG, "Client not running, dropping get request for path %s%s", path_prefix, path);
        return GOLIOTH_ERR_INVALID_STATE;
    }

    uint64_t ageout_ms = GOLIOTH_SYS_WAIT_FOREVER;
    if (timeout_s != GOLIOTH_SYS_WAIT_FOREVER)
    {
        ageout_ms = golioth_sys_now_ms() + (1000 * timeout_s);
    }

    golioth_coap_request_msg_t request_msg = {};
    enum golioth_status status = GOLIOTH_OK;
    request_msg.type = type;
    request_msg.path_prefix = path_prefix;
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);
    if (is_synchronous)
    {
        // Created here, deleted by coap thread (or here if fail to enqueue
        request_msg.request_complete_event = golioth_event_group_create();
        request_msg.request_complete_ack_sem = golioth_sys_sem_create(1, 0);
        request_msg.status = &status;
    }
    request_msg.ageout_ms = ageout_ms;
    if (type == GOLIOTH_COAP_REQUEST_GET_BLOCK)
    {
        request_msg.get_block = *(golioth_coap_get_block_params_t *) request_params;
    }
    else
    {
        assert(type == GOLIOTH_COAP_REQUEST_GET);
        request_msg.get = *(golioth_coap_get_params_t *) request_params;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGE(TAG, "Failed to enqueue request, queue full");
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
            request_msg.status = NULL;
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        int32_t tmo_ms = timeout_s * 1000;
        if (timeout_s == GOLIOTH_SYS_WAIT_FOREVER)
        {
            tmo_ms = GOLIOTH_SYS_WAIT_FOREVER;
        }
        uint32_t bits =
            golioth_event_group_wait_bits(request_msg.request_complete_event,
                                          RESPONSE_RECEIVED_EVENT_BIT | RESPONSE_TIMEOUT_EVENT_BIT,
                                          true,  // clear bits after waiting
                                          tmo_ms);

        // Notify CoAP thread that we received the event
        golioth_sys_sem_give(request_msg.request_complete_ack_sem);

        if ((bits == 0) || (bits & RESPONSE_TIMEOUT_EVENT_BIT))
        {
            return GOLIOTH_ERR_TIMEOUT;
        }

        return status;
    }
    return GOLIOTH_OK;
}

enum golioth_status golioth_coap_client_get(struct golioth_client *client,
                                            const char *path_prefix,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            golioth_get_cb_fn callback,
                                            void *arg,
                                            bool is_synchronous,
                                            int32_t timeout_s)
{
    golioth_coap_get_params_t params = {
        .content_type = content_type,
        .callback = callback,
        .arg = arg,
    };
    return golioth_coap_client_get_internal(client,
                                            path_prefix,
                                            path,
                                            GOLIOTH_COAP_REQUEST_GET,
                                            &params,
                                            is_synchronous,
                                            timeout_s);
}

enum golioth_status golioth_coap_client_get_block(struct golioth_client *client,
                                                  const char *path_prefix,
                                                  const char *path,
                                                  enum golioth_content_type content_type,
                                                  size_t block_index,
                                                  size_t block_size,
                                                  golioth_get_block_cb_fn callback,
                                                  void *arg,
                                                  bool is_synchronous,
                                                  int32_t timeout_s)
{
    golioth_coap_get_block_params_t params = {
        .content_type = content_type,
        .block_index = block_index,
        .block_size = block_size,
        .callback = callback,
        .arg = arg,
    };
    return golioth_coap_client_get_internal(client,
                                            path_prefix,
                                            path,
                                            GOLIOTH_COAP_REQUEST_GET_BLOCK,
                                            &params,
                                            is_synchronous,
                                            timeout_s);
}

enum golioth_status golioth_coap_client_observe_async(struct golioth_client *client,
                                                      const char *path_prefix,
                                                      const char *path,
                                                      enum golioth_content_type content_type,
                                                      golioth_get_cb_fn callback,
                                                      void *arg)
{
    if (!client || !path)
    {
        return GOLIOTH_ERR_NULL;
    }

    if (!client->is_running)
    {
        GLTH_LOGW(TAG,
                  "Client not running, dropping observe request for path %s%s",
                  path_prefix,
                  path);
        return GOLIOTH_ERR_INVALID_STATE;
    }

    golioth_coap_request_msg_t request_msg = {
        .type = GOLIOTH_COAP_REQUEST_OBSERVE,
        .path_prefix = path_prefix,
        .ageout_ms = GOLIOTH_SYS_WAIT_FOREVER,
        .observe =
            {
                .content_type = content_type,
                .callback = callback,
                .arg = arg,
            },
    };
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGW(TAG, "Failed to enqueue request, queue full");
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    return GOLIOTH_OK;
}

void golioth_client_register_event_callback(struct golioth_client *client,
                                            golioth_client_event_cb_fn callback,
                                            void *arg)
{
    if (!client)
    {
        return;
    }
    client->event_callback = callback;
    client->event_callback_arg = arg;
}

uint32_t golioth_client_num_items_in_request_queue(struct golioth_client *client)
{
    if (!client)
    {
        return 0;
    }
    return golioth_mbox_num_messages(client->request_queue);
}

golioth_sys_thread_t golioth_client_get_thread(struct golioth_client *client)
{
    return client->coap_thread_handle;
}

bool golioth_client_wait_for_connect(struct golioth_client *client, int timeout_ms)
{
    const uint32_t poll_period_ms = 100;

    if (timeout_ms == -1)
    {
        while (!golioth_client_is_connected(client))
        {
            golioth_sys_msleep(poll_period_ms);
        }
        return true;
    }

    const uint32_t max_iterations = timeout_ms / poll_period_ms + 1;
    for (uint32_t i = 0; i < max_iterations; i++)
    {
        if (golioth_client_is_connected(client))
        {
            return true;
        }
        golioth_sys_msleep(poll_period_ms);
    }
    return false;
}
