/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <string.h>
#include "coap_client.h"
#include <golioth/lightdb_state.h>
#include <golioth/payload_utils.h>
#include "golioth_util.h"
#include <golioth/golioth_sys.h>

#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE)

LOG_TAG_DEFINE(lightdb_state);

#define GOLIOTH_LIGHTDB_STATE_PATH_PREFIX ".d/"

typedef enum
{
    LIGHTDB_GET_TYPE_INT,
    LIGHTDB_GET_TYPE_BOOL,
    LIGHTDB_GET_TYPE_FLOAT,
    LIGHTDB_GET_TYPE_STRING,
    LIGHTDB_GET_TYPE_BINARY,
} lightdb_get_type_t;

typedef struct
{
    lightdb_get_type_t type;
    union
    {
        int32_t *i;
        float *f;
        bool *b;
        uint8_t *buf;
    };
    size_t buf_size;  // only applicable for string & binary types
    bool is_null;
} lightdb_get_response_t;

enum golioth_status golioth_lightdb_set_int_async(struct golioth_client *client,
                                                  const char *path,
                                                  int32_t value,
                                                  golioth_set_cb_fn callback,
                                                  void *callback_arg)
{
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%" PRId32, value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_set_bool_async(struct golioth_client *client,
                                                   const char *path,
                                                   bool value,
                                                   golioth_set_cb_fn callback,
                                                   void *callback_arg)
{
    const char *valuestr = (value ? "true" : "false");
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) valuestr,
                                   strlen(valuestr),
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS)

