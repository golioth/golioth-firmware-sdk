/*
 * copyright (c) 2023 golioth, inc.
 *
 * spdx-license-identifier: apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rpc_sample, LOG_LEVEL_DBG);

#include <golioth/golioth.h>
#include <samples/common/sample_credentials.h>
#include <samples/common/net_connect.h>
#include <string.h>
#include <zephyr/kernel.h>

static K_SEM_DEFINE(connected, 0, 1);

static golioth_rpc_status_t on_multiply(
    zcbor_state_t* request_params_array,
    zcbor_state_t* response_detail_map,
    void* callback_arg) {
    double a, b;
    double value;
    bool ok;

    ok = zcbor_float_decode(request_params_array, &a)
        && zcbor_float_decode(request_params_array, &b);
    if (!ok) {
        LOG_ERR("Failed to decode array items");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    value = a * b;

    LOG_DBG("%lf * %lf = %lf", a, b, value);

    ok = zcbor_tstr_put_lit(response_detail_map, "value")
        && zcbor_float64_put(response_detail_map, value);
    if (!ok) {
        LOG_ERR("Failed to encode value");
        return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
    }

    return GOLIOTH_RPC_OK;
}

static void on_client_event(golioth_client_t client, golioth_client_event_t event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected) {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

int main(void) {
    LOG_DBG("Start RPC sample");

    net_connect();

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const golioth_client_config_t* client_config = golioth_sample_credentials_get();

    golioth_client_t client = golioth_client_create(client_config);

    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    int err = golioth_rpc_register(client, "multiply", on_multiply, NULL);

    if (err) {
        LOG_ERR("Failed to register RPC: %d", err);
    }

    while (true) {
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
