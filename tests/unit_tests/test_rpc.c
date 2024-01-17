#define _GNU_SOURCE

#include <stdio.h>
#include <unity.h>
#include <fff.h>


DEFINE_FFF_GLOBALS;

static const char *last_err_msg = NULL;
static const char *last_wrn_msg = NULL;

#define CONFIG_GOLIOTH_RPC
#define CONFIG_GOLIOTH_DEBUG_LOG
#define GLTH_LOGX(...)
#define GLTH_LOG_BUFFER_HEXDUMP(...)
#define GLTH_LOGE(TAG, msg, ...) last_err_msg = msg;
#define GLTH_LOGW(TAG, msg, ...) last_wrn_msg = msg;

#include "fakes/coap_client_fake.h"
#include "../../src/rpc.c"

FAKE_VALUE_FUNC(enum golioth_rpc_status,
                test_rpc_method_fn,
                zcbor_state_t *,
                zcbor_state_t *,
                void *);

struct golioth_rpc grpc;
uint8_t last_coap_payload[256];
size_t last_coap_payload_size;

enum golioth_status golioth_coap_client_set_custom_fake(struct golioth_client *client,
                                                        const char *path_prefix,
                                                        const char *path,
                                                        uint32_t content_type,
                                                        const uint8_t *payload,
                                                        size_t payload_size,
                                                        golioth_set_cb_fn callback,
                                                        void *callback_arg,
                                                        bool is_synchronous,
                                                        int32_t timeout_s)
{
    memcpy(last_coap_payload, payload, payload_size);
    last_coap_payload_size = payload_size;

    return GOLIOTH_OK;
}

void setUp(void)
{
    memset(&grpc, 0, sizeof(grpc));
    golioth_coap_client_set_fake.custom_fake = golioth_coap_client_set_custom_fake;
}
void tearDown(void)
{
    last_err_msg = NULL;
    last_wrn_msg = NULL;
    last_coap_payload_size = 0;
    RESET_FAKE(golioth_coap_client_observe_async);
    RESET_FAKE(golioth_coap_client_set);
    RESET_FAKE(test_rpc_method_fn);
    FFF_RESET_HISTORY();
}

void test_rpc_register(void)
{
    enum golioth_status ret = golioth_rpc_register(&grpc, "", NULL, NULL);

    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    TEST_ASSERT_EQUAL(1, grpc.num_rpcs);
    TEST_ASSERT_EQUAL(1, golioth_coap_client_observe_async_fake.call_count);
}

void test_rpc_register_multi(void)
{
    for (int i = 0; i < 3; i++)
    {
        enum golioth_status ret = golioth_rpc_register(&grpc, "", NULL, NULL);
        TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    }

    TEST_ASSERT_EQUAL(3, grpc.num_rpcs);
    TEST_ASSERT_EQUAL(1, golioth_coap_client_observe_async_fake.call_count);
}

void test_rpc_register_too_many(void)
{
    for (int i = 0; i < CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS; i++)
    {
        enum golioth_status ret = golioth_rpc_register(&grpc, "", NULL, NULL);
        TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    }

    enum golioth_status ret = golioth_rpc_register(&grpc, "", NULL, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_ERR_MEM_ALLOC, ret);

    TEST_ASSERT_EQUAL(8, grpc.num_rpcs);
}