enum golioth_status golioth_lightdb_set_float_async(struct golioth_client *client,
                                                    const char *path,
                                                    float value,
                                                    golioth_set_cb_fn callback,
                                                    void *callback_arg)
{
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%f", (double) value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

#endif  // CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS

enum golioth_status golioth_lightdb_set_string_async(struct golioth_client *client,
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

    enum golioth_status status = golioth_coap_client_set(client,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         (const uint8_t *) buf,
                                                         bufsize - 1,  // excluding NULL
                                                         callback,
                                                         callback_arg,
                                                         false,
                                                         GOLIOTH_SYS_WAIT_FOREVER);

    golioth_sys_free(buf);
    return status;
}

enum golioth_status golioth_lightdb_set_async(struct golioth_client *client,
                                              const char *path,
                                              enum golioth_content_type content_type,
                                              const uint8_t *buf,
                                              size_t buf_len,
                                              golioth_set_cb_fn callback,
                                              void *callback_arg)
{
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   content_type,
                                   buf,
                                   buf_len,
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_get_async(struct golioth_client *client,
                                              const char *path,
                                              enum golioth_content_type content_type,
                                              golioth_get_cb_fn callback,
                                              void *callback_arg)
{
    return golioth_coap_client_get(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   content_type,
                                   callback,
                                   callback_arg,
                                   false,
                                   GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_delete_async(struct golioth_client *client,
                                                 const char *path,
                                                 golioth_set_cb_fn callback,
                                                 void *callback_arg)
{
    return golioth_coap_client_delete(client,
                                      GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                      path,
                                      callback,
                                      callback_arg,
                                      false,
                                      GOLIOTH_SYS_WAIT_FOREVER);
}

enum golioth_status golioth_lightdb_observe_async(struct golioth_client *client,
                                                  const char *path,
                                                  enum golioth_content_type content_type,
                                                  golioth_get_cb_fn callback,
                                                  void *arg)
{
    return golioth_coap_client_observe(client,
                                       GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                       path,
                                       content_type,
                                       callback,
                                       arg);
}

enum golioth_status golioth_lightdb_set_int_sync(struct golioth_client *client,
                                                 const char *path,
                                                 int32_t value,
                                                 int32_t timeout_s)
{
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%" PRId32, value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

enum golioth_status golioth_lightdb_set_bool_sync(struct golioth_client *client,
                                                  const char *path,
                                                  bool value,
                                                  int32_t timeout_s)
{
    const char *valuestr = (value ? "true" : "false");
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) valuestr,
                                   strlen(valuestr),
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS)

enum golioth_status golioth_lightdb_set_float_sync(struct golioth_client *client,
                                                   const char *path,
                                                   float value,
                                                   int32_t timeout_s)
{
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%f", (double) value);
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   GOLIOTH_CONTENT_TYPE_JSON,
                                   (const uint8_t *) buf,
                                   strlen(buf),
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

#endif  // CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS

enum golioth_status golioth_lightdb_set_string_sync(struct golioth_client *client,
                                                    const char *path,
                                                    const char *str,
                                                    size_t str_len,
                                                    int32_t timeout_s)
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

    enum golioth_status status = golioth_coap_client_set(client,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         (const uint8_t *) buf,
                                                         bufsize - 1,  // excluding NULL
                                                         NULL,
                                                         NULL,
                                                         true,
                                                         timeout_s);

    golioth_sys_free(buf);
    return status;
}

enum golioth_status golioth_lightdb_set_sync(struct golioth_client *client,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             const uint8_t *buf,
                                             size_t buf_len,
                                             int32_t timeout_s)
{
    return golioth_coap_client_set(client,
                                   GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                   path,
                                   content_type,
                                   buf,
                                   buf_len,
                                   NULL,
                                   NULL,
                                   true,
                                   timeout_s);
}

static void on_payload(struct golioth_client *client,
                       const struct golioth_response *response,
                       const char *path,
                       const uint8_t *payload,
                       size_t payload_size,
                       void *arg)
{
    lightdb_get_response_t *ldb_response = (lightdb_get_response_t *) arg;

    if (response->status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Error response from LightDB State: %d", response->status);
        ldb_response->is_null = true;
        return;
    }

    if (golioth_payload_is_null(payload, payload_size))
    {
        ldb_response->is_null = true;
        return;
    }

    switch (ldb_response->type)
    {
        case LIGHTDB_GET_TYPE_INT:
            *ldb_response->i = golioth_payload_as_int(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_FLOAT:
#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS)
            *ldb_response->f = golioth_payload_as_float(payload, payload_size);
#else
            GLTH_LOGE(TAG, "Float support disabled");
#endif  // CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS
            break;
        case LIGHTDB_GET_TYPE_BOOL:
            *ldb_response->b = golioth_payload_as_bool(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_STRING:
        {
            // Remove the leading and trailing quote to get the raw string value
            size_t nbytes = min(ldb_response->buf_size - 1, payload_size - 2);
            memcpy(ldb_response->buf, payload + 1 /* skip quote */, nbytes);
            ldb_response->buf[nbytes] = 0;
        }
        break;
        case LIGHTDB_GET_TYPE_BINARY:
            memcpy(ldb_response->buf, payload, min(ldb_response->buf_size, payload_size));
            ldb_response->buf_size = payload_size;
            break;
        default:
            assert(false);
    }
}

enum golioth_status golioth_lightdb_get_int_sync(struct golioth_client *client,
                                                 const char *path,
                                                 int32_t *value,
                                                 int32_t timeout_s)
{
    lightdb_get_response_t response = {
        .type = LIGHTDB_GET_TYPE_INT,
        .i = value,
    };
    enum golioth_status status = golioth_coap_client_get(client,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         on_payload,
                                                         &response,
                                                         true,
                                                         timeout_s);
    if (status == GOLIOTH_OK && response.is_null)
    {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

enum golioth_status golioth_lightdb_get_bool_sync(struct golioth_client *client,
                                                  const char *path,
                                                  bool *value,
                                                  int32_t timeout_s)
{
    lightdb_get_response_t response = {
        .type = LIGHTDB_GET_TYPE_BOOL,
        .b = value,
    };
    enum golioth_status status = golioth_coap_client_get(client,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         on_payload,
                                                         &response,
                                                         true,
                                                         timeout_s);
    if (status == GOLIOTH_OK && response.is_null)
    {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS)

enum golioth_status golioth_lightdb_get_float_sync(struct golioth_client *client,
                                                   const char *path,
                                                   float *value,
                                                   int32_t timeout_s)
{
    lightdb_get_response_t response = {
        .type = LIGHTDB_GET_TYPE_FLOAT,
        .f = value,
    };
    enum golioth_status status = golioth_coap_client_get(client,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         on_payload,
                                                         &response,
                                                         true,
                                                         timeout_s);
    if (status == GOLIOTH_OK && response.is_null)
    {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

#endif  // CONFIG_GOLIOTH_LIGHTDB_STATE_FLOAT_HELPERS

enum golioth_status golioth_lightdb_get_string_sync(struct golioth_client *client,
                                                    const char *path,
                                                    char *strbuf,
                                                    size_t strbuf_size,
                                                    int32_t timeout_s)
{
    lightdb_get_response_t response = {
        .type = LIGHTDB_GET_TYPE_STRING,
        .buf = (uint8_t *) strbuf,
        .buf_size = strbuf_size,
    };
    enum golioth_status status = golioth_coap_client_get(client,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         GOLIOTH_CONTENT_TYPE_JSON,
                                                         on_payload,
                                                         &response,
                                                         true,
                                                         timeout_s);
    if (status == GOLIOTH_OK && response.is_null)
    {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

enum golioth_status golioth_lightdb_get_sync(struct golioth_client *client,
                                             const char *path,
                                             enum golioth_content_type content_type,
                                             uint8_t *buf,
                                             size_t *buf_size,
                                             int32_t timeout_s)
{
    lightdb_get_response_t response = {
        .type = LIGHTDB_GET_TYPE_BINARY,
        .buf = buf,
        .buf_size = *buf_size,
    };
    enum golioth_status status = golioth_coap_client_get(client,
                                                         GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                                         path,
                                                         content_type,
                                                         on_payload,
                                                         &response,
                                                         true,
                                                         timeout_s);
    *buf_size = response.buf_size;
    if (status == GOLIOTH_OK && response.is_null)
    {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

enum golioth_status golioth_lightdb_delete_sync(struct golioth_client *client,
                                                const char *path,
                                                int32_t timeout_s)
{
    return golioth_coap_client_delete(client,
                                      GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
                                      path,
                                      NULL,
                                      NULL,
                                      true,
                                      timeout_s);
}

#endif  // CONFIG_GOLIOTH_LIGHTDB_STATE
