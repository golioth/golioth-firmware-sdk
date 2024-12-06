/*
 * Copyright (c) 2022-2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "coap_client.h"
#include <assert.h>
#include <string.h>
#include <golioth/golioth_debug.h>
#include "golioth_util.h"

#ifdef __ZEPHYR__
#include "coap_client_zephyr.h"
#else
#include "coap_client_libcoap.h"
#endif

LOG_TAG_DEFINE(golioth_coap_client);

static golioth_sys_mutex_t token_mut;

static enum golioth_status setup_synchronous(struct golioth_coap_request_msg *request_msg)
{
    // Created here, deleted by coap thread (or here if fail to create or enqueue)
    request_msg->request_complete_event = golioth_event_group_create();
    if (!request_msg->request_complete_event)
    {
        GLTH_LOGW(TAG, "Failed to create event group");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    request_msg->request_complete_ack_sem = golioth_sys_sem_create(1, 0);
    if (!request_msg->request_complete_ack_sem)
    {
        GLTH_LOGW(TAG, "Failed to create semaphore");
        golioth_event_group_destroy(request_msg->request_complete_event);
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    return GOLIOTH_OK;
}

static enum golioth_status wait_for_synchronous(struct golioth_coap_request_msg *request_msg,
                                                int32_t timeout_s)
{
    int32_t tmo_ms = timeout_s * 1000;
    if (timeout_s == GOLIOTH_SYS_WAIT_FOREVER)
    {
        tmo_ms = GOLIOTH_SYS_WAIT_FOREVER;
    }
    uint32_t bits =
        golioth_event_group_wait_bits(request_msg->request_complete_event,
                                      RESPONSE_RECEIVED_EVENT_BIT | RESPONSE_TIMEOUT_EVENT_BIT,
                                      true,  // clear bits after waiting
                                      tmo_ms);

    // Notify CoAP thread that we received the event
    golioth_sys_sem_give(request_msg->request_complete_ack_sem);

    if ((bits == 0) || (bits & RESPONSE_TIMEOUT_EVENT_BIT))
    {
        return GOLIOTH_ERR_TIMEOUT;
    }

    return GOLIOTH_OK;
}

bool golioth_client_is_connected(struct golioth_client *client)
{
    if (!client)
    {
        return false;
    }
    return client->session_connected;
}

void golioth_coap_token_mutex_create(void)
{
    /* Called by golioth_client_create(); created once, never destroyed */
    if (!token_mut)
    {
        token_mut = golioth_sys_mutex_create();
        assert(token_mut);
    }
}

void golioth_coap_next_token(uint8_t token[GOLIOTH_COAP_TOKEN_LEN])
{
    static uint8_t stored_token[GOLIOTH_COAP_TOKEN_LEN] = {0};
    static bool token_is_initialized = false;

    golioth_sys_mutex_lock(token_mut, GOLIOTH_SYS_WAIT_FOREVER);

    /* Systems without a hardware-backed random source need to seed rand. Waiting until now
     * introduces the variability of the network connection time to receive a different ms count on
     * each power cycle to use as the seed. */
    if (!token_is_initialized)
    {
        uint32_t rnd = 0;
        golioth_sys_srand(golioth_sys_now_ms());

        for (int i = 0; i < GOLIOTH_COAP_TOKEN_LEN; i++)
        {
            if (i % 4 == 0)
            {
                rnd = golioth_sys_rand();
            }

            stored_token[i] = (uint8_t) (rnd >> (i % 4));
        }

        token_is_initialized = true;
    }

    uint64_t new_token = 0;
    memcpy(&new_token, stored_token, GOLIOTH_COAP_TOKEN_LEN);
    new_token++;
    memcpy(stored_token, &new_token, GOLIOTH_COAP_TOKEN_LEN);
    memcpy(token, stored_token, GOLIOTH_COAP_TOKEN_LEN);

    golioth_sys_mutex_unlock(token_mut);
}

