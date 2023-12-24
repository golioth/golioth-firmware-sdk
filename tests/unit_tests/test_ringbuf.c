#include <unity.h>
#include <fff.h>
#include <stdint.h>

#include "ringbuf.h"

void setUp(void) {}
void tearDown(void) {}

void empty_ringbuf_has_zero_size(void) {
    RINGBUF_DEFINE(rb, 1, 8);
    TEST_ASSERT_EQUAL(0, ringbuf_size(&rb));
    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
}

void capacity_is_what_i_asked_for(void) {
    RINGBUF_DEFINE(rb, 1, 8);
    TEST_ASSERT_EQUAL(8, ringbuf_capacity(&rb));
}

void get_returns_the_oldest_item(void) {
    RINGBUF_DEFINE(rb, 1, 8);

    uint8_t item1 = 4;
    uint8_t item2 = 5;

    uint8_t item;
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item1));
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item2));
    TEST_ASSERT_TRUE(ringbuf_get(&rb, &item));
    TEST_ASSERT_EQUAL(4, item);
}

void get_when_empty_fails(void) {
    RINGBUF_DEFINE(rb, 1, 8);
    uint8_t item;
    TEST_ASSERT_FALSE(ringbuf_get(&rb, &item));
}

void put_when_full_fails(void) {
    const size_t maxitems = 8;
    RINGBUF_DEFINE(rb, 1, maxitems);

    uint8_t item = 0;
    for (size_t i = 0; i < maxitems; i++) {
        TEST_ASSERT_TRUE(ringbuf_put(&rb, &item));
    }
    TEST_ASSERT_TRUE(ringbuf_is_full(&rb));
    TEST_ASSERT_FALSE(ringbuf_put(&rb, &item));
}

void can_peek(void) {
    RINGBUF_DEFINE(rb, 1, 8);

    uint8_t item1 = 4;
    uint8_t item2 = 5;

    uint8_t item;
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item1));
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item2));
    TEST_ASSERT_TRUE(ringbuf_peek(&rb, &item));
    TEST_ASSERT_EQUAL(4, item);
    TEST_ASSERT_EQUAL(2, ringbuf_size(&rb));
}

void can_reset(void) {
    RINGBUF_DEFINE(rb, 1, 8);
    uint8_t item = 0;
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item));
    TEST_ASSERT_EQUAL(1, ringbuf_size(&rb));
    ringbuf_reset(&rb);
    TEST_ASSERT_EQUAL(0, ringbuf_size(&rb));
}

void array_wraparound(void) {
    // Verify the size is reported correctly when the write pointer wraps
    // around in the internal array.
    //
    // To force wraparound, fill the ringbuf, read an item, then write an item
    RINGBUF_DEFINE(rb, 1, 2);
    uint8_t item1 = 1;
    uint8_t item2 = 2;
    uint8_t item3 = 3;
    uint8_t rd_item;
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item1));
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item2));
    TEST_ASSERT_TRUE(ringbuf_is_full(&rb));
    TEST_ASSERT_TRUE(ringbuf_get(&rb, &rd_item));
    TEST_ASSERT_EQUAL(1, rd_item);
    TEST_ASSERT_TRUE(ringbuf_put(&rb, &item3));
    TEST_ASSERT_TRUE(ringbuf_get(&rb, &rd_item));
    TEST_ASSERT_EQUAL(2, rd_item);
    TEST_ASSERT_TRUE(ringbuf_get(&rb, &rd_item));
    TEST_ASSERT_EQUAL(3, rd_item);
    TEST_ASSERT_TRUE(ringbuf_is_empty(&rb));
}

void put_when_null_item_fails(void) {
    RINGBUF_DEFINE(rb, 1, 1);
    TEST_ASSERT_FALSE(ringbuf_put(&rb, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(empty_ringbuf_has_zero_size);
    RUN_TEST(capacity_is_what_i_asked_for);
    RUN_TEST(get_returns_the_oldest_item);
    RUN_TEST(get_when_empty_fails);
    RUN_TEST(put_when_full_fails);
    RUN_TEST(put_when_null_item_fails);
    RUN_TEST(array_wraparound);
    RUN_TEST(can_peek);
    RUN_TEST(can_reset);
    return UNITY_END();
}
