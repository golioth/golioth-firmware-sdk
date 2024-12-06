/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <string.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include "coap_client.h"
#include <golioth/config.h>
#include <golioth/rpc.h>
#include "golioth_util.h"
#include <golioth/golioth_debug.h>
#include <golioth/zcbor_utils.h>

LOG_TAG_DEFINE(golioth_rpc);

// Request:
//
// {
//      "id": "id_string",
//      "method": "method_name_string",
//      "params": [...]
// }
//
// Response:
//
// {
//      "id": "id_string",
//      "statusCode": integer,
//      "detail": {...}
// }

#if defined(CONFIG_GOLIOTH_RPC)

#define GOLIOTH_RPC_PATH_PREFIX ".rpc/"

#define QUERY_METHOD "query"

/// Private struct to contain data about a single registered method
struct golioth_rpc_method
{
    const char *method;
    golioth_rpc_cb_fn callback;
    void *callback_arg;
};

/// Private struct to contain RPC state data
struct golioth_rpc
{
    struct golioth_client *client;
    int num_rpcs;
    struct golioth_rpc_method rpcs[CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS];
};

static int params_decode(zcbor_state_t *zsd, void *value)
{
    zcbor_state_t *params_zsd = value;
    bool ok;

    ok = zcbor_list_start_decode(zsd);
    if (!ok)
    {
        GLTH_LOGW(TAG, "Did not start CBOR list correctly");
        return -EBADMSG;
    }

    memcpy(params_zsd, zsd, sizeof(*params_zsd));

    while (!zcbor_list_or_map_end(zsd))
    {
        ok = zcbor_any_skip(zsd, NULL);
        if (!ok)
        {
            GLTH_LOGW(TAG, "Failed to skip param (%d)", (int) zcbor_peek_error(zsd));
            return -EBADMSG;
        }
    }

    ok = zcbor_list_end_decode(zsd);
    if (!ok)
    {
        GLTH_LOGW(TAG, "Did not end CBOR list correctly");
        return -EBADMSG;
    }

    return 0;
}