enum golioth_status golioth_coap_client_empty(struct golioth_client *client,
                                              bool is_synchronous,
                                              int32_t timeout_s)
{
    enum golioth_status status = GOLIOTH_OK;

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

    struct golioth_coap_request_msg request_msg = {
        .type = GOLIOTH_COAP_REQUEST_EMPTY,
        .ageout_ms = ageout_ms,
    };
    enum golioth_status request_result = GOLIOTH_OK;

    if (is_synchronous)
    {
        status = setup_synchronous(&request_msg);

        if (status != GOLIOTH_OK)
        {
            goto finish;
        }

        request_msg.status = &request_result;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGW(TAG, "Failed to enqueue request, queue full");
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        status = wait_for_synchronous(&request_msg, timeout_s);

        if (status == GOLIOTH_OK)
        {
            status = request_result;
        }
    }

finish:
    return status;
}

static enum golioth_status golioth_coap_client_set_internal(
    struct golioth_client *client,
    const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
    const char *path_prefix,
    const char *path,
    const uint8_t *payload,
    size_t payload_size,
    enum golioth_coap_request_type type,
    void *request_params,
    bool is_synchronous,
    int32_t timeout_s)
{
    if (!client || !token || !path)
    {
        return GOLIOTH_ERR_NULL;
    }

    struct golioth_coap_request_msg request_msg = {};
    enum golioth_status status = GOLIOTH_OK;
    enum golioth_status request_result = GOLIOTH_OK;
    uint8_t *request_payload = NULL;

    memcpy(request_msg.token, token, GOLIOTH_COAP_TOKEN_LEN);

    if (!client->is_running)
    {
        GLTH_LOGW(TAG, "Client not running, dropping set request for path %s%s", path_prefix, path);
        return GOLIOTH_ERR_INVALID_STATE;
    }

    if (strlen(path) > sizeof(request_msg.path) - 1)
    {
        GLTH_LOGE(TAG, "Path too long: %zu > %zu", strlen(path), sizeof(request_msg.path) - 1);
        return GOLIOTH_ERR_INVALID_FORMAT;
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

    request_msg.type = type;
    request_msg.path_prefix = path_prefix;
    request_msg.ageout_ms = ageout_ms;

    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);

    if (is_synchronous)
    {
        status = setup_synchronous(&request_msg);

        if (status != GOLIOTH_OK)
        {
            if (request_payload)
            {
                golioth_sys_free(request_payload);
            }
            goto finish;
        }

        request_msg.status = &request_result;
    }

    if (type == GOLIOTH_COAP_REQUEST_POST_BLOCK)
    {
        request_msg.post_block = *(struct golioth_coap_post_block_params *) request_params;
        request_msg.post_block.payload = request_payload;
        request_msg.post_block.payload_size = payload_size;
    }
    else
    {
        assert(type == GOLIOTH_COAP_REQUEST_POST);
        request_msg.post = *(struct golioth_coap_post_params *) request_params;
        request_msg.post.payload = request_payload;
        request_msg.post.payload_size = payload_size;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        /* NOTE: Logging a message here when cloud logging is enabled can cause
         *       a loop where the logging thread attempts to enqueue a message,
         *       the mbox is full, so coap_client writes a log, which the
         *       logging thread attempts to send to the cloud, and so on.
         */
        if (request_payload)
        {
            golioth_sys_free(request_payload);
        }
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        status = wait_for_synchronous(&request_msg, timeout_s);

        if (status == GOLIOTH_OK)
        {
            status = request_result;
        }
    }

finish:
    return status;
}

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
                                            int32_t timeout_s)
{
    struct golioth_coap_post_params params = {
        .content_type = content_type,
        .callback = callback,
        .arg = callback_arg,
    };
    return golioth_coap_client_set_internal(client,
                                            token,
                                            path_prefix,
                                            path,
                                            payload,
                                            payload_size,
                                            GOLIOTH_COAP_REQUEST_POST,
                                            &params,
                                            is_synchronous,
                                            timeout_s);
}

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
                                                  bool is_synchronous,
                                                  int32_t timeout_s)
{
    struct golioth_coap_post_block_params params = {
        .is_last = is_last,
        .content_type = content_type,
        .block_index = block_index,
        .block_szx = block_szx,
        .callback = callback,
        .arg = callback_arg,
    };
    return golioth_coap_client_set_internal(client,
                                            token,
                                            path_prefix,
                                            path,
                                            payload,
                                            payload_size,
                                            GOLIOTH_COAP_REQUEST_POST_BLOCK,
                                            &params,
                                            is_synchronous,
                                            timeout_s);
}

