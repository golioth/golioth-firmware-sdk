/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "unity.h"
#include "nvs.h"
#include "wifi.h"
#include "time.h"
#include "shell.h"
#include "util.h"
#include <golioth/golioth.h>

#define TAG "test"

// Wait this long, in seconds, for a response to a request
#define TEST_RESPONSE_TIMEOUT_S (2 * CONFIG_GOLIOTH_COAP_RESPONSE_TIMEOUT_S + 1)

static const char* _current_version = "1.2.3";

static SemaphoreHandle_t _connected_sem;
static SemaphoreHandle_t _disconnected_sem;
static golioth_client_t _client;
static uint32_t _initial_free_heap;
static bool _wifi_connected;

// Note: Don't put TEST_ASSERT_* statements in client callback functions, as this
// will cause a stack overflow in the client task if any of the assertions fail.
//
// I'm not sure exactly why this happens, but I suspect it's related to
// Unity's UNITY_FAIL_AND_BAIL macro that gets called on failing assertions.

static void on_client_event(golioth_client_t client, golioth_client_event_t event, void* arg) {
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    ESP_LOGI(TAG, "Golioth %s", is_connected ? "connected" : "disconnected");
    if (is_connected) {
        xSemaphoreGive(_connected_sem);
    } else {
        xSemaphoreGive(_disconnected_sem);
    }
}

static golioth_rpc_status_t on_double(
        zcbor_state_t* request_params_array,
        zcbor_state_t* response_detail_map,
        void* callback_arg) {
    double value;
    bool ok;

    ok = zcbor_float_decode(request_params_array, &value);
    if (!ok) {
        GLTH_LOGE(TAG, "Failed to decode value to be doubled");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    value *= 2;

    ok = zcbor_tstr_put_lit(response_detail_map, "value")
            && zcbor_float64_put(response_detail_map, value);
    if (!ok) {
        GLTH_LOGE(TAG, "Failed to encode value");
        return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
    }

    return GOLIOTH_RPC_OK;
}

static golioth_settings_status_t on_test_setting(int32_t new_value, void* arg) {
    GLTH_LOGI(TAG, "Setting LightDB TEST_SETTING to %" PRId32, new_value);
    golioth_lightdb_set_int_async(_client, "TEST_SETTING", new_value, NULL, NULL);
    return GOLIOTH_SETTINGS_SUCCESS;
}

static void test_connects_to_wifi(void) {
    if (_wifi_connected) {
        return;
    }
    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    TEST_ASSERT_TRUE(wifi_wait_for_connected_with_timeout(TEST_RESPONSE_TIMEOUT_S));
    _wifi_connected = true;
}

static void test_golioth_client_create(void) {
    if (!_client) {
        const char* psk_id = nvs_read_golioth_psk_id();
        const char* psk = nvs_read_golioth_psk();

        golioth_client_config_t config = {
                .credentials = {
                        .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                        .psk = {
                                .psk_id = psk_id,
                                .psk_id_len = strlen(psk_id),
                                .psk = psk,
                                .psk_len = strlen(psk),
                        }}};
        _client = golioth_client_create(&config);

        TEST_ASSERT_NOT_NULL(_client);
        golioth_client_register_event_callback(_client, on_client_event, NULL);
        golioth_rpc_register(_client, "double", on_double, NULL);
        golioth_settings_register_int(_client, "TEST_SETTING", on_test_setting, NULL);
    }
}

static void test_connects_to_golioth(void) {
    TEST_ASSERT_NOT_NULL(_client);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, golioth_client_start(_client));
    TEST_ASSERT_EQUAL(
            pdTRUE,
            xSemaphoreTake(_connected_sem, TEST_RESPONSE_TIMEOUT_S * 1000 / portTICK_PERIOD_MS));
}

static void test_golioth_client_heap_usage(void) {
    uint32_t post_connect_free_heap = esp_get_minimum_free_heap_size();
    int32_t golioth_heap_usage = _initial_free_heap - post_connect_free_heap;
    ESP_LOGI(TAG, "Estimated heap usage by Golioth stack = %" PRIu32, golioth_heap_usage);
    TEST_ASSERT_TRUE(golioth_heap_usage < 50000);
}

