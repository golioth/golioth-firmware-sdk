/*
 * Copyright (c) 2022-2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "coap_client.h"
#include <golioth/client.h>
#include "mbox.h"
#include <golioth/golioth_sys.h>

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/tls_credentials.h>

#define GOLIOTH_COAP_MAX_NON_PAYLOAD_LEN 128

/**
 * @typedef golioth_get_next_cb_t
 *
 * @brief Callback function for requesting more data to be received.
 */
typedef int (*golioth_get_next_cb_t)(void *get_next_data, int status);

/**
 * @brief Information about response to user request
 *
 * Stores information about receive response, acknowledgment, error condition (e.g. timeout, error
 * received from server).
 *
 * If @p err is <0, then request failed with error and just @p user_data stores valid information.
 * If @p err is 0, then response was successfully received (either with data, or just an
 * acknowledgment).
 */
struct golioth_req_rsp
{
    const uint8_t *data;
    size_t len;
    size_t off;

    size_t total;

    bool is_last;

    void *user_data;

    int err;
};

/**
 * @typedef golioth_req_cb_t
 *
 * @brief User callback for handling valid response from server or error condition
 */
typedef int (*golioth_req_cb_t)(struct golioth_req_rsp *rsp);

struct golioth_tls
{
    const sec_tag_t *sec_tag_list;
    size_t sec_tag_count;
};

struct golioth_client
{
    golioth_mbox_t request_queue;
    golioth_sys_thread_t coap_thread_handle;
    struct k_sem run_sem;
    struct k_poll_event run_event;
    golioth_sys_timer_t keepalive_timer;
    bool is_running;
    bool session_connected;
    struct golioth_client_config config;
    golioth_coap_observe_info_t observations[CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS];
    // token to use for block GETs (must use same token for all blocks)
    uint8_t block_token[8];
    size_t block_token_len;

    struct golioth_tls tls;
    uint8_t *rx_buffer;
    size_t rx_buffer_len;
    size_t rx_received;

    struct coap_packet rx_packet;
    int sock;

    sys_dlist_t coap_reqs;
    bool coap_reqs_connected;
    struct k_mutex coap_reqs_lock;

    void (*on_connect)(struct golioth_client *client);
    void (*wakeup)(struct golioth_client *client);

    golioth_client_event_cb_fn event_callback;
    void *event_callback_arg;
};

int golioth_send_coap(struct golioth_client *client, struct coap_packet *packet);
