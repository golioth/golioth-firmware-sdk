/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <golioth/golioth_status.h>
#include <golioth/client.h>
#include <stdlib.h>

/// Start a gateway uplink. The pointer returned from this function can be used with
/// \ref golioth_gateway_uplink_block.
///
/// @param client The client handle from @ref golioth_client_create
struct blockwise_transfer *golioth_gateway_uplink_start(struct golioth_client *client);

/// Send a single uplink block
///
/// Call this function for each block. For each call you must increment the \p block_idx, On the
/// final block, set \p is_last to true. Block size is determined by the value of
/// CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE.
///
/// An optional callback and callback argument may be supplied. The callback will be called after
/// the block is uploaded to provide access to status and CoAP response codes.
///
/// @param ctx Block upload context used for all blocks in a related upload operation
/// @param block_idx The index of the block being sent
/// @param buf The buffer where the data for this block is located
/// @param buf_len The actual length of data (in bytes) for this block. This should be equal to
///        CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE for all blocks except for the final
///        block, which may be shorter
/// @param is_last Set this to true if this is the last block in the upload
/// @param set_cb A callback that will be called after each block is sent (can be NULL)
/// @param callback_arg An optional user provided argument that will be passed to \p callback (can
///        be NULL)
enum golioth_status golioth_gateway_uplink_block(struct blockwise_transfer *ctx,
                                                 uint32_t block_idx,
                                                 const uint8_t *buf,
                                                 size_t buf_len,
                                                 bool is_last,
                                                 golioth_set_block_cb_fn set_cb,
                                                 void *callback_arg);

/// Finish a gateway uplink.
///
/// @param ctx The uplink context to finish, returned from \ref golioth_gateway_uplink_block
void golioth_gateway_uplink_finish(struct blockwise_transfer *ctx);

#ifdef __cplusplus
}
#endif
