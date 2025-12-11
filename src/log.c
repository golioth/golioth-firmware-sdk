/*
 * Copyright (c) 2022-2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <zcbor_encode.h>
#include "coap_client.h"
#include <golioth/log.h>
#include <golioth/golioth_debug.h>
#include <golioth/zcbor_utils.h>

LOG_TAG_DEFINE(golioth_log);

#define CBOR_LOG_MAX_LEN 1024

#if defined(CONFIG_GOLIOTH_LOG_LEGACY)
#define GOLIOTH_LOG_PATH_PREFIX ""
#define GOLIOTH_LOG_PATH "logs"
#else
#define GOLIOTH_LOG_PATH_PREFIX ".s/"
#define GOLIOTH_LOG_PATH CONFIG_GOLIOTH_LOG_PIPELINES_PATH
#endif  // CONFIG_GOLIOTH_LOG_LEGACY

// Important Note!
//
// Do not use GLTH_LOGX statements in this file, as it can cause an infinite
// recursion with golioth_debug_printf().
//
// If you must log, use printf instead.

typedef enum
{
    GOLIOTH_LOG_LEVEL_ERROR,
    GOLIOTH_LOG_LEVEL_WARN,
    GOLIOTH_LOG_LEVEL_INFO,
    GOLIOTH_LOG_LEVEL_DEBUG
} golioth_log_level_t;

static const char *_level_to_str[GOLIOTH_LOG_LEVEL_DEBUG + 1] = {
    [GOLIOTH_LOG_LEVEL_ERROR] = "error",
    [GOLIOTH_LOG_LEVEL_WARN] = "warn",
    [GOLIOTH_LOG_LEVEL_INFO] = "info",
    [GOLIOTH_LOG_LEVEL_DEBUG] = "debug"};

static enum golioth_status golioth_log_internal(struct golioth_client *client,
                                                golioth_log_level_t level,
                                                const char *tag,
                                                const char *log_message,
                                                bool is_synchronous,
                                                int32_t timeout_s,
                                                golioth_set_cb_fn callback,
                                                void *callback_arg)
{
    assert(level <= GOLIOTH_LOG_LEVEL_DEBUG);

    uint8_t *cbor_buf = malloc(CBOR_LOG_MAX_LEN);
    enum golioth_status status = GOLIOTH_ERR_SERIALIZE;
    bool ok;

    if (!cbor_buf)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ZCBOR_STATE_E(zse, 1, cbor_buf, CBOR_LOG_MAX_LEN, 1);

    ok = zcbor_map_start_encode(zse, 3);
    if (!ok)
    {
        goto cleanup;
    }

    ok = zcbor_tstr_put_lit(zse, "level") && zcbor_tstr_put_term(zse, _level_to_str[level], 5)
        && zcbor_tstr_put_lit(zse, "module") && zcbor_tstr_put_term(zse, tag, SIZE_MAX)
        && zcbor_tstr_put_lit(zse, "msg") && zcbor_tstr_put_term(zse, log_message, SIZE_MAX);
    if (!ok)
    {
        goto cleanup;
    }

    ok = zcbor_map_end_encode(zse, 3);
    if (!ok)
    {
        goto cleanup;
    }

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    status = golioth_coap_client_set(client,
                                     token,
                                     GOLIOTH_LOG_PATH_PREFIX,
                                     GOLIOTH_LOG_PATH,
                                     GOLIOTH_CONTENT_TYPE_CBOR,
                                     cbor_buf,
                                     zse->payload - cbor_buf,
                                     callback,
                                     callback_arg,
                                     is_synchronous,
                                     timeout_s);

cleanup:
    free(cbor_buf);
    return status;
}

enum golioth_status golioth_log_error_async(struct golioth_client *client,
                                            const char *tag,
                                            const char *log_message,
                                            golioth_set_cb_fn callback,
                                            void *callback_arg)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_ERROR,
                                tag,
                                log_message,
                                false,
                                GOLIOTH_SYS_WAIT_FOREVER,
                                callback,
                                callback_arg);
}

enum golioth_status golioth_log_warn_async(struct golioth_client *client,
                                           const char *tag,
                                           const char *log_message,
                                           golioth_set_cb_fn callback,
                                           void *callback_arg)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_WARN,
                                tag,
                                log_message,
                                false,
                                GOLIOTH_SYS_WAIT_FOREVER,
                                callback,
                                callback_arg);
}

enum golioth_status golioth_log_info_async(struct golioth_client *client,
                                           const char *tag,
                                           const char *log_message,
                                           golioth_set_cb_fn callback,
                                           void *callback_arg)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_INFO,
                                tag,
                                log_message,
                                false,
                                GOLIOTH_SYS_WAIT_FOREVER,
                                callback,
                                callback_arg);
}

enum golioth_status golioth_log_debug_async(struct golioth_client *client,
                                            const char *tag,
                                            const char *log_message,
                                            golioth_set_cb_fn callback,
                                            void *callback_arg)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_DEBUG,
                                tag,
                                log_message,
                                false,
                                GOLIOTH_SYS_WAIT_FOREVER,
                                callback,
                                callback_arg);
}

enum golioth_status golioth_log_error_sync(struct golioth_client *client,
                                           const char *tag,
                                           const char *log_message,
                                           int32_t timeout_s)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_ERROR,
                                tag,
                                log_message,
                                true,
                                timeout_s,
                                NULL,
                                NULL);
}

enum golioth_status golioth_log_warn_sync(struct golioth_client *client,
                                          const char *tag,
                                          const char *log_message,
                                          int32_t timeout_s)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_WARN,
                                tag,
                                log_message,
                                true,
                                timeout_s,
                                NULL,
                                NULL);
}

enum golioth_status golioth_log_info_sync(struct golioth_client *client,
                                          const char *tag,
                                          const char *log_message,
                                          int32_t timeout_s)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_INFO,
                                tag,
                                log_message,
                                true,
                                timeout_s,
                                NULL,
                                NULL);
}

enum golioth_status golioth_log_debug_sync(struct golioth_client *client,
                                           const char *tag,
                                           const char *log_message,
                                           int32_t timeout_s)
{
    return golioth_log_internal(client,
                                GOLIOTH_LOG_LEVEL_DEBUG,
                                tag,
                                log_message,
                                true,
                                timeout_s,
                                NULL,
                                NULL);
}
