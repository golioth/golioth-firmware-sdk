/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "coap_client.h"
#include <golioth/lightdb_state.h>
// #include <golioth/payload_utils.h>
#include "golioth_util.h"
#include <golioth/golioth_sys.h>

#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE)

LOG_TAG_DEFINE(lightdb_state);

#define GOLIOTH_LIGHTDB_STATE_PATH_PREFIX ".d/"

enum golioth_status golioth_lightdb_set_int(struct golioth_client *client,
                                            const char *path,
                                            int32_t value,
                                            golioth_set_cb_fn callback,
                                            void *callback_arg)
{
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%" PRId32, value);

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_set(client,
                                   token,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   callback,
                                   callback_arg,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_set_bool(struct golioth_client *client,
                                             const char *path,
                                             bool value,
                                             golioth_set_cb_fn callback,
                                             void *callback_arg)
{
    const char *valuestr = (value ? "true" : "false");

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_set(client,
                                   token,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) valuestr,
                                   strlen(valuestr),
                                   callback,
                                   callback_arg,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS)

enum golioth_status golioth_lightdb_set_float(struct golioth_client *client,
                                              const char *path,
                                              float value,
                                              golioth_set_cb_fn callback,
                                              void *callback_arg)
{
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%f", (double) value);

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_set(client,
                                   token,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   callback,
                                   callback_arg,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

#endif  // CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS

enum golioth_status golioth_lightdb_set_string(struct golioth_client *client,
                                               const char *path,
                                               const char *str,
                                               size_t str_len,
                                               golioth_set_cb_fn callback,
                                               void *callback_arg)
{
    // Server requires that non-JSON-formatted strings
    // be surrounded with literal ".
    size_t bufsize = str_len + 3;  // two " and a NULL
    char *buf = golioth_sys_malloc(bufsize);
    if (!buf)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }
    memset(buf, 0, bufsize);
    snprintf(buf, bufsize, "\"%s\"", str);

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    enum golioth_status status = golioth_coap_client_set(client,
                                                         token,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         (const uint8_t *) buf,
                                                         bufsize - 1,  // excluding NULL
                                                         callback,
                                                         callback_arg,
                                                         GOLIOTH_SYS_WAIT_FOREVER);

    golioth_sys_free(buf);
    return status;
}

enum golioth_status golioth_lightdb_set(struct golioth_client *client,
                                        const char *path,
                                        enum golioth_content_type content_type,
                                        const uint8_t *buf,
                                        size_t buf_len,
                                        golioth_set_cb_fn callback,
                                        void *callback_arg)
{
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_set(client,
                                   token,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   content_type,
                                   buf,
                                   buf_len,
                                   callback,
                                   callback_arg,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_get(struct golioth_client *client,
                                        const char *path,
                                        enum golioth_content_type content_type,
                                        golioth_get_cb_fn callback,
                                        void *callback_arg)
{
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_get(client,
                                   token,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   content_type,
                                   callback,
                                   callback_arg,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_delete(struct golioth_client *client,
                                           const char *path,
                                           golioth_set_cb_fn callback,
                                           void *callback_arg)
{
    return golioth_coap_client_delete(client,
                                      GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                      path,
                                      callback,
                                      callback_arg,
                                      GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_observe(struct golioth_client *client,
                                            const char *path,
                                            enum golioth_content_type content_type,
                                            golioth_get_cb_fn callback,
                                            void *arg)
{
    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    return golioth_coap_client_observe(client,
                                       token,
                                       GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                       path,
                                       content_type,
                                       callback,
                                       arg);
}

#endif  // CONFIG_GOLIOTH_LIGHTDB_STATE
