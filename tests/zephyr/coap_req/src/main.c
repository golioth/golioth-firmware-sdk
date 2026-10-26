/*
 * Copyright (c) 2026 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <zephyr/ztest.h>

#include <golioth/golioth_status.h>

#include "coap_client_zephyr.h"
#include "zephyr_coap_req.h"

/* Mirrors COAP_SEPARATE_TIMEOUT_MS in zephyr_coap_req.c */
#define SEPARATE_RESPONSE_TIMEOUT_MS 6000

/* Past the first retransmission deadline of a request, but before the second one */
#define AFTER_FIRST_RETRANSMISSION_MS 3500

#define MAX_INITIAL_ACK_TIMEOUT_MS \
    (CONFIG_COAP_INIT_ACK_TIMEOUT_MS * CONFIG_COAP_ACK_RANDOM_PERCENT / 100)

BUILD_ASSERT(MAX_INITIAL_ACK_TIMEOUT_MS < AFTER_FIRST_RETRANSMISSION_MS);
BUILD_ASSERT(3 * CONFIG_COAP_INIT_ACK_TIMEOUT_MS > AFTER_FIRST_RETRANSMISSION_MS);

#define SEPARATE_RESPONSE_MID 0x4242
#define NO_OBSERVE -1

static const uint8_t token_a[GOLIOTH_COAP_TOKEN_LEN] =
    {0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8};
static const uint8_t token_b[GOLIOTH_COAP_TOKEN_LEN] =
    {0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8};

static const uint8_t *observe_path[] = {(const uint8_t *) "observed", NULL};

struct rsp_record
{
    int calls;
    enum golioth_status status;
    struct golioth_coap_rsp_code code;
};

static struct golioth_client client;
static struct rsp_record rec[2];

static struct
{
    int count;
    uint16_t last_mid;
} sent;

int __wrap_golioth_send_coap(struct golioth_client *c, struct coap_packet *packet);

int __wrap_golioth_send_coap(struct golioth_client *c, struct coap_packet *packet)
{
    ARG_UNUSED(c);

    sent.count++;
    sent.last_mid = coap_header_get_id(packet);

    return 0;
}

static int record_rsp(struct golioth_req_rsp *rsp)
{
    struct rsp_record *r = rsp->user_data;

    r->calls++;
    r->status = rsp->status;
    r->code = rsp->coap_rsp_code;

    return 0;
}

/* Schedules a CON POST and lets the request layer transmit it; returns its Message ID */
static uint16_t send_request(const uint8_t *token, struct rsp_record *r, uint32_t now)
{
    struct golioth_coap_req *req;
    int sent_before = sent.count;

    zassert_ok(golioth_coap_req_new(&req,
                                    &client,
                                    token,
                                    COAP_METHOD_POST,
                                    COAP_TYPE_CON,
                                    64,
                                    record_rsp,
                                    r));
    zassert_ok(golioth_coap_req_schedule(req));

    (void) golioth_coap_reqs_poll_prepare(&client, now);
    zassert_equal(sent.count, sent_before + 1, "request was not transmitted");

    return sent.last_mid;
}

/* Registers an observation of observe_path with token_b; returns the registration Message ID */
static uint16_t start_observation(struct rsp_record *r, uint32_t now)
{
    int sent_before = sent.count;

    zassert_ok(golioth_coap_req_cb(&client,
                                   token_b,
                                   COAP_METHOD_GET,
                                   observe_path,
                                   COAP_CONTENT_FORMAT_APP_JSON,
                                   NULL,
                                   0,
                                   record_rsp,
                                   r,
                                   GOLIOTH_COAP_REQ_OBSERVE));

    (void) golioth_coap_reqs_poll_prepare(&client, now);
    zassert_equal(sent.count, sent_before + 1, "registration was not transmitted");

    return sent.last_mid;
}

static void receive_observe(uint8_t type,
                            uint8_t code,
                            uint16_t mid,
                            const uint8_t *token,
                            uint8_t tkl,
                            int observe)
{
    uint8_t buf[48];
    struct coap_packet tx;
    struct coap_packet rx;

    zassert_ok(
        coap_packet_init(&tx, buf, sizeof(buf), COAP_VERSION_1, type, tkl, token, code, mid));
    if (observe != NO_OBSERVE)
    {
        zassert_ok(coap_append_option_int(&tx, COAP_OPTION_OBSERVE, observe));
    }
    zassert_ok(coap_packet_parse(&rx, buf, tx.offset, NULL, 0));

    golioth_coap_req_process_rx(&client, &rx);
}

static void receive(uint8_t type, uint8_t code, uint16_t mid, const uint8_t *token, uint8_t tkl)
{
    receive_observe(type, code, mid, token, tkl, NO_OBSERVE);
}

static void coap_req_before(void *fixture)
{
    ARG_UNUSED(fixture);

    memset(&client, 0, sizeof(client));
    memset(rec, 0, sizeof(rec));
    memset(&sent, 0, sizeof(sent));

    client.sock = -1;
    client.rx_buffer_len = 1280;
    golioth_coap_reqs_init(&client);
    golioth_coap_reqs_on_connect(&client);
}

