#include <unity.h>
#include <fff.h>


DEFINE_FFF_GLOBALS;

static const char* last_err_msg = NULL;
static const char* last_wrn_msg = NULL;

#undef CONFIG_GOLIOTH_DEBUG_LOG_ENABLE
#define CONFIG_GOLIOTH_DEBUG_LOG_ENABLE 1
#define GLTH_LOGX(...)
#define GLTH_LOG_BUFFER_HEXDUMP(...)
#define GLTH_LOGE(TAG, msg, ...) last_err_msg = msg;
#define GLTH_LOGW(TAG, msg, ...) last_wrn_msg = msg;

#include "fakes/golioth_coap_client_fake.h"
#include "fakes/golioth_time_fake.h"
#include "../src/golioth_rpc.c"

FAKE_VALUE_FUNC(
        golioth_rpc_status_t,
        test_rpc_method_fn,
        const char*,
        const cJSON*,
        uint8_t*,
        size_t,
        void*);

golioth_rpc_t grpc_fake;
char last_coap_payload_str[256];

golioth_status_t golioth_coap_client_set_custom_fake(
        golioth_client_t client,
        const char* path_prefix,
        const char* path,
        uint32_t content_type,
        const uint8_t* payload,
        size_t payload_size,
        golioth_set_cb_fn callback,
        void* callback_arg,
        bool is_synchronous,
        int32_t timeout_s) {
    memset(last_coap_payload_str, 0, sizeof(last_coap_payload_str));
    strcpy(last_coap_payload_str, (const char*)payload);

    return GOLIOTH_OK;
}

void setUp(void) {
    memset(&grpc_fake, 0, sizeof(grpc_fake));
    golioth_coap_client_get_rpc_fake.return_val = &grpc_fake;
    golioth_coap_client_set_fake.custom_fake = golioth_coap_client_set_custom_fake;
}
void tearDown(void) {
    last_err_msg = NULL;
    last_wrn_msg = NULL;
    memset(last_coap_payload_str, 0, sizeof(last_coap_payload_str));
    RESET_FAKE(golioth_coap_client_get_rpc);
    RESET_FAKE(golioth_coap_client_observe_async);
    RESET_FAKE(golioth_coap_client_set);
    RESET_FAKE(test_rpc_method_fn);
    FFF_RESET_HISTORY();
}

void test_rpc_register(void) {
    golioth_status_t ret = golioth_rpc_register(NULL, "", NULL, NULL);

    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    TEST_ASSERT_EQUAL(1, grpc_fake.num_rpcs);
    TEST_ASSERT_EQUAL(1, golioth_coap_client_observe_async_fake.call_count);
}

void test_rpc_register_multi(void) {
    for (int i = 0; i < 3; i++) {
        golioth_status_t ret = golioth_rpc_register(NULL, "", NULL, NULL);
        TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    }

    TEST_ASSERT_EQUAL(3, grpc_fake.num_rpcs);
    TEST_ASSERT_EQUAL(1, golioth_coap_client_observe_async_fake.call_count);
}

void test_rpc_register_too_many(void) {
    for (int i = 0; i < CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS; i++) {
        golioth_status_t ret = golioth_rpc_register(NULL, "", NULL, NULL);
        TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    }

    golioth_status_t ret = golioth_rpc_register(NULL, "", NULL, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_ERR_MEM_ALLOC, ret);

    TEST_ASSERT_EQUAL(8, grpc_fake.num_rpcs);
}

