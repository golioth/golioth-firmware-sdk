/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "golioth_decompressor.h"
#include "golioth_sys.h"
#include "golioth_config.h"
#include <assert.h>

#define TAG "decompressoror"

#define HEATSHRINK_DECODE_BUFFER_SIZE 512

golioth_status_t golioth_decompressor_input(
        golioth_decompressor_t* decompressor,
        const uint8_t* in_bytes,
        size_t in_bytes_size,
        golioth_decompressor_output_fn output_fn,
        void* output_fn_user_arg) {
    assert(output_fn);

#if CONFIG_GOLIOTH_OTA_DECOMPRESSION_ENABLE == 0
    // Decompression is disabled, so forward directly to output callback
    return output_fn(in_bytes, in_bytes_size, output_fn_user_arg);
#else
    heatshrink_decoder* hsd = &decompressor->hsd;

    decompressor->num_bytes_in += in_bytes_size;

    // Sink the compressed data
    size_t sunk = 0;
    HSD_sink_res sink_res = heatshrink_decoder_sink(hsd, (uint8_t*)in_bytes, in_bytes_size, &sunk);
    if (sink_res != HSDR_SINK_OK) {
        GLTH_LOGE(TAG, "sink error: %d, sunk = %" PRIu32, sink_res, (uint32_t)sunk);
    }

    // Pull the uncompressed data out of the decoder
    HSD_poll_res pres;
    uint8_t decode_buffer[HEATSHRINK_DECODE_BUFFER_SIZE];
    do {
        size_t poll_sz = 0;
        pres = heatshrink_decoder_poll(hsd, decode_buffer, sizeof(decode_buffer), &poll_sz);
        if (pres < 0) {
            GLTH_LOGE(TAG, "poll error: %d", pres);
        }

        golioth_status_t status = output_fn(decode_buffer, poll_sz, output_fn_user_arg);
        if (status != GOLIOTH_OK) {
            return status;
        }

        decompressor->num_bytes_out += poll_sz;
    } while (pres == HSDR_POLL_MORE);

    return GOLIOTH_OK;
#endif
}
