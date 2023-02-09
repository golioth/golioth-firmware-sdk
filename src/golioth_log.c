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

LOG_TAG_DEFINE(golioth_log);

// Important Note!
//
// Do not use GLTH_LOGX statements in this file, as it can cause an infinite
// recursion with golioth_debug_printf().
//
// If you must log, use printf instead.

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

    cJSON* json = cJSON_CreateObject();
    char* printed_str = NULL;
    golioth_status_t status = GOLIOTH_OK;

    GSTATS_INC_ALLOC("json");
    cJSON_AddStringToObject(json, "level", _level_to_str[level]);
    cJSON_AddStringToObject(json, "module", tag);
    cJSON_AddStringToObject(json, "msg", log_message);
    printed_str = cJSON_PrintUnformatted(json);
    if (!printed_str) {
        status = GOLIOTH_ERR_SERIALIZE;
        goto cleanup;
    }
    GSTATS_INC_ALLOC("printed_str");

    status = golioth_coap_client_set(
            client,
            "",  // path-prefix unused
            "logs",
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)printed_str,
            strlen(printed_str),
            callback,
            callback_arg,
            is_synchronous,
            timeout_s);

cleanup:
    if (json) {
        cJSON_Delete(json);
        GSTATS_INC_FREE("json");
    }
    if (printed_str) {
        free(printed_str);
        GSTATS_INC_FREE("printed_str");
    }
    return status;
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