static void coap_req_after(void *fixture)
{
    ARG_UNUSED(fixture);

    golioth_coap_reqs_on_disconnect(&client);
}

ZTEST_SUITE(coap_req, NULL, NULL, coap_req_before, coap_req_after, NULL);

/* Consecutive blocks of a blockwise transfer share one token, so the server's replay of
 * the ACK for block N-1 carries the token of block N as well. */
ZTEST(coap_req, test_duplicate_ack_does_not_complete_next_request)
{
    uint32_t now = k_uptime_get_32();

    uint16_t mid1 = send_request(token_a, &rec[0], now);
    receive(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CONTINUE, mid1, token_a, sizeof(token_a));
    zassert_equal(rec[0].calls, 1);

    uint16_t mid2 = send_request(token_a, &rec[1], now);
    receive(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CONTINUE, mid1, token_a, sizeof(token_a));
    zassert_equal(rec[1].calls, 0, "duplicate ACK of the previous request completed this one");

    receive(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CHANGED, mid2, token_a, sizeof(token_a));
    zassert_equal(rec[1].calls, 1);
    zassert_equal(rec[1].status, GOLIOTH_OK);
    zassert_equal(rec[1].code.code_class, 2);
    zassert_equal(rec[1].code.code_detail, 4);
}

ZTEST(coap_req, test_ack_without_token_does_not_match_request_with_token)
{
    uint16_t mid = send_request(token_a, &rec[0], k_uptime_get_32());

    receive(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CONTENT, mid, NULL, 0);
    zassert_equal(rec[0].calls, 0);
}

ZTEST(coap_req, test_ack_with_other_token_does_not_match)
{
    uint16_t mid = send_request(token_a, &rec[0], k_uptime_get_32());

    receive(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CONTENT, mid, token_b, sizeof(token_b));
    zassert_equal(rec[0].calls, 0);
}

ZTEST(coap_req, test_request_is_not_a_response)
{
    send_request(token_a, &rec[0], k_uptime_get_32());

    receive(COAP_TYPE_CON, COAP_METHOD_GET, SEPARATE_RESPONSE_MID, token_a, sizeof(token_a));
    zassert_equal(rec[0].calls, 0);
}

ZTEST(coap_req, test_empty_ack_then_separate_response)
{
    uint32_t now = k_uptime_get_32();
    uint16_t mid = send_request(token_a, &rec[0], now);

    receive(COAP_TYPE_ACK, COAP_CODE_EMPTY, mid, NULL, 0);
    zassert_equal(rec[0].calls, 0, "empty ACK was treated as the response");

    (void) golioth_coap_reqs_poll_prepare(&client, now + AFTER_FIRST_RETRANSMISSION_MS);
    zassert_equal(sent.count, 1, "acknowledged request was retransmitted");
    zassert_equal(rec[0].calls, 0);

    receive(COAP_TYPE_CON,
            COAP_RESPONSE_CODE_CONTENT,
            SEPARATE_RESPONSE_MID,
            token_a,
            sizeof(token_a));
    zassert_equal(rec[0].calls, 1);
    zassert_equal(rec[0].status, GOLIOTH_OK);
    zassert_equal(rec[0].code.code_class, 2);
    zassert_equal(rec[0].code.code_detail, 5);
}

ZTEST(coap_req, test_separate_response_times_out)
{
    uint32_t now = k_uptime_get_32();
    uint16_t mid = send_request(token_a, &rec[0], now);

    receive(COAP_TYPE_ACK, COAP_CODE_EMPTY, mid, NULL, 0);

    (void) golioth_coap_reqs_poll_prepare(&client, now + SEPARATE_RESPONSE_TIMEOUT_MS + 1000);
    zassert_equal(rec[0].calls, 1);
    zassert_equal(rec[0].status, GOLIOTH_ERR_TIMEOUT);
    zassert_equal(sent.count, 1, "acknowledged request was retransmitted");
}

ZTEST(coap_req, test_separate_response_wait_starts_at_empty_ack)
{
    uint16_t mid = send_request(token_a, &rec[0], k_uptime_get_32());

    k_sleep(K_MSEC(AFTER_FIRST_RETRANSMISSION_MS));
    uint32_t now = k_uptime_get_32();
    (void) golioth_coap_reqs_poll_prepare(&client, now);
    zassert_equal(sent.count, 2, "request was not retransmitted");

    receive(COAP_TYPE_ACK, COAP_CODE_EMPTY, mid, NULL, 0);

    (void) golioth_coap_reqs_poll_prepare(&client, now + SEPARATE_RESPONSE_TIMEOUT_MS - 100);
    zassert_equal(rec[0].calls, 0, "wait for the separate response did not start at its ACK");
    zassert_equal(sent.count, 2, "acknowledged request was retransmitted");
}