static void on_rpc(struct golioth_client *client,
                   enum golioth_status status,
                   const struct golioth_coap_rsp_code *coap_rsp_code,
                   const char *path,
                   const uint8_t *payload,
                   size_t payload_size,
                   void *arg)
{
    ZCBOR_STATE_D(zsd, 2, payload, payload_size, 1, 0);
    zcbor_state_t params_zsd;
    struct zcbor_string id, method;
    struct zcbor_map_entry map_entries[] = {
        ZCBOR_TSTR_LIT_MAP_ENTRY("id", zcbor_map_tstr_decode, &id),
        ZCBOR_TSTR_LIT_MAP_ENTRY("method", zcbor_map_tstr_decode, &method),
        ZCBOR_TSTR_LIT_MAP_ENTRY("params", params_decode, &params_zsd),
    };
    int err;
    bool ok;

    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Error response on observed RPC: %d", status);
        return;
    }

    GLTH_LOG_BUFFER_HEXDUMP(TAG, payload, min(64, payload_size), GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    if (payload_size == 3 && payload[1] == 'O' && payload[2] == 'K')
    {
        GLTH_LOGI(TAG, "RPC observation established");
        /* Ignore "OK" response received after observing */
        return;
    }

    /* Decode request */
    err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
    if (err)
    {
        GLTH_LOGE(TAG, "Failed to parse tstr map");
        return;
    }

    /* Start encoding response */
    static uint8_t response_buf[CONFIG_GOLIOTH_RPC_MAX_RESPONSE_LEN];
    ZCBOR_STATE_E(zse, 2, response_buf, sizeof(response_buf), 1);

    ok = zcbor_map_start_encode(zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode RPC response map");
        return;
    }

    ok = zcbor_tstr_put_lit(zse, "id") && zcbor_tstr_encode(zse, &id);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode RPC '%s'", "id");
        return;
    }

    struct golioth_rpc *grpc = arg;

    const struct golioth_rpc_method *matching_rpc = NULL;
    enum golioth_rpc_status rpc_status = GOLIOTH_RPC_UNKNOWN;

    for (int i = 0; i < grpc->num_rpcs; i++)
    {
        const struct golioth_rpc_method *rpc = &grpc->rpcs[i];
        GLTH_LOGW(TAG, "RPC: %s", rpc->method);
        if (strlen(rpc->method) == method.len
            && strncmp(rpc->method, (char *) method.value, method.len) == 0)
        {
            matching_rpc = rpc;
        }
    }

    if (matching_rpc)
    {
        GLTH_LOGD(TAG, "Calling registered RPC method: %s", matching_rpc->method);

        /**
         * Call callback while decode context is inside the params array
         * and encode context is inside the detail map.
         */
        ok = zcbor_tstr_put_lit(zse, "detail");
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to encode RPC '%s'", "detail");
            return;
        }

        ok = zcbor_map_start_encode(zse, SIZE_MAX);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Did not start CBOR map correctly");
            return;
        }

        if (strncmp(matching_rpc->method, QUERY_METHOD, strlen(QUERY_METHOD)) == 0)
        {
            GLTH_LOGW(TAG, "Found query");

            for (int i = 0; i < grpc->num_rpcs; i++)
            {
                const struct golioth_rpc_method *rpc = &grpc->rpcs[i];
                ok = zcbor_tstr_encode_ptr(zse, rpc->method, strlen(rpc->method))
                    && zcbor_map_start_encode(zse, SIZE_MAX)
                    /* TODO: Develop a way to add parameter information */
                    /*&& zcbor_tstr_put_lit(zse, "int_a")*/
                    /*&& zcbor_uint64_put(zse, 2)*/
                    /*&& zcbor_tstr_put_lit(zse, "int_b")*/
                    /*&& zcbor_uint64_put(zse, 2)*/
                    && zcbor_map_end_encode(zse, SIZE_MAX);

                if (!ok)
                {
                    GLTH_LOGE(TAG, "Did not encode CBOR method entry");
                    return;
                }
            }

            rpc_status = GOLIOTH_RPC_OK;
        }
        else
        {
            rpc_status = matching_rpc->callback(&params_zsd, zse, matching_rpc->callback_arg);
        }

        GLTH_LOGD(TAG, "RPC status code %d for call id :%.*s", rpc_status, (int) id.len, id.value);

        ok = zcbor_map_end_encode(zse, SIZE_MAX);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to close '%s'", "detail");
            return;
        }
    }
    else
    {
        rpc_status = GOLIOTH_RPC_NOT_FOUND;
        GLTH_LOGW(TAG, "Method %.*s not registered", (int) method.len, method.value);
    }

    ok = zcbor_tstr_put_lit(zse, "statusCode") && zcbor_uint64_put(zse, rpc_status);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode RPC '%s'", "statusCode");
        return;
    }

    /* root response map */
    ok = zcbor_map_end_encode(zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to close '%s'", "root");
        return;
    }

    golioth_coap_client_set(client,
                            GOLIOTH_RPC_PATH_PREFIX,
                            "status",
                            GOLIOTH_CONTENT_TYPE_CBOR,
                            response_buf,
                            zse->payload - response_buf,
                            NULL,
                            NULL,
                            false,
                            GOLIOTH_SYS_WAIT_FOREVER);
}

struct golioth_rpc *golioth_rpc_init(struct golioth_client *client)
{
    struct golioth_rpc *grpc = golioth_sys_malloc(sizeof(struct golioth_rpc));

    if (grpc != NULL)
    {
        grpc->client = client;
        grpc->num_rpcs = 0;
    }

    return grpc;
}

enum golioth_status golioth_rpc_deinit(struct golioth_rpc *grpc)
{

    if (grpc == NULL)
    {
        GLTH_LOGE(TAG, "RPC service handle must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    golioth_coap_client_cancel_observations_by_prefix(grpc->client, GOLIOTH_RPC_PATH_PREFIX);
    free(grpc);
    return GOLIOTH_OK;
}

enum golioth_status golioth_rpc_register(struct golioth_rpc *grpc,
                                         const char *method,
                                         golioth_rpc_cb_fn callback,
                                         void *callback_arg)
{
    if (grpc->num_rpcs >= CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS)
    {
        GLTH_LOGE(TAG,
                  "Unable to register, can't register more than %d methods",
                  CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS);
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    struct golioth_rpc_method *rpc = &grpc->rpcs[grpc->num_rpcs];

    rpc->method = method;
    rpc->callback = callback;
    rpc->callback_arg = callback_arg;

    grpc->num_rpcs++;
    if (grpc->num_rpcs == 1)
    {
        return golioth_coap_client_observe(grpc->client,
                                           GOLIOTH_RPC_PATH_PREFIX,
                                           "",
                                           GOLIOTH_CONTENT_TYPE_CBOR,
                                           on_rpc,
                                           grpc);
    }
    return GOLIOTH_OK;
}

#endif  // CONFIG_GOLIOTH_RPC