enum golioth_status golioth_coap_client_delete(struct golioth_client *client,
                                               const char *path_prefix,
                                               const char *path,
                                               golioth_set_cb_fn callback,
                                               void *callback_arg,
                                               bool is_synchronous,
                                               int32_t timeout_s)
{
    enum golioth_status status = GOLIOTH_OK;

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

    struct golioth_coap_request_msg request_msg = {
        .type = GOLIOTH_COAP_REQUEST_DELETE,
        .path_prefix = path_prefix,
        .delete =
            {
                .callback = callback,
                .arg = callback_arg,
            },
        .ageout_ms = ageout_ms,
    };

    if (strlen(path) > sizeof(request_msg.path) - 1)
    {
        GLTH_LOGE(TAG, "Path too long: %zu > %zu", strlen(path), sizeof(request_msg.path) - 1);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);
    enum golioth_status request_result = GOLIOTH_OK;

    if (is_synchronous)
    {
        status = setup_synchronous(&request_msg);

        if (status != GOLIOTH_OK)
        {
            goto finish;
        }

        request_msg.status = &request_result;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGW(TAG, "Failed to enqueue request, queue full");
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        status = wait_for_synchronous(&request_msg, timeout_s);

        if (status == GOLIOTH_OK)
        {
            status = request_result;
        }
    }

finish:
    return status;
}

static enum golioth_status golioth_coap_client_get_internal(
    struct golioth_client *client,
    const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
    const char *path_prefix,
    const char *path,
    enum golioth_coap_request_type type,
    void *request_params,
    bool is_synchronous,
    int32_t timeout_s)
{
    enum golioth_status status = GOLIOTH_OK;

    if (!client || !token || !path)
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

    struct golioth_coap_request_msg request_msg = {};
    request_msg.type = type;
    request_msg.path_prefix = path_prefix;

    memcpy(request_msg.token, token, GOLIOTH_COAP_TOKEN_LEN);

    if (strlen(path) > sizeof(request_msg.path) - 1)
    {
        GLTH_LOGE(TAG, "Path too long: %zu > %zu", strlen(path), sizeof(request_msg.path) - 1);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);
    enum golioth_status request_result = GOLIOTH_OK;

    if (is_synchronous)
    {
        status = setup_synchronous(&request_msg);

        if (status != GOLIOTH_OK)
        {
            goto finish;
        }

        request_msg.status = &request_result;
    }

    request_msg.ageout_ms = ageout_ms;
    if (type == GOLIOTH_COAP_REQUEST_GET_BLOCK)
    {
        request_msg.get_block = *(struct golioth_coap_get_block_params *) request_params;
    }
    else
    {
        assert(type == GOLIOTH_COAP_REQUEST_GET);
        request_msg.get = *(struct golioth_coap_get_params *) request_params;
    }

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGE(TAG, "Failed to enqueue request, queue full");
        if (is_synchronous)
        {
            golioth_event_group_destroy(request_msg.request_complete_event);
            golioth_sys_sem_destroy(request_msg.request_complete_ack_sem);
        }
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    if (is_synchronous)
    {
        status = wait_for_synchronous(&request_msg, timeout_s);

        if (status == GOLIOTH_OK)
        {
            status = request_result;
        }
    }

finish:
    return status;
}

