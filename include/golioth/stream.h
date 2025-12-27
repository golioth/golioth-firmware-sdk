/*
 * Copyright (c) 2023 Golioth, Inc.
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

/// @defgroup golioth_stream golioth_stream
/// Functions for interacting with Golioth Stream service.
///
/// https://docs.golioth.io/reference/protocols/coap/stream
/// @{

/// Set an object in stream at a particular path asynchronously
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. Optionally, the user may supply
/// a callback that will be called when the response is received (indicating
/// the request was acknowledged by the server) or a timeout occurs (response
/// never received).
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in stream to set (e.g. "my_obj")
/// @param content_type The serialization format of buf
/// @param buf A buffer containing the object to send
/// @param buf_len Length of buf
/// @param callback Callback to call on response received or timeout. Can be NULL.
/// @param callback_arg Callback argument, passed directly when callback invoked. Can be NULL.
///
/// @retval GOLIOTH_OK request enqueued
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_MEM_ALLOC memory allocation error
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
enum golioth_status golioth_stream_set(struct golioth_client *client,
                                       const char *path,
                                       enum golioth_content_type content_type,
                                       const uint8_t *buf,
                                       size_t buf_len,
                                       golioth_set_cb_fn callback,
                                       void *callback_arg);

/// Read block callback
///
/// This callback will be called by the Golioth client each time it needs to
/// fill a block.
///
/// When the callback runs, the buffer should be filled with `block_size` number
/// of bytes. When the last block contains fewer bytes, set `block_size` to the
/// actual number of bytes written. Set `is_last` to indicate all data has been
/// written to buffer.
///
/// @param block_idx The index of the block to fill
/// @param block_buffer The buffer that this callback should fill
/// @param block_size (in/out) Contains the maximum size of block_buffer, and
///        should be set to the size of data actually placed in block_buffer.
///        Value must be > 0 when this function returns GOLIOTH_OK.
/// @param is_last (out) Set this to true if this is the last block to transfer
/// @param arg A user provided argument
///
/// @retval GOLIOTH_OK Callback ran successfully
/// @retval GOLIOTH_ERR_* Indicate error type encountered by callback
typedef enum golioth_status (*stream_read_block_cb)(uint32_t block_idx,
                                                    uint8_t *block_buffer,
                                                    size_t *block_size,
                                                    bool *is_last,
                                                    void *arg);

/// Set an object in stream at a particular path synchronously
///
/// This function will block until the whole transfer is complete, or
/// errors out.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in stream to set (e.g. "my_obj")
/// @param content_type The content type of the object (e.g. JSON or CBOR)
/// @param cb A callback that will be used to fill each block in the transfer
/// @param arg An optional user provided argument that will be passed to cb
enum golioth_status golioth_stream_set_blockwise_sync(struct golioth_client *client,
                                                      const char *path,
                                                      enum golioth_content_type content_type,
                                                      stream_read_block_cb cb,
                                                      void *arg);

/// Create a multipart blockwise upload context
///
/// Creates the context and returns a pointer to it. This context is used to associate all blocks of
/// a multipart upload together. A new context is needed at the start of each multipart blockwise
/// upload operation.
///
/// @param client The client handle from @ref golioth_client_create
/// @param path The path in stream to set (e.g. "my_obj")
/// @param content_type The content type of the object (e.g. JSON or CBOR)
struct blockwise_transfer *golioth_stream_blockwise_start(struct golioth_client *client,
                                                          const char *path,
                                                          enum golioth_content_type content_type);

/// Destroy a multi-block upload context
///
/// Free the memory allocated for a given multi-block upload context.
///
/// @param ctx Block upload context to be destroyed
void golioth_stream_blockwise_finish(struct blockwise_transfer *ctx);

/// Set an object in stream at a particular path by sending each block
///
/// Call this function for each block. For each call you must increment the \p block_idx, adjust the
/// \p block_buffer pointer and update the \p data_len. On the final block, set \p is_last to true.
/// Block size is determined by the value of CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE.
///
/// Create a new \p ctx by calling \ref golioth_stream_blockwise_start. The same \p ctx must be
/// used for all blocks in a related upload. Generate a new \p ctx for each new upload operation.
/// Free the context memory by calling \ref golioth_stream_blockwise_finish.
///
/// An optional callback and callback argument may be supplied. The callback will be called after
/// the block is uploaded to provide access to status and CoAP response codes.
///
/// @param ctx Block upload context used for all blocks in a related upload operation
/// @param block_idx The index of the block being sent
/// @param block_buffer The buffer where the data for this block is located
/// @param data_len The actual length of data (in bytes) for this block. This should be equal to
///        CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE for all blocks except for the final
///        block, which may be shorter
/// @param is_last Set this to true if this is the last block in the upload
/// @param callback A callback that will be called after each block is sent (can be NULL)
/// @param callback_arg An optional user provided argument that will be passed to \p callback (can
///        be NULL)
enum golioth_status golioth_stream_blockwise_set_block(struct blockwise_transfer *ctx,
                                                       uint32_t block_idx,
                                                       const uint8_t *block_buffer,
                                                       size_t data_len,
                                                       bool is_last,
                                                       golioth_set_block_cb_fn callback,
                                                       void *callback_arg);

/// @}

#ifdef __cplusplus
}
#endif