static void wait_for_empty_request_queue(void) {
    bool is_empty = false;

    // Wait up to 10 s for queue to be empty
    uint64_t timeout_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 5000;
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout_ms) {
        if (golioth_client_num_items_in_request_queue(_client) == 0) {
            is_empty = true;
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    TEST_ASSERT_TRUE(is_empty);
}

static void test_request_dropped_if_client_not_running(void) {
    TEST_ASSERT_EQUAL(GOLIOTH_OK, golioth_client_stop(_client));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(_disconnected_sem, 3000 / portTICK_PERIOD_MS));

    // Verify each request type returns proper state
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_INVALID_STATE, golioth_lightdb_set_int_async(_client, "a", 1, NULL, NULL));
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_INVALID_STATE, golioth_lightdb_get_async(_client, "a", NULL, NULL));
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_INVALID_STATE, golioth_lightdb_delete_async(_client, "a", NULL, NULL));
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_INVALID_STATE, golioth_lightdb_observe_async(_client, "a", NULL, NULL));

    TEST_ASSERT_EQUAL(GOLIOTH_OK, golioth_client_start(_client));
    TEST_ASSERT_EQUAL(
            pdTRUE,
            xSemaphoreTake(_connected_sem, TEST_RESPONSE_TIMEOUT_S * 1000 / portTICK_PERIOD_MS));

    wait_for_empty_request_queue();
}

static void test_lightdb_set_get_sync(void) {
    int randint = esp_random();
    ESP_LOGD(TAG, "randint = %d", randint);
    TEST_ASSERT_EQUAL(
            GOLIOTH_OK,
            golioth_lightdb_set_int_sync(_client, "test_int", randint, TEST_RESPONSE_TIMEOUT_S));

    // Delay for a bit. This is done because the value may not have been written to
    // the database on the back end yet.
    //
    // The server responds before the data is written to the database ("eventually consistent"),
    // so there's a chance that if we try to read immediately, we will get the wrong data,
    // so we need to wait to be sure.
    vTaskDelay(200 / portTICK_PERIOD_MS);

    int32_t get_randint = 0;
    TEST_ASSERT_EQUAL(
            GOLIOTH_OK,
            golioth_lightdb_get_int_sync(
                    _client, "test_int", &get_randint, TEST_RESPONSE_TIMEOUT_S));
    TEST_ASSERT_EQUAL(randint, get_randint);
}

static bool _on_get_test_int2_called = false;
static int32_t _test_int2_value = 0;
static void on_get_test_int2(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    golioth_response_t* arg_response = (golioth_response_t*)arg;
    *arg_response = *response;
    _on_get_test_int2_called = true;
    _test_int2_value = golioth_payload_as_int(payload, payload_size);
}

static bool _on_set_test_int2_called = false;
static void on_set_test_int2(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        void* arg) {
    golioth_response_t* arg_response = (golioth_response_t*)arg;
    *arg_response = *response;
    _on_set_test_int2_called = true;
}

static void test_lightdb_set_get_async(void) {
    _on_set_test_int2_called = false;
    _on_get_test_int2_called = false;
    golioth_response_t set_async_response = {};
    golioth_response_t get_async_response = {};

    int randint = esp_random();
    TEST_ASSERT_EQUAL(
            GOLIOTH_OK,
            golioth_lightdb_set_int_async(
                    _client, "test_int2", randint, on_set_test_int2, &set_async_response));

    // Wait for response
    uint64_t timeout_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) + TEST_RESPONSE_TIMEOUT_S * 1000;
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout_ms) {
        if (_on_set_test_int2_called) {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    TEST_ASSERT_TRUE(_on_set_test_int2_called);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, set_async_response.status);
    TEST_ASSERT_EQUAL(2, set_async_response.status_class);  // success
    TEST_ASSERT_EQUAL(4, set_async_response.status_code);   // changed

    // Delay for a bit. This is done because the value may not have been written to
    // the database on the back end yet.
    //
    // The server responds before the data is written to the database ("eventually consistent"),
    // so there's a chance that if we try to read immediately, we will get the wrong data,
    // so we need to wait to be sure.
    vTaskDelay(200 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL(
            GOLIOTH_OK,
            golioth_lightdb_get_async(_client, "test_int2", on_get_test_int2, &get_async_response));

    timeout_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) + TEST_RESPONSE_TIMEOUT_S * 1000;
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout_ms) {
        if (_on_get_test_int2_called) {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    TEST_ASSERT_TRUE(_on_get_test_int2_called);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, get_async_response.status);
    TEST_ASSERT_EQUAL(2, get_async_response.status_class);  // success
    TEST_ASSERT_EQUAL(5, get_async_response.status_code);   // content
    TEST_ASSERT_EQUAL(randint, _test_int2_value);
}

