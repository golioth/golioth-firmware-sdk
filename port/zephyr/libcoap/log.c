/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(libcoap);

#include <coap3/coap.h>

static void libcoap_log_handler(coap_log_t level, const char* msg) {
    size_t msg_len = strlen(msg);

    if (msg[msg_len - 1] == '\n') {
        msg_len--;
    }

    if (level <= COAP_LOG_ERR) {
        LOG_ERR("%.*s", msg_len, msg);
    } else if (level <= COAP_LOG_WARN) {
        LOG_WRN("%.*s", msg_len, msg);
    } else if (level <= COAP_LOG_INFO) {
        LOG_INF("%.*s", msg_len, msg);
    } else {
        LOG_DBG("%.*s", msg_len, msg);
    }
}

static int libcoap_init(void) {
    coap_set_log_handler(libcoap_log_handler);
    coap_set_log_level(COAP_LOG_INFO);

    return 0;
}

SYS_INIT(libcoap_init, POST_KERNEL, 10);