ZTEST(coap_req, test_separate_response_matches_by_token)
{
    send_request(token_a, &rec[0], k_uptime_get_32());

    receive(COAP_TYPE_CON,
            COAP_RESPONSE_CODE_CONTENT,
            SEPARATE_RESPONSE_MID,
            token_a,
            sizeof(token_a));
    zassert_equal(rec[0].calls, 1);
    zassert_equal(rec[0].status, GOLIOTH_OK);
}

ZTEST(coap_req, test_reset_fails_request_with_same_message_id)
{
    uint16_t mid = send_request(token_a, &rec[0], k_uptime_get_32());

    receive(COAP_TYPE_RESET, COAP_CODE_EMPTY, mid, NULL, 0);
    zassert_equal(rec[0].calls, 1);
    zassert_equal(rec[0].status, GOLIOTH_ERR_COAP_RESPONSE);
    zassert_equal(rec[0].code.code_class, 0);
    zassert_equal(rec[0].code.code_detail, 0);
}

ZTEST(coap_req, test_reset_with_other_message_id_is_ignored)
{
    uint16_t mid = send_request(token_a, &rec[0], k_uptime_get_32());

    receive(COAP_TYPE_RESET, COAP_CODE_EMPTY, mid + 1, NULL, 0);
    zassert_equal(rec[0].calls, 0);
}

/* A Reset that is not Empty is a message format error and is silently ignored */
ZTEST(coap_req, test_reset_with_code_is_ignored)
{
    uint16_t mid = send_request(token_a, &rec[0], k_uptime_get_32());

    receive(COAP_TYPE_RESET, COAP_RESPONSE_CODE_CONTENT, mid, token_a, sizeof(token_a));
    zassert_equal(rec[0].calls, 0);
}

ZTEST(coap_req, test_observe_notifications_delivered_in_order)
{
    uint16_t mid = start_observation(&rec[0], k_uptime_get_32());

    receive_observe(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CONTENT, mid, token_b, sizeof(token_b), 5);
    zassert_equal(rec[0].calls, 1);
    zassert_equal(rec[0].status, GOLIOTH_OK);

    receive_observe(COAP_TYPE_NON_CON,
                    COAP_RESPONSE_CODE_CONTENT,
                    0x2001,
                    token_b,
                    sizeof(token_b),
                    6);
    zassert_equal(rec[0].calls, 2, "NON notification was not delivered");

    receive_observe(COAP_TYPE_NON_CON,
                    COAP_RESPONSE_CODE_CONTENT,
                    0x2002,
                    token_b,
                    sizeof(token_b),
                    4);
    zassert_equal(rec[0].calls, 2, "out-of-order notification was delivered");

    receive_observe(COAP_TYPE_CON, COAP_RESPONSE_CODE_CONTENT, 0x2003, token_b, sizeof(token_b), 7);
    zassert_equal(rec[0].calls, 3, "CON notification was not delivered");
}

ZTEST(coap_req, test_stale_acks_leave_observation_established)
{
    uint32_t now = k_uptime_get_32();
    uint16_t mid = start_observation(&rec[0], now);

    receive_observe(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CONTENT, mid, token_b, sizeof(token_b), 5);
    zassert_equal(rec[0].calls, 1);

    receive_observe(COAP_TYPE_ACK, COAP_RESPONSE_CODE_CONTENT, mid, token_b, sizeof(token_b), 5);
    receive(COAP_TYPE_ACK, COAP_CODE_EMPTY, mid, NULL, 0);
    zassert_equal(rec[0].calls, 1);

    (void) golioth_coap_reqs_poll_prepare(&client, now + 10 * SEPARATE_RESPONSE_TIMEOUT_MS);
    zassert_equal(sent.count, 1, "established observation was retransmitted");
    zassert_equal(rec[0].calls, 1, "established observation timed out");

    receive_observe(COAP_TYPE_NON_CON,
                    COAP_RESPONSE_CODE_CONTENT,
                    0x2001,
                    token_b,
                    sizeof(token_b),
                    6);
    zassert_equal(rec[0].calls, 2, "observation was lost");
}

ZTEST(coap_req, test_observe_registration_with_separate_response)
{
    uint32_t now = k_uptime_get_32();
    uint16_t mid = start_observation(&rec[0], now);

    receive(COAP_TYPE_ACK, COAP_CODE_EMPTY, mid, NULL, 0);
    zassert_equal(rec[0].calls, 0);

    receive_observe(COAP_TYPE_CON, COAP_RESPONSE_CODE_CONTENT, 0x3001, token_b, sizeof(token_b), 2);
    zassert_equal(rec[0].calls, 1);
    zassert_equal(rec[0].status, GOLIOTH_OK);

    (void) golioth_coap_reqs_poll_prepare(&client, now + 10 * SEPARATE_RESPONSE_TIMEOUT_MS);
    zassert_equal(rec[0].calls, 1, "established observation timed out");

    receive_observe(COAP_TYPE_NON_CON,
                    COAP_RESPONSE_CODE_CONTENT,
                    0x3002,
                    token_b,
                    sizeof(token_b),
                    3);
    zassert_equal(rec[0].calls, 2);
}