static bool _on_test_timeout_called = false;
static void on_test_timeout(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    golioth_response_t* arg_response = (golioth_response_t*)arg;
    *arg_response = *response;
    _on_test_timeout_called = true;
}

// This test takes about 30 seconds to complete
static void test_request_timeout_if_packets_dropped(void) {
    golioth_client_set_packet_loss_percent(100);

    int32_t dummy = 0;
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_TIMEOUT,
            golioth_lightdb_get_int_sync(_client, "expect_timeout", &dummy, 1));
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_TIMEOUT, golioth_lightdb_set_int_sync(_client, "expect_timeout", 4, 1));
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_TIMEOUT, golioth_lightdb_delete_sync(_client, "expect_timeout", 1));

    _on_test_timeout_called = false;
    golioth_response_t async_response = {};
    TEST_ASSERT_EQUAL(
            GOLIOTH_OK,
            golioth_lightdb_get_async(_client, "expect_timeout", on_test_timeout, &async_response));

    // Wait for async response to time out (must be longer than client task timeout of 10 s)
    uint64_t timeout_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 12000;
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout_ms) {
        if (_on_test_timeout_called) {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    TEST_ASSERT_TRUE(_on_test_timeout_called);
    TEST_ASSERT_EQUAL(GOLIOTH_ERR_TIMEOUT, async_response.status);

    // If a synchronous request is performed with infinite timeout specified,
    // the request will still timeout after CONFIG_GOLIOTH_COAP_RESPONSE_TIMEOUT_S.
    uint64_t now = (xTaskGetTickCount() * portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_TIMEOUT,
            golioth_lightdb_delete_sync(_client, "expect_timeout", GOLIOTH_SYS_WAIT_FOREVER));
    TEST_ASSERT_TRUE((xTaskGetTickCount() * portTICK_PERIOD_MS) - now > (1000 * CONFIG_GOLIOTH_COAP_RESPONSE_TIMEOUT_S));

    golioth_client_set_packet_loss_percent(0);

    // Wait for connected
    TEST_ASSERT_EQUAL(
            pdTRUE,
            xSemaphoreTake(_connected_sem, TEST_RESPONSE_TIMEOUT_S * 1000 / portTICK_PERIOD_MS));

    wait_for_empty_request_queue();
}

static void test_lightdb_error_if_path_not_found(void) {
    // Issue a sync GET request to an invalid path.
    // Verify a non-success response is received.

    // The server actually gives us a 2.05 response (success) for non-existant paths.
    // In this case, our SDK detects the payload is empty and returns GOLIOTH_ERR_NULL.
    int32_t dummy = 0;
    TEST_ASSERT_EQUAL(
            GOLIOTH_ERR_NULL,
            golioth_lightdb_get_int_sync(_client, "not_found", &dummy, TEST_RESPONSE_TIMEOUT_S));
}

static void test_client_task_stack_min_remaining(void) {
    // A bit of hack, but since we know the golioth_sys_thread_t is really
    // a FreeRTOS TaskHandle_t, just cast it directly.
    TaskHandle_t client_task = (TaskHandle_t)golioth_client_get_thread(_client);

    uint32_t stack_unused = uxTaskGetStackHighWaterMark(client_task);
    uint32_t stack_used = CONFIG_GOLIOTH_COAP_THREAD_STACK_SIZE - stack_unused;
    ESP_LOGI(
            TAG,
            "Client task stack used = %" PRIu32 ", unused = %" PRIu32,
            stack_used,
            stack_unused);

    // Verify at least 25% stack was not used
    TEST_ASSERT_TRUE(stack_unused >= CONFIG_GOLIOTH_COAP_THREAD_STACK_SIZE / 4);
}

static void test_client_destroy(void) {
    TEST_ASSERT_EQUAL(GOLIOTH_OK, golioth_client_stop(_client));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(_disconnected_sem, 3000 / portTICK_PERIOD_MS));

    // Request queue should be empty now
    TEST_ASSERT_EQUAL(0, golioth_client_num_items_in_request_queue(_client));

    golioth_client_destroy(_client);
    _client = NULL;
}