void test_rpc_call_not_json(void)
{
    const char *payload = "Not CBOR";
    on_rpc(NULL, NULL, NULL, (const uint8_t *) payload, strlen(payload), &grpc);

    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_malformed(void)
{
    const uint8_t payload[] = {
        0xA1, /* map(1) */
        0x6C, /* text(12) */
        0x67,
        0x6F,
        0x62,
        0x62,
        0x6C,
        0x65,
        0x64,
        0x79,
        0x67,
        0x6F,
        0x6F,
        0x6B, /* "gobbledygook" */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL_STRING("Failed to parse tstr map", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_no_id(void)
{
    const uint8_t payload[] = {
        0xA2,                               /* map(2) */
        0x66,                               /* text(6) */
        0x6D, 0x65, 0x74, 0x68, 0x6F, 0x64, /* "method" */
        0x63,                               /* text(3) */
        0x66, 0x6F, 0x6F,                   /* "foo" */
        0x66,                               /* text(6) */
        0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, /* "params" */
        0x80,                               /* array(0) */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL_STRING("Failed to parse tstr map", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_no_method(void)
{
    const uint8_t payload[] = {
        0xA2, /* map(2) */
        0x62, /* text(2) */
        0x69,
        0x64, /* "id" */
        0x63, /* text(3) */
        0x31,
        0x32,
        0x33, /* "123" */
        0x66, /* text(6) */
        0x70,
        0x61,
        0x72,
        0x61,
        0x6D,
        0x73, /* "params" */
        0x80, /* array(0) */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL_STRING("Failed to parse tstr map", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_no_params(void)
{
    const uint8_t payload[] = {
        0xA2,                               /* map(2) */
        0x66,                               /* text(6) */
        0x6D, 0x65, 0x74, 0x68, 0x6F, 0x64, /* "method" */
        0x63,                               /* text(3) */
        0x66, 0x6F, 0x6F,                   /* "foo" */
        0x62,                               /* text(2) */
        0x69, 0x64,                         /* "id" */
        0x63,                               /* text(3) */
        0x31, 0x32, 0x33,                   /* "123" */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL_STRING("Failed to parse tstr map", last_err_msg);
    TEST_ASSERT_EQUAL(0, golioth_coap_client_set_fake.call_count);
}

void test_rpc_call_not_registered(void)
{
    const uint8_t payload[] = {
        0xA3,                               /* map(3) */
        0x66,                               /* text(6) */
        0x6D, 0x65, 0x74, 0x68, 0x6F, 0x64, /* "method" */
        0x63,                               /* text(3) */
        0x66, 0x6F, 0x6F,                   /* "foo" */
        0x62,                               /* text(2) */
        0x69, 0x64,                         /* "id" */
        0x63,                               /* text(3) */
        0x31, 0x32, 0x33,                   /* "123" */
        0x66,                               /* text(6) */
        0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, /* "params" */
        0x80,                               /* array(0) */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL_STRING("Method %.*s not registered", last_wrn_msg);
    TEST_ASSERT_EQUAL(1, golioth_coap_client_set_fake.call_count);

    const uint8_t expected[] = {
        0xBF,                                                       /* map(*) */
        0x62,                                                       /* text(2) */
        0x69, 0x64,                                                 /* "id" */
        0x63,                                                       /* text(3) */
        0x31, 0x32, 0x33,                                           /* "123" */
        0x6A,                                                       /* text(10) */
        0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x43, 0x6F, 0x64, 0x65, /* "statusCode" */
        0x02,                                                       /* unsigned(2) */
        0xFF,                                                       /* primitive(*) */
    };
    TEST_ASSERT_EQUAL(sizeof(expected), last_coap_payload_size);
    TEST_ASSERT_EQUAL_MEMORY(expected, last_coap_payload, last_coap_payload_size);
}

void test_rpc_call_one(void)
{
    enum golioth_status ret = golioth_rpc_register(&grpc, "test", test_rpc_method_fn, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const uint8_t payload[] = {
        0xA3,                               /* map(3) */
        0x66,                               /* text(6) */
        0x6D, 0x65, 0x74, 0x68, 0x6F, 0x64, /* "method" */
        0x64,                               /* text(4) */
        0x74, 0x65, 0x73, 0x74,             /* "test" */
        0x62,                               /* text(2) */
        0x69, 0x64,                         /* "id" */
        0x63,                               /* text(3) */
        0x31, 0x32, 0x33,                   /* "123" */
        0x66,                               /* text(6) */
        0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, /* "params" */
        0x80,                               /* array(0) */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL(1, test_rpc_method_fn_fake.call_count);
}

/** Extract the major type, i.e. the first 3 bits of the header byte. */
#define ZCBOR_MAJOR_TYPE(header_byte) ((zcbor_major_type_t) (((header_byte) >> 5) & 0x7))

enum golioth_rpc_status rpc_method_fake(zcbor_state_t *request_params_array,
                                        zcbor_state_t *response_detail_map,
                                        void *callback_arg)
{
    zcbor_tstr_put_lit(response_detail_map, "return_val");
    zcbor_tstr_put_lit(response_detail_map, "foo");

    // callback_arg holds a boolean that indicates whether or not to verify the parameters
    if ((bool) callback_arg)
    {
        bool ok;
        struct zcbor_string param_1;
        int32_t param_2;

        ok = zcbor_tstr_decode(request_params_array, &param_1);
        TEST_ASSERT_TRUE(ok);
        TEST_ASSERT_EQUAL_STRING_LEN("a", param_1.value, param_1.len);

        ok = zcbor_int32_decode(request_params_array, &param_2);
        TEST_ASSERT_TRUE(ok);
        TEST_ASSERT_EQUAL(248, param_2);
    }

    return GOLIOTH_RPC_OK;
}

void test_rpc_call_with_return(void)
{
    test_rpc_method_fn_fake.custom_fake = rpc_method_fake;
    enum golioth_status ret = golioth_rpc_register(&grpc, "test", test_rpc_method_fn, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const uint8_t payload[] = {
        0xA3,                               /* map(3) */
        0x66,                               /* text(6) */
        0x6D, 0x65, 0x74, 0x68, 0x6F, 0x64, /* "method" */
        0x64,                               /* text(4) */
        0x74, 0x65, 0x73, 0x74,             /* "test" */
        0x62,                               /* text(2) */
        0x69, 0x64,                         /* "id" */
        0x63,                               /* text(3) */
        0x31, 0x32, 0x33,                   /* "123" */
        0x66,                               /* text(6) */
        0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, /* "params" */
        0x80,                               /* array(0) */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL(1, test_rpc_method_fn_fake.call_count);

    const uint8_t expected[] = {
        0xBF,                                                       /* map(*) */
        0x62,                                                       /* text(2) */
        0x69, 0x64,                                                 /* "id" */
        0x63,                                                       /* text(3) */
        0x31, 0x32, 0x33,                                           /* "123" */
        0x66,                                                       /* text(6) */
        0x64, 0x65, 0x74, 0x61, 0x69, 0x6C,                         /* "detail" */
        0xBF,                                                       /* map(*) */
        0x6A,                                                       /* text(10) */
        0x72, 0x65, 0x74, 0x75, 0x72, 0x6E, 0x5F, 0x76, 0x61, 0x6C, /* "return_val" */
        0x63,                                                       /* text(3) */
        0x66, 0x6F, 0x6F,                                           /* "foo" */
        0xFF,                                                       /* primitive(*) */
        0x6A,                                                       /* text(10) */
        0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x43, 0x6F, 0x64, 0x65, /* "statusCode" */
        0x00,                                                       /* unsigned(0) */
        0xFF,                                                       /* primitive(*) */
    };

    TEST_ASSERT_EQUAL(sizeof(expected), last_coap_payload_size);
    TEST_ASSERT_EQUAL_MEMORY(expected, last_coap_payload, last_coap_payload_size);
}

void test_rpc_call_one_with_params(void)
{
    test_rpc_method_fn_fake.custom_fake = rpc_method_fake;
    enum golioth_status ret =
        golioth_rpc_register(&grpc, "test", test_rpc_method_fn, (void *) true);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const uint8_t payload[] = {
        0xA3,                               /* map(3) */
        0x66,                               /* text(6) */
        0x6D, 0x65, 0x74, 0x68, 0x6F, 0x64, /* "method" */
        0x64,                               /* text(4) */
        0x74, 0x65, 0x73, 0x74,             /* "test" */
        0x62,                               /* text(2) */
        0x69, 0x64,                         /* "id" */
        0x63,                               /* text(3) */
        0x31, 0x32, 0x33,                   /* "123" */
        0x66,                               /* text(6) */
        0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, /* "params" */
        0x82,                               /* array(2) */
        0x61,                               /* text(1) */
        0x61,                               /* "a" */
        0x18, 0xF8,                         /* unsigned(248) */
    };
    on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

    TEST_ASSERT_EQUAL(1, test_rpc_method_fn_fake.call_count);
}

void test_rpc_call_same_multiple(void)
{
    enum golioth_status ret = golioth_rpc_register(&grpc, "test", test_rpc_method_fn, NULL);
    TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);

    const uint8_t payload[] = {
        0xA3,                               /* map(3) */
        0x66,                               /* text(6) */
        0x6D, 0x65, 0x74, 0x68, 0x6F, 0x64, /* "method" */
        0x64,                               /* text(4) */
        0x74, 0x65, 0x73, 0x74,             /* "test" */
        0x62,                               /* text(2) */
        0x69, 0x64,                         /* "id" */
        0x63,                               /* text(3) */
        0x31, 0x32, 0x33,                   /* "123" */
        0x66,                               /* text(6) */
        0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, /* "params" */
        0x80,                               /* array(0) */
    };

    for (int i = 0; i < 100; i++)
    {
        on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);
    }

    TEST_ASSERT_EQUAL(100, test_rpc_method_fn_fake.call_count);
}

void test_rpc_register_many_call_all(void)
{
    test_rpc_method_fn_fake.custom_fake = rpc_method_fake;
    char *method_names[CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS];
    for (int i = 0; i < CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS; i++)
    {
        asprintf(&method_names[i], "test%d", i);
        enum golioth_status ret =
            golioth_rpc_register(&grpc, method_names[i], test_rpc_method_fn, NULL);
        TEST_ASSERT_EQUAL(GOLIOTH_OK, ret);
    }
    for (int i = 0; i < CONFIG_GOLIOTH_RPC_MAX_NUM_METHODS; i++)
    {
        const uint8_t payload[] = {
            0xA3, /* map(3) */
            0x66, /* text(6) */
            0x6D,
            0x65,
            0x74,
            0x68,
            0x6F,
            0x64,                           /* "method" */
            0x60 + strlen(method_names[i]), /* text(...) */
            0x74,
            0x65,
            0x73,
            0x74,
            method_names[i][4], /* "test%d" */
            0x62,               /* text(2) */
            0x69,
            0x64, /* "id" */
            0x63, /* text(3) */
            0x31,
            0x32,
            0x33, /* "123" */
            0x66, /* text(6) */
            0x70,
            0x61,
            0x72,
            0x61,
            0x6D,
            0x73, /* "params" */
            0x80, /* array(0) */
        };
        on_rpc(NULL, NULL, NULL, payload, sizeof(payload), &grpc);

        TEST_ASSERT_EQUAL(i + 1, test_rpc_method_fn_fake.call_count);
        free(method_names[i]);
    }
}

int main(void)
{
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
