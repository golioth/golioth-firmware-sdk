/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rpc_sample, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/rpc.h>
#include <samples/common/sample_credentials.h>
#include <samples/common/net_connect.h>
#include <string.h>
#include <zephyr/kernel.h>

static K_SEM_DEFINE(connected, 0, 1);

#define RPC_RETURN_STRING_BUF_SIZE 128

static enum golioth_rpc_status on_multiply(zcbor_state_t *request_params_array,
                                           zcbor_state_t *response_detail_map,
                                           void *callback_arg)
{
    double a, b;
    double value;
    bool ok;

    ok = zcbor_float_decode(request_params_array, &a)
        && zcbor_float_decode(request_params_array, &b);
    if (!ok)
    {
        LOG_ERR("Failed to decode array items");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    value = a * b;

    LOG_DBG("%lf * %lf = %lf", a, b, value);

    ok = zcbor_tstr_put_lit(response_detail_map, "value")
        && zcbor_float64_put(response_detail_map, value);
    if (!ok)
    {
        LOG_ERR("Failed to encode value");
        return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
    }

    return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_repeat_word(zcbor_state_t *request_params_array,
                                        zcbor_state_t *response_detail_map,
                                        void *callback_arg)
{
    struct zcbor_string zstr;
    double multiple_f;
    bool ok;

    ok = zcbor_tstr_decode(request_params_array, &zstr)
        && zcbor_float64_decode(request_params_array, &multiple_f);

    if (!ok)
    {
        LOG_ERR("Could not decode params");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    char return_str[RPC_RETURN_STRING_BUF_SIZE] = { 0 };
    uint32_t multiple = (uint32_t) multiple_f;
    size_t seed_len = zstr.len + 1; /* Add one for space */

    if (multiple * seed_len > RPC_RETURN_STRING_BUF_SIZE)
    {
        LOG_ERR("Input params too large to return");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < multiple; i++)
    {
        int offset = seed_len * i;
        int remaining = RPC_RETURN_STRING_BUF_SIZE - offset;
        if (remaining >= seed_len)
        {
            snprintk(return_str + offset, seed_len + 1, "%.*s ", zstr.len, zstr.value);
        }
    }

    return_str[(seed_len * multiple) - 1] = '\0';

    ok = zcbor_tstr_put_lit(response_detail_map, "response")
        && zcbor_tstr_encode_ptr(response_detail_map, return_str, strlen(return_str));

    return GOLIOTH_RPC_OK;
}

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

int main(void)
{
    LOG_DBG("Start RPC sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    struct golioth_client *client = golioth_client_create(client_config);
    struct golioth_rpc *rpc = golioth_rpc_init(client);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    // Register the rpc itself and the parameters individually
    int err = golioth_rpc_register(rpc, "multiply", on_multiply, NULL);
    golioth_rpc_add_param_info(rpc,"multiply", "int_a",GOLIOTH_RPC_PARAM_TYPE_NUM);
    golioth_rpc_add_param_info(rpc,"multiply", "int_b",GOLIOTH_RPC_PARAM_TYPE_NUM);

    // Register the rpc itself and the parameters individually
    err = golioth_rpc_register(rpc, "repeat_word", on_repeat_word, NULL);
    golioth_rpc_add_param_info(rpc,"repeat_word", "word",GOLIOTH_RPC_PARAM_TYPE_STR);
    golioth_rpc_add_param_info(rpc,"repeat_word", "multiple",GOLIOTH_RPC_PARAM_TYPE_NUM);

    if (err)
    {
        LOG_ERR("Failed to register RPC: %d", err);
    }

    while (true)
    {
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