void test_rpc_call_not_json(void) {
    const char* payload = "Not JSON";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_malformed(void) {
    const char* payload = "{gobbledygook:";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL_STRING("Failed to parse rpc call", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_no_id(void) {
    const char* payload = "{\"method\":\"foo\", \"params\" : [] }";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL_STRING("Key id not found", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_no_method(void) {
    const char* payload = "{\"id\":\"123\", \"params\" : [] }";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL_STRING("Key method not found", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_no_params(void) {
    const char* payload = "{\"method\":\"foo\", \"id\":\"123\"}";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL_STRING("Key params not found", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_not_registered(void) {
    const char* payload = "{\"method\":\"foo\", \"id\":\"123\", \"params\" : [] }";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL_STRING("Method %s not registered", last_wrn_msg);
    TEST_ASSERT_EQUAL(1, golioth_coap_client_set_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("{ \"id\": \"123\", \"statusCode\": 14 }", last_coap_payload_str);
}

void test_rpc_call_one(void) {
    golioth_status_t ret = golioth_rpc_register(NULL, "test", test_rpc_method_fn, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const char* payload = "{\"method\":\"test\", \"id\":\"123\", \"params\" : [] }";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL(1, test_rpc_method_fn_fake.call_count);
}

char last_received_method_name[10];

golioth_rpc_status_t rpc_method_fake(
        const char* method,
        const cJSON* params,
        uint8_t* detail,
        size_t detail_size,
        void* callback_arg) {
    strcpy(last_received_method_name, method);
    strcpy((char*)detail, "{\"return_val\":\"foo\"}");

    // callback_arg holds a boolean that indicates whether or not to verify the parameters
    if ((bool)callback_arg) {
        TEST_ASSERT_TRUE(cJSON_IsArray(params));
        TEST_ASSERT_EQUAL(2, cJSON_GetArraySize(params));
        const cJSON* item = cJSON_GetArrayItem(params, 0);
        TEST_ASSERT_TRUE(cJSON_IsString(item));
        TEST_ASSERT_EQUAL_STRING("a", item->valuestring);
        item = cJSON_GetArrayItem(params, 1);
        TEST_ASSERT_TRUE(cJSON_IsNumber(item));
        TEST_ASSERT_EQUAL(248, item->valueint);
    }

    return RPC_OK;
}

void test_rpc_call_with_return(void) {
    test_rpc_method_fn_fake.custom_fake = rpc_method_fake;
    golioth_status_t ret = golioth_rpc_register(NULL, "test", test_rpc_method_fn, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const char* payload = "{\"method\":\"test\", \"id\":\"123\", \"params\" : [] }";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL(1, test_rpc_method_fn_fake.call_count);
    TEST_ASSERT_EQUAL_STRING(
            "{ \"id\": \"123\", \"statusCode\": 0, \"detail\": {\"return_val\":\"foo\"} }",
            last_coap_payload_str);
}

void test_rpc_call_one_with_params(void) {
    test_rpc_method_fn_fake.custom_fake = rpc_method_fake;
    golioth_status_t ret = golioth_rpc_register(NULL, "test", test_rpc_method_fn, (void*)true);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const char* payload = "{\"method\":\"test\", \"id\":\"123\", \"params\" : [\"a\",248] }";
    on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

    TEST_ASSERT_EQUAL(1, test_rpc_method_fn_fake.call_count);
}

void test_rpc_call_same_multiple(void) {
    golioth_status_t ret = golioth_rpc_register(NULL, "test", test_rpc_method_fn, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const char* payload = "{\"method\":\"test\", \"id\":\"123\", \"params\" : [] }";
    for (int i = 0; i < 100; i++) {
        on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);
    }

    TEST_ASSERT_EQUAL(100, test_rpc_method_fn_fake.call_count);
}

void test_rpc_register_many_call_all(void) {
    test_rpc_method_fn_fake.custom_fake = rpc_method_fake;
    char* method_names[CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS];
    for (int i = 0; i < CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS; i++) {
        asprintf(&method_names[i], "test%d", i);
        golioth_status_t ret =
                golioth_rpc_register(NULL, method_names[i], test_rpc_method_fn, NULL);
        TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    }
    for (int i = 0; i < CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS; i++) {
        char* payload = NULL;
        asprintf(
                &payload, "{\"method\":\"%s\", \"id\":\"123\", \"params\" : [] }", method_names[i]);
        on_rpc(NULL, NULL, NULL, (const uint8_t*)payload, strlen(payload), NULL);

        TEST_ASSERT_EQUAL(i + 1, test_rpc_method_fn_fake.call_count);
        TEST_ASSERT_EQUAL_STRING(method_names[i], last_received_method_name);
        free(payload);
        free(method_names[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rpc_register);
    RUN_TEST(test_rpc_register_multi);
    RUN_TEST(test_rpc_register_too_many);
    RUN_TEST(test_rpc_call_not_json);
    RUN_TEST(test_rpc_call_malformed);
    RUN_TEST(test_rpc_call_no_id);
    RUN_TEST(test_rpc_call_no_method);
    RUN_TEST(test_rpc_call_no_params);
    RUN_TEST(test_rpc_call_not_registered);
    RUN_TEST(test_rpc_call_one);
    RUN_TEST(test_rpc_call_with_return);
    RUN_TEST(test_rpc_call_one_with_params);
    RUN_TEST(test_rpc_call_same_multiple);
    RUN_TEST(test_rpc_register_many_call_all);
    return UNITY_END();
}
