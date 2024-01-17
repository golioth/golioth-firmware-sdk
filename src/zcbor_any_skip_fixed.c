/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <zcbor_decode.h>

/** Return value length from additional value.
 */
static size_t additional_len(uint8_t additional)
{
    if (ZCBOR_VALUE_IS_1_BYTE <= additional && additional <= ZCBOR_VALUE_IS_8_BYTES)
    {
        /* 24 => 1
         * 25 => 2
         * 26 => 4
         * 27 => 8
         */
        return 1U << (additional - ZCBOR_VALUE_IS_1_BYTE);
    }
    return 0;
}

/** Extract the major type, i.e. the first 3 bits of the header byte. */
#define MAJOR_TYPE(header_byte) ((zcbor_major_type_t) (((header_byte) >> 5) & 0x7))

/** Extract the additional info, i.e. the last 5 bits of the header byte. */
#define ADDITIONAL(header_byte) ((header_byte) &0x1F)


#define FAIL_AND_DECR_IF(expr, err) \
    do                              \
    {                               \
        if (expr)                   \
        {                           \
            (state->payload)--;     \
            ZCBOR_ERR(err);         \
        }                           \
    } while (0)


static bool initial_checks(zcbor_state_t *state)
{
    ZCBOR_CHECK_ERROR();
    ZCBOR_CHECK_PAYLOAD();
    return true;
}

#define INITIAL_CHECKS()            \
    do                              \
    {                               \
        if (!initial_checks(state)) \
        {                           \
            ZCBOR_FAIL();           \
        }                           \
    } while (0)


/** Get a single value.
 *
 * @details @p ppayload must point to the header byte. This function will
 *          retrieve the value (either from within the additional info, or from
 *          the subsequent bytes) and return it in the result. The result can
 *          have arbitrary length.
 *
 *          The function will also validate
 *           - Min/max constraints on the value.
 *           - That @p payload doesn't overrun past @p payload_end.
 *           - That @p elem_count has not been exhausted.
 *
 *          @p ppayload and @p elem_count are updated if the function
 *          succeeds. If not, they are left unchanged.
 *
 *          CBOR values are always big-endian, so this function converts from
 *          big to little-endian if necessary (@ref CONFIG_BIG_ENDIAN).
 */
static bool value_extract(zcbor_state_t *state, void *const result, size_t result_len)
{
    zcbor_trace();
    zcbor_assert_state(result_len != 0, "0-length result not supported.\r\n");
    zcbor_assert_state(result != NULL, NULL);

    INITIAL_CHECKS();
    ZCBOR_ERR_IF((state->elem_count == 0), ZCBOR_ERR_LOW_ELEM_COUNT);

    uint8_t *u8_result = (uint8_t *) result;
    uint8_t additional = ADDITIONAL(*state->payload);

    state->payload_bak = state->payload;
    (state->payload)++;

    memset(result, 0, result_len);
    if (additional <= ZCBOR_VALUE_IN_HEADER)
    {
#ifdef CONFIG_BIG_ENDIAN
        u8_result[result_len - 1] = additional;
#else
        u8_result[0] = additional;
#endif /* CONFIG_BIG_ENDIAN */
    }
    else
    {
        size_t len = additional_len(additional);

        FAIL_AND_DECR_IF(len > result_len, ZCBOR_ERR_INT_SIZE);
        FAIL_AND_DECR_IF(len == 0, ZCBOR_ERR_ADDITIONAL_INVAL);  // additional_len() did not
                                                                 // recognize the additional value.
        FAIL_AND_DECR_IF((state->payload + len) > state->payload_end, ZCBOR_ERR_NO_PAYLOAD);

#ifdef CONFIG_BIG_ENDIAN
        memcpy(&u8_result[result_len - len], state->payload, len);
#else
        for (size_t i = 0; i < len; i++)
        {
            u8_result[i] = (state->payload)[len - i - 1];
        }
#endif /* CONFIG_BIG_ENDIAN */

        (state->payload) += len;
    }

    (state->elem_count)--;
    return true;
}

static bool array_end_expect(zcbor_state_t *state)
{
    INITIAL_CHECKS();
    ZCBOR_ERR_IF(*state->payload != 0xFF, ZCBOR_ERR_WRONG_TYPE);

    state->payload++;
    return true;
}

static bool zcbor_array_at_end(zcbor_state_t *state)
{
    return ((!state->indefinite_length_array && (state->elem_count == 0))
            || (state->indefinite_length_array && (state->payload < state->payload_end)
                && (*state->payload == 0xFF)));
}

bool zcbor_any_skip_fixed(zcbor_state_t *state, void *result)
{
    zcbor_assert_state(result == NULL, "'any' type cannot be returned, only skipped.\r\n");
    (void) result;

    INITIAL_CHECKS();
    zcbor_major_type_t major_type = MAJOR_TYPE(*state->payload);
    uint8_t additional = ADDITIONAL(*state->payload);
    uint64_t value = 0; /* In case of indefinite_length_array. */
    zcbor_state_t state_copy;

    memcpy(&state_copy, state, sizeof(zcbor_state_t));

    while (major_type == ZCBOR_MAJOR_TYPE_TAG)
    {
        uint32_t tag_dummy;

        if (!zcbor_tag_decode(&state_copy, &tag_dummy))
        {
            ZCBOR_FAIL();
        }
        ZCBOR_ERR_IF(state_copy.payload >= state_copy.payload_end, ZCBOR_ERR_NO_PAYLOAD);
        major_type = MAJOR_TYPE(*state_copy.payload);
        additional = ADDITIONAL(*state_copy.payload);
    }

    const bool indefinite_length_array =
        ((additional == ZCBOR_VALUE_IS_INDEFINITE_LENGTH)
         && ((major_type == ZCBOR_MAJOR_TYPE_LIST) || (major_type == ZCBOR_MAJOR_TYPE_MAP)));

    if (!indefinite_length_array && !value_extract(&state_copy, &value, sizeof(value)))
    {
        /* Can happen because of elem_count (or payload_end) */
        ZCBOR_FAIL();
    }

    switch (major_type)
    {
        case ZCBOR_MAJOR_TYPE_BSTR:
        case ZCBOR_MAJOR_TYPE_TSTR:
            /* 'value' is the length of the BSTR or TSTR.
             * The cast to size_t is safe because value_extract() above
             * checks that payload_end is greater than payload. */
            ZCBOR_ERR_IF(value > (uint64_t) (state_copy.payload_end - state_copy.payload),
                         ZCBOR_ERR_NO_PAYLOAD);
            (state_copy.payload) += value;
            break;
        case ZCBOR_MAJOR_TYPE_MAP:
            ZCBOR_ERR_IF(value > (SIZE_MAX / 2), ZCBOR_ERR_INT_SIZE);
            value *= 2;
            /* fallthrough */
        case ZCBOR_MAJOR_TYPE_LIST:
            if (indefinite_length_array)
            {
                state_copy.payload++;
                value = ZCBOR_LARGE_ELEM_COUNT;
            }
            state_copy.elem_count = (uint_fast32_t) value;
            state_copy.indefinite_length_array = indefinite_length_array;
            while (!zcbor_array_at_end(&state_copy))
            {
                if (!zcbor_any_skip(&state_copy, NULL))
                {
                    ZCBOR_FAIL();
                }
            }
            if (indefinite_length_array && !array_end_expect(&state_copy))
            {
                ZCBOR_FAIL();
            }
            break;
        default:
            /* Do nothing */
            break;
    }

    state->payload = state_copy.payload;
    state->elem_count--;

    return true;
}
