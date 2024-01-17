/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zcbor_decode.h>

/** Extract the major type, i.e. the first 3 bits of the header byte. */
#define ZCBOR_MAJOR_TYPE(header_byte) ((zcbor_major_type_t) (((header_byte) >> 5) & 0x7))

#if defined(ZCBOR_VERSION_MAJOR) && defined(ZCBOR_VERSION_MINOR)
#if ZCBOR_VERSION_MAJOR == 0 && ZCBOR_VERSION_MINOR < 7
#define ZCBOR_MAJOR_TYPE_SIMPLE ZCBOR_MAJOR_TYPE_PRIM
#endif
#endif

enum {
    ZCBOR_MAP_KEY_TYPE_U32,
    ZCBOR_MAP_KEY_TYPE_TSTR,
};

struct zcbor_map_key {
    uint8_t type;
    union {
        uint32_t u32;
        struct zcbor_string tstr;
    };
};

struct zcbor_map_entry {
    struct zcbor_map_key key;
    int (*decode)(zcbor_state_t *zsd, void *value);
    void *value;
};

/**
 * @brief Check the end of CBOR list or map
 *
 * @retval true   Reached end of list or map
 * @retval false  There are more items (from list of map) to be processed
 */
static inline bool zcbor_list_or_map_end(zcbor_state_t *state) {
    if (state->indefinite_length_array) {
        return *state->payload == 0xff;
    }

    return state->elem_count == 0;
}

/**
 * @brief Decode int64_t value from CBOR map
 *
 * Callback for decoding int64_t value from CBOR map using zcbor_map_decode().
 *
 * @param[inout] zsd    The current state of the decoding
 * @param[out]   value  Pointer to int64_t value where result of decoding is saved.
 *
 * @retval  0  On success
 * @retval <0  POSIX error code on error
 */
int zcbor_map_int64_decode(zcbor_state_t *zsd, void *value);

/**
 * @brief Decode text string value from CBOR map
 *
 * Callback for decoding tstr (struct zcbor_string) value from CBOR map using zcbor_map_decode().
 *
 * @param[inout] zsd    The current state of the decoding
 * @param[out]   value  Pointer to struct 'struct zcbor_string' value where result of decoding is
 *                      saved.
 *
 * @retval  0  On success
 * @retval <0  POSIX error code on error
 */
int zcbor_map_tstr_decode(zcbor_state_t *zsd, void *value);

/**
 * @brief Decode CBOR map with specified entries
 *
 * Decode CBOR map with entries specified by @a entries. All specified entries need to exist in
 * processed CBOR map.
 *
 * @param[inout] zsd          The current state of the decoding
 * @param[in]    entries      Array with entries to be decoded
 * @param[in]    num_entries  Number of entries (size of @a entries array)
 *
 * @retval  0       On success
 * @retval -EBADMSG Failed to parse all map entries
 * @retval -ENOENT  Map was empty
 * @retval <0       Other error returned from @ entries decode callback
 */
int zcbor_map_decode(zcbor_state_t *zsd, struct zcbor_map_entry *entries, size_t num_entries);

/**
 * @brief Define CBOR map entry to be decoded, referenced by uint32_t key
 *
 * @param _u32     Map key
 * @param _decode  Map value decode callback
 * @param _value   Value passed to decode callback
 */
#define ZCBOR_U32_MAP_ENTRY(_u32, _decode, _value) \
    {                                              \
        .key =                                     \
            {                                      \
                .type = ZCBOR_MAP_KEY_TYPE_U32,    \
                .u32 = _u32,                       \
            },                                     \
        .decode = _decode, .value = _value,        \
    }

/**
 * @brief Define CBOR map entry to be decoded, referenced by literal string key
 *
 * @param _tstr_lit  Map key (literal string, e.g. "my_key")
 * @param _decode    Map value decode callback
 * @param _value     Value passed to decode callback
 */
#define ZCBOR_TSTR_LIT_MAP_ENTRY(_tstr_lit, _decode, _value)                  \
    {                                                                         \
        .key =                                                                \
            {                                                                 \
                .type = ZCBOR_MAP_KEY_TYPE_TSTR,                              \
                .tstr = {(const uint8_t *) _tstr_lit, sizeof(_tstr_lit) - 1}, \
            },                                                                \
        .decode = _decode, .value = _value,                                   \
    }
