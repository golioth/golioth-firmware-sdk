/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "coap_client.h"
#include <golioth/lightdb_state.h>
#include <golioth/payload_utils.h>
#include "golioth_util.h"
#include <golioth/golioth_sys.h>

#if defined(CONFIG_GOLIOTH_LIGHTDB_STATE)

#define GOLIOTH_LIGHTDB_STATE_PATH_PREFIX ".d/"

typedef enum {
    LIGHTDB_GET_TYPE_INT,
    LIGHTDB_GET_TYPE_BOOL,
    LIGHTDB_GET_TYPE_FLOAT,
    LIGHTDB_GET_TYPE_STRING,
} lightdb_get_type_t;

typedef struct {
    lightdb_get_type_t type;
    union {
        int32_t* i;
        float* f;
        bool* b;
        char* strbuf;
    };
    size_t strbuf_size;  // only applicable for string type
    bool is_null;
} lightdb_get_response_t;

golioth_status_t golioth_lightdb_set_int_async(
        golioth_client_t client,
        const char* path,
        int32_t value,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%" PRId32, value);
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)buf,
            strlen(buf),
            callback,
            callback_arg,
            false,
            GOLIOTH_SYS_WAIT_FOREVER);
}

golioth_status_t golioth_lightdb_set_bool_async(
        golioth_client_t client,
        const char* path,
        bool value,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    const char* valuestr = (value ? "true" : "false");
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)valuestr,
            strlen(valuestr),
            callback,
            callback_arg,
            false,
            GOLIOTH_SYS_WAIT_FOREVER);
}

golioth_status_t golioth_lightdb_set_float_async(
        golioth_client_t client,
        const char* path,
        float value,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%f", value);
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)buf,
            strlen(buf),
            callback,
            callback_arg,
            false,
            GOLIOTH_SYS_WAIT_FOREVER);
}

golioth_status_t golioth_lightdb_set_string_async(
        golioth_client_t client,
        const char* path,
        const char* str,
        size_t str_len,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    // Server requires that non-JSON-formatted strings
    // be surrounded with literal ".
    size_t bufsize = str_len + 3;  // two " and a NULL
    char* buf = golioth_sys_malloc(bufsize);
    if (!buf) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }
    memset(buf, 0, bufsize);
    snprintf(buf, bufsize, "\"%s\"", str);

    golioth_status_t status = golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)buf,
            bufsize - 1,  // excluding NULL
            callback,
            callback_arg,
            false,
            GOLIOTH_SYS_WAIT_FOREVER);

    golioth_sys_free(buf);
    return status;
}

golioth_status_t golioth_lightdb_set_json_async(
        golioth_client_t client,
        const char* path,
        const char* json_str,
        size_t json_str_len,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)json_str,
            json_str_len,
            callback,
            callback_arg,
            false,
            GOLIOTH_SYS_WAIT_FOREVER);
}

golioth_status_t golioth_lightdb_get_async(
        golioth_client_t client,
        const char* path,
        golioth_get_cb_fn callback,
        void* callback_arg) {
    return golioth_coap_client_get(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            callback,
            callback_arg,
            false,
            GOLIOTH_SYS_WAIT_FOREVER);
}

golioth_status_t golioth_lightdb_delete_async(
        golioth_client_t client,
        const char* path,
        golioth_set_cb_fn callback,
        void* callback_arg) {
    return golioth_coap_client_delete(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            callback,
            callback_arg,
            false,
            GOLIOTH_SYS_WAIT_FOREVER);
}

golioth_status_t golioth_lightdb_observe_async(
        golioth_client_t client,
        const char* path,
        golioth_get_cb_fn callback,
        void* arg) {
    return golioth_coap_client_observe_async(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            callback,
            arg);
}

golioth_status_t golioth_lightdb_set_int_sync(
        golioth_client_t client,
        const char* path,
        int32_t value,
        int32_t timeout_s) {
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "%" PRId32, value);
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)buf,
            strlen(buf),
            NULL,
            NULL,
            true,
            timeout_s);
}

golioth_status_t golioth_lightdb_set_bool_sync(
        golioth_client_t client,
        const char* path,
        bool value,
        int32_t timeout_s) {
    const char* valuestr = (value ? "true" : "false");
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)valuestr,
            strlen(valuestr),
            NULL,
            NULL,
            true,
            timeout_s);
}

golioth_status_t golioth_lightdb_set_float_sync(
        golioth_client_t client,
        const char* path,
        float value,
        int32_t timeout_s) {
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%f", value);
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)buf,
            strlen(buf),
            NULL,
            NULL,
            true,
            timeout_s);
}

