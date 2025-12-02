/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <golioth/stream.h>
#include <golioth/payload_utils.h>
#include <golioth/rpc.h>
#include <golioth/settings.h>
#include <golioth/golioth_sys.h>

#include "golioth_basics.h"
#include "fw_update.h"

LOG_TAG_DEFINE(golioth_basics);

// Current firmware version
static const char *_current_version = "1.2.5";

// Configurable via LightDB State at path "desired/my_config"
int32_t _my_config = 0;

// Configurable via Settings service, key = "LOOP_DELAY_S"
int32_t _loop_delay_s = 10;

// Given if/when the we have a connection to Golioth
static golioth_sys_sem_t _connected_sem;

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        golioth_sys_sem_give(_connected_sem);
    }
    GLTH_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

static enum golioth_settings_status on_loop_delay_setting(int32_t new_value, void *arg)
{
    GLTH_LOGI(TAG, "Setting loop delay to %" PRId32 " s", new_value);
    _loop_delay_s = new_value;
    return GOLIOTH_SETTINGS_SUCCESS;
}

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
        GLTH_LOGE(TAG, "Failed to decode array items");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    value = a * b;

    GLTH_LOGD(TAG, "%lf * %lf = %lf", a, b, value);

    ok = zcbor_tstr_put_lit(response_detail_map, "value")
        && zcbor_float64_put(response_detail_map, value);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode value");
        return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
    }

    return GOLIOTH_RPC_OK;
}


// Callback function for asynchronous get request of LightDB path "my_int"
static void on_get_my_int(struct golioth_client *client,
                          enum golioth_status status,
                          const struct golioth_coap_rsp_code *coap_rsp_code,
                          const char *path,
                          const uint8_t *payload,
                          size_t payload_size,
                          void *arg)
{
    // It's a good idea to check the response status, to make sure the request didn't time out.
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "on_get_my_int status = %s", golioth_status_to_str(status));
        return;
    }

    // Now we can use a helper function to convert the binary payload to an integer.
    int32_t value = golioth_payload_as_int(payload, payload_size);
    GLTH_LOGI(TAG, "Callback got my_int = %" PRId32, value);
}

// Callback function for asynchronous observation of LightDB path "desired/my_config"
static void on_my_config(struct golioth_client *client,
                         enum golioth_status status,
                         const struct golioth_coap_rsp_code *coap_rsp_code,
                         const char *path,
                         const uint8_t *payload,
                         size_t payload_size,
                         void *arg)
{
    if (status != GOLIOTH_OK)
    {
        return;
    }

    // Payload might be null if desired/my_config is deleted, so ignore that case
    if (golioth_payload_is_null(payload, payload_size))
    {
        return;
    }

    int32_t desired_value = golioth_payload_as_int(payload, payload_size);
    GLTH_LOGI(TAG, "Cloud desires %s = %" PRId32 ". Setting now.", path, desired_value);
    _my_config = desired_value;
    golioth_lightdb_delete_async(client, path, NULL, NULL);
}


