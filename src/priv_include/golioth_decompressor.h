/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "golioth_status.h"
#include "golioth_config.h"
#include <heatshrink_decoder.h>

typedef golioth_status_t (
        *golioth_decompressor_output_fn)(const uint8_t* bytes, size_t bytes_size, void* user_arg);

typedef struct {
    /// Number bytes input to golioth_decompressor_input(). Internal use only.
    int num_bytes_in;
    /// Number bytes output from golioth_decompressor_input(). Internal use only.
    int num_bytes_out;
#if CONFIG_GOLIOTH_OTA_DECOMPRESSION_ENABLE
    /// Decoder, contains buffer of size HEATSHRINK_STATIC_INPUT_BUFFER_SIZE.
    /// Internal use only.
    heatshrink_decoder hsd;
#endif
} golioth_decompressor_t;

/// Provide input bytes to decompressor for processing and forwarding to output_fn.
///
/// @param decompressor decompressor struct pointer
/// @param in_bytes input data buffer
/// @param in_bytes_size number of bytes in in_bytes data buffer
/// @param output_fn output function to call after decompression. Must be non-NULL.
/// @param output_fn_user_arg opaque data, forwarded untouched to output_fn. Can be NULL.
///
/// @return GOLIOTH_OK - decompressor successfully processed input data
/// @return Otherwise - decompression failed, or output_fn return not GOLIOTH_OK.
golioth_status_t golioth_decompressor_input(
        golioth_decompressor_t* decompressor,
        const uint8_t* in_bytes,
        size_t in_bytes_size,
        golioth_decompressor_output_fn output_fn,
        void* output_fn_user_arg);