golioth_status_t golioth_lightdb_set_string_sync(
        golioth_client_t client,
        const char* path,
        const char* str,
        size_t str_len,
        int32_t timeout_s) {
    // Server requires that non-JSON-formatted strings
    // be surrounded with literal ".
    size_t bufsize = str_len + 3;  // two " and a NULL
    char* buf = golioth_sys_malloc(bufsize);
    if (!buf) {
        return GOLIOTH_ERR_MEM_ALLOC;
    }
    memset(buf, 0, bufsize);
    snprintf(buf, bufsize, "\"%s\"", str);

    golioth_status_t status = golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)buf,
            bufsize - 1,  // excluding NULL
            NULL,
            NULL,
            true,
            timeout_s);

    golioth_sys_free(buf);
    return status;
}

golioth_status_t golioth_lightdb_set_json_sync(
        golioth_client_t client,
        const char* path,
        const char* json_str,
        size_t json_str_len,
        int32_t timeout_s) {
    return golioth_coap_client_set(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            (const uint8_t*)json_str,
            json_str_len,
            NULL,
            NULL,
            true,
            timeout_s);
}

static void on_payload(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    lightdb_get_response_t* ldb_response = (lightdb_get_response_t*)arg;

    if (response->status != GOLIOTH_OK) {
        ldb_response->is_null = true;
        return;
    }

    if (golioth_payload_is_null(payload, payload_size)) {
        ldb_response->is_null = true;
        return;
    }

    switch (ldb_response->type) {
        case LIGHTDB_GET_TYPE_INT:
            *ldb_response->i = golioth_payload_as_int(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_FLOAT:
            *ldb_response->f = golioth_payload_as_float(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_BOOL:
            *ldb_response->b = golioth_payload_as_bool(payload, payload_size);
            break;
        case LIGHTDB_GET_TYPE_STRING: {
            // Remove the leading and trailing quote to get the raw string value
            size_t nbytes = min(ldb_response->strbuf_size - 1, payload_size - 2);
            memcpy(ldb_response->strbuf, payload + 1 /* skip quote */, nbytes);
            ldb_response->strbuf[nbytes] = 0;
        } break;
        default:
            assert(false);
    }
}

golioth_status_t golioth_lightdb_get_int_sync(
        golioth_client_t client,
        const char* path,
        int32_t* value,
        int32_t timeout_s) {
    lightdb_get_response_t response = {
            .type = LIGHTDB_GET_TYPE_INT,
            .i = value,
    };
    golioth_status_t status = golioth_coap_client_get(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            on_payload,
            &response,
            true,
            timeout_s);
    if (status == GOLIOTH_OK && response.is_null) {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

golioth_status_t golioth_lightdb_get_bool_sync(
        golioth_client_t client,
        const char* path,
        bool* value,
        int32_t timeout_s) {
    lightdb_get_response_t response = {
            .type = LIGHTDB_GET_TYPE_BOOL,
            .b = value,
    };
    golioth_status_t status = golioth_coap_client_get(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            on_payload,
            &response,
            true,
            timeout_s);
    if (status == GOLIOTH_OK && response.is_null) {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

golioth_status_t golioth_lightdb_get_float_sync(
        golioth_client_t client,
        const char* path,
        float* value,
        int32_t timeout_s) {
    lightdb_get_response_t response = {
            .type = LIGHTDB_GET_TYPE_FLOAT,
            .f = value,
    };
    golioth_status_t status = golioth_coap_client_get(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            on_payload,
            &response,
            true,
            timeout_s);
    if (status == GOLIOTH_OK && response.is_null) {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

golioth_status_t golioth_lightdb_get_string_sync(
        golioth_client_t client,
        const char* path,
        char* strbuf,
        size_t strbuf_size,
        int32_t timeout_s) {
    lightdb_get_response_t response = {
            .type = LIGHTDB_GET_TYPE_STRING,
            .strbuf = strbuf,
            .strbuf_size = strbuf_size,
    };
    golioth_status_t status = golioth_coap_client_get(
            client,
            GOLIOTH_LIGHTDB_STATE_PATH_PREFIX,
            path,
            COAP_MEDIATYPE_APPLICATION_JSON,
            on_payload,
            &response,
            true,
            timeout_s);
    if (status == GOLIOTH_OK && response.is_null) {
        return GOLIOTH_ERR_NULL;
    }
    return status;
}

golioth_status_t golioth_lightdb_get_json_sync(
        golioth_client_t client,
        const char* path,
        char* strbuf,
        size_t strbuf_size,
        int32_t timeout_s) {
    return golioth_lightdb_get_string_sync(client, path, strbuf, strbuf_size, timeout_s);
}

golioth_status_t golioth_lightdb_delete_sync(
        golioth_client_t client,
        const char* path,
        int32_t timeout_s) {
    return golioth_coap_client_delete(
            client, GOLIOTH_LIGHTDB_STATE_PATH_PREFIX, path, NULL, NULL, true, timeout_s);
}

#endif // CONFIG_GOLIOTH_LIGHTDB_STATE
