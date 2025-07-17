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

struct gateway_uplink;

typedef enum golioth_status (*gateway_downlink_block_cb)(const uint8_t *data,
                                                         size_t len,
                                                         bool is_last,
                                                         void *arg);

typedef void (*gateway_downlink_end_cb)(enum golioth_status status,
                                        const struct golioth_coap_rsp_code *coap_rsp_code,
                                        void *arg);

/// Start a gateway uplink. The pointer returned from this function can be used with
/// \ref golioth_gateway_uplink_block.
///
/// @param client The client handle from @ref golioth_client_create
struct gateway_uplink *golioth_gateway_uplink_start(struct golioth_client *client,
                                                    gateway_downlink_block_cb dnlk_block_cb,
                                                    gateway_downlink_end_cb dnlk_end_cb,
                                                    void *downlink_arg);

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
enum golioth_status golioth_gateway_uplink_block(struct gateway_uplink *uplink,
                                                 uint32_t block_idx,
                                                 const uint8_t *buf,
                                                 size_t buf_len,
                                                 bool is_last,
                                                 golioth_set_block_cb_fn set_cb,
                                                 void *callback_arg);

/// Finish a gateway uplink.
///
/// @param ctx The uplink context to finish, returned from \ref golioth_gateway_uplink_block
void golioth_gateway_uplink_finish(struct gateway_uplink *uplink);

/// Get server certificate.
///
/// @param client The client handle from @ref golioth_client_create
/// @param buf Pointer to buffer where certificate will be written
/// @param len Pointer to length. On input it is buffer length. On output it is certificate length.
/// @param timeout_s Timeout in seconds for API call
enum golioth_status golioth_gateway_server_cert_get(struct golioth_client *client,
                                                    void *buf,
                                                    size_t *len,
                                                    int32_t timeout_s);

#ifdef __cplusplus
}
#endif
