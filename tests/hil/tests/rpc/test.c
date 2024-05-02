/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <golioth/client.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include <golioth/rpc.h>
#include <golioth/zcbor_utils.h>

LOG_TAG_DEFINE(test_rpc);

static golioth_sys_sem_t connected_sem;
static golioth_sys_sem_t disconnect_sem;
static golioth_sys_sem_t cancel_all_sem;

struct golioth_rpc *grpc = NULL;

static enum golioth_rpc_status on_no_response(zcbor_state_t *request_params_array,
                                              zcbor_state_t *response_detail_map,
                                              void *callback_arg)
{
    size_t arg_count = 0;
    while (!zcbor_list_or_map_end(request_params_array))
    {
        arg_count++;
        bool ok = zcbor_any_skip(request_params_array, NULL);
        if (!ok)
        {
            GLTH_LOGE(TAG,
                      "Failed to skip param (%" PRId32 ")",
                      (int32_t) zcbor_peek_error(request_params_array));
            return GOLIOTH_RPC_INVALID_ARGUMENT;
        }
    }

    GLTH_LOGI(TAG, "Received no_response with %u args", arg_count);

    return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_basic_return_type(zcbor_state_t *request_params_array,
                                                    zcbor_state_t *response_detail_map,
                                                    void *callback_arg)
{
    // one of {int, string, float, bool}
    struct zcbor_string type;

    bool ok = zcbor_tstr_decode(request_params_array, &type);
    if (!ok)
    {
        return GOLIOTH_RPC_INTERNAL;
    }

    if (strlen("int") == type.len && 0 == strncmp("int", (char *) type.value, type.len))
    {
        ok = zcbor_tstr_put_lit(response_detail_map, "value")
            && zcbor_int64_put(response_detail_map, 42);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to encode value");
            return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
        }
    }
    else if (strlen("string") == type.len && 0 == strncmp("string", (char *) type.value, type.len))
    {
        ok = zcbor_tstr_put_lit(response_detail_map, "value")
            && zcbor_tstr_put_lit(response_detail_map, "foo");
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to encode value");
            return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
        }
    }
    else if (strlen("float") == type.len && 0 == strncmp("float", (char *) type.value, type.len))
    {
        ok = zcbor_tstr_put_lit(response_detail_map, "value")
            && zcbor_float64_put(response_detail_map, 12.34);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to encode value");
            return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
        }
    }
    else if (strlen("bool") == type.len && 0 == strncmp("bool", (char *) type.value, type.len))
    {
        ok = zcbor_tstr_put_lit(response_detail_map, "value")
            && zcbor_bool_put(response_detail_map, true);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to encode value");
            return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
        }
    }
    else
    {
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_object_return_type(zcbor_state_t *request_params_array,
                                                     zcbor_state_t *response_detail_map,
                                                     void *callback_arg)
{
    bool ok = true;
    ok &= zcbor_tstr_put_lit(response_detail_map, "foo");
    ok &= zcbor_map_start_encode(response_detail_map, 1);
    ok &= zcbor_tstr_put_lit(response_detail_map, "bar");
    ok &= zcbor_map_start_encode(response_detail_map, 1);
    ok &= zcbor_tstr_put_lit(response_detail_map, "baz");
    ok &= zcbor_int64_put(response_detail_map, 357);
    ok &= zcbor_map_end_encode(response_detail_map, 1);
    ok &= zcbor_map_end_encode(response_detail_map, 1);

    if (!ok)
    {
        GLTH_LOGE(TAG, "FAILED to encode response");
        return GOLIOTH_RPC_INTERNAL;
    }

    return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_malformed_response(zcbor_state_t *request_params_array,
                                                     zcbor_state_t *response_detail_map,
                                                     void *callbark_arg)
{
    zcbor_tstr_put_lit(response_detail_map, "foo");
    zcbor_map_start_encode(response_detail_map, 1);
    zcbor_tstr_put_lit(response_detail_map, "bar");

    return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_disconnect(zcbor_state_t *request_params_array,
                                             zcbor_state_t *response_detail_map,
                                             void *callback_arg)
{
    golioth_sys_sem_give(disconnect_sem);

    return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_cancel_all(zcbor_state_t *request_params_array,
                                             zcbor_state_t *response_detail_map,
                                             void *callback_arg)
{
    golioth_sys_sem_give(cancel_all_sem);

    return GOLIOTH_RPC_OK;
}

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    if (event == GOLIOTH_CLIENT_EVENT_CONNECTED)
    {
        golioth_sys_sem_give(connected_sem);
    }
}

static void perform_rpc_registration(struct golioth_client *client)
{
    grpc = golioth_rpc_init(client);

    golioth_rpc_register(grpc, "no_response", on_no_response, NULL);

    golioth_rpc_register(grpc, "basic_return_type", on_basic_return_type, NULL);

    golioth_rpc_register(grpc, "object_return_type", on_object_return_type, NULL);

    golioth_rpc_register(grpc, "malformed_response", on_malformed_response, NULL);

    golioth_rpc_register(grpc, "disconnect", on_disconnect, NULL);

    golioth_rpc_register(grpc, "cancel_all", on_cancel_all, NULL);
}

void hil_test_entry(const struct golioth_client_config *config)
{
    connected_sem = golioth_sys_sem_create(1, 0);
    disconnect_sem = golioth_sys_sem_create(1, 0);
    cancel_all_sem = golioth_sys_sem_create(1, 0);

    struct golioth_client *client = golioth_client_create(config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_sys_sem_take(connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    perform_rpc_registration(client);

    while (1)
    {
        if (golioth_sys_sem_take(disconnect_sem, 0))
        {
            // Delay to ensure RPC response makes it to cloud
            golioth_sys_msleep(1000);

            GLTH_LOGI(TAG, "Stopping client");
            golioth_client_stop(client);

            golioth_sys_msleep(3 * 1000);

            GLTH_LOGI(TAG, "Starting client");
            golioth_client_start(client);
        }

        if (golioth_sys_sem_take(cancel_all_sem, 0))
        {
            // Delay to ensure RPC response makes it to cloud
            golioth_sys_msleep(1000);

            GLTH_LOGI(TAG, "Cancelling observations");
            golioth_rpc_deinit(grpc);

            golioth_sys_msleep(3 * 1000);

            GLTH_LOGI(TAG, "Observations cancelled");
            golioth_sys_msleep(12 * 1000);

            /* re-register for future tests */
            perform_rpc_registration(client);
        }

        golioth_sys_msleep(100);
    }
}