enum golioth_status golioth_coap_client_get(struct golioth_client *client,
                                            const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                            const char *path_prefix,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            golioth_get_cb_fn callback,
                                            void *arg,
                                            bool is_synchronous,
                                            int32_t timeout_s)
{
    struct golioth_coap_get_params params = {
        .content_type = content_type,
        .callback = callback,
        .arg = arg,
    };
    return golioth_coap_client_get_internal(client,
                                            token,
                                            path_prefix,
                                            path,
                                            GOLIOTH_COAP_REQUEST_GET,
                                            &params,
                                            is_synchronous,
                                            timeout_s);
}

enum golioth_status golioth_coap_client_get_block(struct golioth_client *client,
                                                  const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
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
    struct golioth_coap_get_block_params params = {
        .content_type = content_type,
        .block_index = block_index,
        .block_size = block_size,
        .callback = callback,
        .arg = arg,
    };
    return golioth_coap_client_get_internal(client,
                                            token,
                                            path_prefix,
                                            path,
                                            GOLIOTH_COAP_REQUEST_GET_BLOCK,
                                            &params,
                                            is_synchronous,
                                            timeout_s);
}

enum golioth_status golioth_coap_client_observe(struct golioth_client *client,
                                                const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                                const char *path_prefix,
                                                const char *path,
                                                enum golioth_content_type content_type,
                                                golioth_get_cb_fn callback,
                                                void *arg)
{
    if (!client || !token || !path)
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

    struct golioth_coap_request_msg request_msg = {
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

    memcpy(request_msg.token, token, GOLIOTH_COAP_TOKEN_LEN);

    if (strlen(path) > sizeof(request_msg.path) - 1)
    {
        GLTH_LOGE(TAG, "Path too long: %zu > %zu", strlen(path), sizeof(request_msg.path) - 1);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGW(TAG, "Failed to enqueue request, queue full");
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    return GOLIOTH_OK;
}

enum golioth_status golioth_coap_client_observe_release(struct golioth_client *client,
                                                        const uint8_t token[GOLIOTH_COAP_TOKEN_LEN],
                                                        const char *path_prefix,
                                                        const char *path,
                                                        enum golioth_content_type content_type,
                                                        void *arg)
{
    if (!client || !path)
    {
        return GOLIOTH_ERR_NULL;
    }

    if (!client->is_running)
    {
        GLTH_LOGW(TAG,
                  "Client not running, dropping observe release request for path %s%s",
                  path_prefix,
                  path);
        return GOLIOTH_ERR_INVALID_STATE;
    }

    struct golioth_coap_request_msg request_msg = {
        .type = GOLIOTH_COAP_REQUEST_OBSERVE_RELEASE,
        .path_prefix = path_prefix,
        .ageout_ms = GOLIOTH_SYS_WAIT_FOREVER,
        .observe =
            {
                .content_type = content_type,
                .arg = arg,
            },
    };

    if (strlen(path) > sizeof(request_msg.path) - 1)
    {
        GLTH_LOGE(TAG, "Path too long: %zu > %zu", strlen(path), sizeof(request_msg.path) - 1);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }
    strncpy(request_msg.path, path, sizeof(request_msg.path) - 1);
    memcpy(request_msg.token, token, GOLIOTH_COAP_TOKEN_LEN);

    bool sent = golioth_mbox_try_send(client->request_queue, &request_msg);
    if (!sent)
    {
        GLTH_LOGE(TAG, "Failed to enqueue request, queue full");
        return GOLIOTH_ERR_QUEUE_FULL;
    }

    return GOLIOTH_OK;
}

void golioth_coap_client_cancel_all_observations(struct golioth_client *client)
{
    golioth_cancel_all_observations(client);
}

void golioth_coap_client_cancel_observations_by_prefix(struct golioth_client *client,
                                                       const char *prefix)
{
    golioth_cancel_all_observations_by_prefix(client, prefix);
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