static bool _on_get_test_int3_called = false;
static int32_t _test_int3_value = 0;
static void on_test_int3(
        golioth_client_t client,
        const golioth_response_t* response,
        const char* path,
        const uint8_t* payload,
        size_t payload_size,
        void* arg) {
    ESP_LOG_BUFFER_HEXDUMP(TAG, payload, payload_size, ESP_LOG_DEBUG);

    if (golioth_payload_is_null(payload, payload_size)) {
        return;
    }

    _on_get_test_int3_called = true;
    _test_int3_value = golioth_payload_as_int(payload, payload_size);
}

static void test_lightdb_observation(void) {
    _on_get_test_int3_called = false;
    TEST_ASSERT_EQUAL(
            GOLIOTH_OK, golioth_lightdb_observe_async(_client, "test_int3", on_test_int3, NULL));

    // Wait up to 3 seconds to receive the initial value from the observation.
    //
    // It's possible this isn't received if test_int3 doesn't exist on the server.
    uint64_t timeout_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) + TEST_RESPONSE_TIMEOUT_S * 1000;
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout_ms) {
        if (_on_get_test_int3_called) {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    _on_get_test_int3_called = false;

    // Set a random number on test_int3
    int randint = esp_random();
    ESP_LOGD(TAG, "randint = %d", randint);
    TEST_ASSERT_EQUAL(
            GOLIOTH_OK,
            golioth_lightdb_set_int_sync(_client, "test_int3", randint, TEST_RESPONSE_TIMEOUT_S));

    // Wait up to 3 seconds to observe the new value
    timeout_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) + TEST_RESPONSE_TIMEOUT_S * 1000;
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout_ms) {
        if (_on_get_test_int3_called) {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    TEST_ASSERT_TRUE(_on_get_test_int3_called);
    TEST_ASSERT_EQUAL(randint, _test_int3_value);
}

static int built_in_test(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_connects_to_wifi);
    if (!_initial_free_heap) {
        // Snapshot of heap usage after connecting to WiFi. This is baseline/reference
        // which we compare against when gauging how much RAM the Golioth client uses.
        _initial_free_heap = esp_get_minimum_free_heap_size();
    }
    if (!_client) {
        RUN_TEST(test_golioth_client_create);
        RUN_TEST(test_connects_to_golioth);
    }
    RUN_TEST(test_lightdb_set_get_sync);
    RUN_TEST(test_lightdb_set_get_async);
    RUN_TEST(test_lightdb_observation);
    RUN_TEST(test_golioth_client_heap_usage);
    RUN_TEST(test_request_dropped_if_client_not_running);
    RUN_TEST(test_lightdb_error_if_path_not_found);
    RUN_TEST(test_request_timeout_if_packets_dropped);
    RUN_TEST(test_client_task_stack_min_remaining);
    RUN_TEST(test_client_destroy);
    UNITY_END();

    return 0;
}

static int connect(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_connects_to_wifi);
    if (!_client) {
        RUN_TEST(test_golioth_client_create);
        RUN_TEST(test_connects_to_golioth);
    }
    UNITY_END();

    return 0;
}

static int start_ota(int argc, char** argv) {
    connect(0, NULL);
    golioth_fw_update_init(_client, _current_version);
    return 0;
}

void app_main(void) {
    nvs_init();

    _connected_sem = xSemaphoreCreateBinary();
    _disconnected_sem = xSemaphoreCreateBinary();

    golioth_debug_set_log_level(GOLIOTH_DEBUG_LOG_LEVEL_DEBUG);

    esp_console_cmd_t built_in_test_cmd = {
            .command = "built_in_test",
            .help = "Run the built-in Unity tests",
            .func = built_in_test,
    };
    shell_register_command(&built_in_test_cmd);

    esp_console_cmd_t start_ota_cmd = {
            .command = "start_ota",
            .help = "Start the firmware update OTA task",
            .func = start_ota,
    };
    shell_register_command(&start_ota_cmd);

    esp_console_cmd_t connect_cmd = {
            .command = "connect",
            .help = "Connect to WiFi and Golioth",
            .func = connect,
    };
    shell_register_command(&connect_cmd);

    shell_start();

    while (1) {
        vTaskDelay(100000 / portTICK_PERIOD_MS);
    };
}
