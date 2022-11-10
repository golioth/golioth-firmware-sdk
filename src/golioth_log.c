/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cJSON.h>
#include "golioth_coap_client.h"
#include "golioth_log.h"
#include "golioth_time.h"
#include "golioth_statistics.h"
#include "golioth_debug.h"

#define TAG "golioth_log"

#define CONFIG_GOLIOTH_LOG_MAX_MESSAGE_LEN 100

typedef enum {
    GOLIOTH_LOG_LEVEL_ERROR,
    GOLIOTH_LOG_LEVEL_WARN,
    GOLIOTH_LOG_LEVEL_INFO,
    GOLIOTH_LOG_LEVEL_DEBUG
} golioth_log_level_t;

static const char* _level_to_str[GOLIOTH_LOG_LEVEL_DEBUG + 1] = {
        [GOLIOTH_LOG_LEVEL_ERROR] = "error",
        [GOLIOTH_LOG_LEVEL_WARN] = "warn",
        [GOLIOTH_LOG_LEVEL_INFO] = "info",
        [GOLIOTH_LOG_LEVEL_DEBUG] = "debug"};

static golioth_status_t golioth_log_internal(
        golioth_client_t client,
        golioth_log_level_t level,
        const char* tag,
        const char* log_message,
        bool is_synchronous,
        int32_t timeout_s,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    assert(level <= GOLIOTH_LOG_LEVEL_DEBUG);

    char logbuf[CONFIG_GOLIOTH_LOG_MAX_MESSAGE_LEN + 5] = {};
    cJSON* json = cJSON_CreateObject();
    GSTATS_INC_ALLOC("json");
    cJSON_AddStringToObject(json, "level", _level_to_str[level]);
    cJSON_AddStringToObject(json, "module", tag);
    cJSON_AddStringToObject(json, "msg", log_message);
    bool printed = cJSON_PrintPreallocated(json, logbuf, CONFIG_GOLIOTH_LOG_MAX_MESSAGE_LEN, false);
    cJSON_Delete(json);
    GSTATS_INC_FREE("json");

    if (!printed) {
        return GOLIOTH_ERR_SERIALIZE;
    }

    size_t msg_len = strnlen(logbuf, CONFIG_GOLIOTH_LOG_MAX_MESSAGE_LEN);
    return golioth_coap_client_set(
            client,
            "",  // path-prefix unused
            "logs",
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)logbuf,
            msg_len,
            callback,
            callback_arg,
            is_synchronous,
            timeout_s);
}

golioth_status_t golioth_log_error_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    return golioth_log_internal(
            client,
            GOLIOTH_LOG_LEVEL_ERROR,
            tag,
            log_message,
            false,
            GOLIOTH_WAIT_FOREVER,
            callback,
            callback_arg);
}

golioth_status_t golioth_log_warn_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    return golioth_log_internal(
            client,
            GOLIOTH_LOG_LEVEL_WARN,
            tag,
            log_message,
            false,
            GOLIOTH_WAIT_FOREVER,
            callback,
            callback_arg);
}

golioth_status_t golioth_log_info_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    return golioth_log_internal(
            client,
            GOLIOTH_LOG_LEVEL_INFO,
            tag,
            log_message,
            false,
            GOLIOTH_WAIT_FOREVER,
            callback,
            callback_arg);
}

golioth_status_t golioth_log_debug_async(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    return golioth_log_internal(
            client,
            GOLIOTH_LOG_LEVEL_DEBUG,
            tag,
            log_message,
            false,
            GOLIOTH_WAIT_FOREVER,
            callback,
            callback_arg);
}

golioth_status_t golioth_log_error_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s) {
    return golioth_log_internal(
            client, GOLIOTH_LOG_LEVEL_ERROR, tag, log_message, true, timeout_s, NULL, NULL);
}

golioth_status_t golioth_log_warn_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s) {
    return golioth_log_internal(
            client, GOLIOTH_LOG_LEVEL_WARN, tag, log_message, true, timeout_s, NULL, NULL);
}

golioth_status_t golioth_log_info_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s) {
    return golioth_log_internal(
            client, GOLIOTH_LOG_LEVEL_INFO, tag, log_message, true, timeout_s, NULL, NULL);
}

golioth_status_t golioth_log_debug_sync(
        golioth_client_t client,
        const char* tag,
        const char* log_message,
        int32_t timeout_s) {
    return golioth_log_internal(
            client, GOLIOTH_LOG_LEVEL_DEBUG, tag, log_message, true, timeout_s, NULL, NULL);
}
