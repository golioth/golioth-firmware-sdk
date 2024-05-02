#pragma once

#include "coap_client.h"
#include "mbox.h"

struct golioth_client
{
    golioth_mbox_t request_queue;
    golioth_sys_thread_t coap_thread_handle;
    golioth_sys_sem_t run_sem;
    golioth_sys_timer_t keepalive_timer;
    bool is_running;
    bool end_session;
    bool session_connected;
    struct golioth_client_config config;
    golioth_coap_request_msg_t *pending_req;
    golioth_coap_observe_info_t observations[CONFIG_GOLIOTH_MAX_NUM_OBSERVATIONS];
    // token to use for block GETs (must use same token for all blocks)
    uint8_t block_token[8];
    size_t block_token_len;
    golioth_client_event_cb_fn event_callback;
    void *event_callback_arg;
};

void golioth_cancel_all_observations_by_prefix(struct golioth_client *client, const char *prefix);

void golioth_cancel_all_observations(struct golioth_client *client);