void golioth_basics(struct golioth_client *client)
{
    // Initialize the Settings and RPC services
    struct golioth_settings *settings = golioth_settings_init(client);
    struct golioth_rpc *rpc = golioth_rpc_init(client);

    // Register a callback function that will be called by the client thread when
    // connect and disconnect events happen.
    //
    // This is optional, but can be useful for synchronizing operations on connect/disconnect
    // events. For this example, the on_client_event callback will simply log a message.
    _connected_sem = golioth_sys_sem_create(1, 0);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGI(TAG, "Waiting for connection to Golioth...");
    golioth_sys_sem_take(_connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    // At this point, we have a client that can be used to interact with Golioth services:
    //      Logging
    //      Over-the-Air (OTA) firmware updates
    //      LightDB state
    //      Stream

    // You can use any of the GLTH_LOGX macros (e.g. GLTH_LOGI, GLTH_LOGE),
    // to log a message to stdout.
    //
    // If you've set CONFIG_GOLIOTH_AUTO_LOG_TO_CLOUD to 1, then the message
    // will also be uploaded to the Golioth Logging Service, which you can
    // view at console.golioth.io.
    GLTH_LOGI(TAG, "Hello, Golioth!");


    // For OTA, we will spawn a background thread that will listen for firmware
    // updates from Golioth and automatically update firmware on the device
    golioth_fw_update_init(client, _current_version);

    // There are a number of different functions you can call to get and set values in
    // LightDB state, based on the type of value (e.g. int, bool, float, string, JSON).
    //
    // This is an "asynchronous" function, meaning that the function will return
    // immediately and the integer will be sent to Golioth at a later time.
    // Internally, the request is added to a queue which is processed
    // by the Golioth client thread.
    //
    // Any functions provided by this SDK ending in _async behave the same way.
    //
    // The last two arguments are for an optional callback function and argument,
    // in case the user wants to be notified when the set request has completed
    // and received acknowledgement from the Golioth server. In this case
    // we set them to NULL, which makes this a "fire-and-forget" request.
    golioth_lightdb_set_int_async(client, "my_int", 42, NULL, NULL);

    // We can also send requests "synchronously", meaning the function will block
    // until one of 3 things happen (whichever comes first):
    //
    //  1. We receive a response to the request from the server
    //  2. The user-provided timeout expires
    //  3. The default client thread timeout expires (GOLIOTH_COAP_RESPONSE_TIMEOUT_S)
    //
    // In this case, we will block for up to 2 seconds waiting for the server response.
    // We'll check the return code to know whether a timeout happened.
    //
    // Any function provided by this SDK ending in _sync will have the same meaning.
    enum golioth_status status = golioth_lightdb_set_string_sync(client, "my_string", "asdf", 4, 5);
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGE(TAG, "Error setting string: %s", golioth_status_to_str(status));
    }

    // Read back the integer we set above
    int32_t readback_int = 0;
    status = golioth_lightdb_get_int_sync(client, "my_int", &readback_int, 5);
    if (status == GOLIOTH_OK)
    {
        GLTH_LOGI(TAG, "Synchronously got my_int = %" PRId32, readback_int);
    }
    else
    {
        GLTH_LOGE(TAG, "Synchronous get my_int failed: %s", golioth_status_to_str(status));
    }

    // To asynchronously get a value from LightDB, a callback function must be provided
    golioth_lightdb_get_async(client, "my_int", GOLIOTH_CONTENT_TYPE_JSON, on_get_my_int, NULL);

    // We can also "observe" paths in LightDB state. The Golioth cloud will notify
    // our client whenever the resource at that path changes, without needing
    // to poll.
    //
    // This can be used to implement the "digital twin" concept that is common in IoT.
    //
    // In this case, we will observe the path desired/my_config for changes.
    // The callback will read the value, update it locally, then delete the path
    // to indicate that the desired state was processed (the "twins" should be
    // in sync at that point).
    //
    // If you want to try this out, log into Golioth console (console.golioth.io),
    // go to the "LightDB State" tab, and add a new item for desired/my_config.
    // Once set, the on_my_config callback function should be called here.
    golioth_lightdb_observe_async(client,
                                  "desired/my_config",
                                  GOLIOTH_CONTENT_TYPE_JSON,
                                  on_my_config,
                                  NULL);

    // Golioth Stream functions are similar to LightDB state.
    const char *sbuf = "{\"my_stream_int\":15}";
    golioth_stream_set_async(client,
                             "/",
                             GOLIOTH_CONTENT_TYPE_JSON,
                             (uint8_t *) sbuf,
                             strlen(sbuf),
                             NULL,
                             NULL);

    // We can register Remote Procedure Call (RPC) methods. RPCs allow
    // remote users to "call a function" on the device.
    //
    // In this case, the device provides a "multiply" method, which takes two integer
    // input parameters and multiplies them, returning the resulting value.
    golioth_rpc_register(rpc, "multiply", on_multiply, NULL);

    // We can register a callback for persistent settings. The Settings service
    // allows remote users to manage and push settings to devices.
    golioth_settings_register_int(settings, "LOOP_DELAY_S", on_loop_delay_setting, NULL);

    // Now we'll just sit in a loop and update a LightDB state variable every
    // once in a while.
    GLTH_LOGI(TAG, "Entering endless loop");
    int32_t counter = 0;
    while (1)
    {
        golioth_lightdb_set_int_async(client, "counter", counter, NULL, NULL);
        GLTH_LOGI(TAG, "Sending hello! %" PRId32, counter);
        counter++;
        golioth_sys_msleep(_loop_delay_s * 1000);
    };

    // That pretty much covers the basics of this SDK!
    //
    // If you log into the Golioth console, you should see the log messages and
    // LightDB state should look something like this:
    //
    // {
    //      "counter": 10,
    //      "my_int": 42,
    //      "my_string": "asdf"
    // }
}
