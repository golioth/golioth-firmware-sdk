/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <golioth/client.h>
#include <golioth/golioth_status.h>

struct blockwise_transfer;

/* Blockwise Upload */
typedef enum golioth_status (*read_block_cb)(uint32_t block_idx,
                                             uint8_t *block_buffer,
                                             size_t *block_size,
                                             bool *is_last,
                                             void *callback_arg);

enum golioth_status golioth_blockwise_post(struct golioth_client *client,
                                           const char *path_prefix,
                                           const char *path,
                                           enum golioth_content_type content_type,
                                           read_block_cb cb,
                                           golioth_set_cb_fn callback,
                                           void *callback_arg);

/* Blockwise Multi-Part Upload */
struct blockwise_transfer *golioth_blockwise_upload_start(struct golioth_client *client,
                                                          const char *path_prefix,
                                                          const char *path,
                                                          enum golioth_content_type content_type);

void golioth_blockwise_upload_finish(struct blockwise_transfer *ctx);

enum golioth_status golioth_blockwise_upload_block(struct blockwise_transfer *ctx,
                                                   uint32_t block_idx,
                                                   const uint8_t *block_buffer,
                                                   size_t block_len,
                                                   bool is_last,
                                                   golioth_set_block_cb_fn set_cb,
                                                   void *callback_arg,
                                                   bool is_synchronous,
                                                   int32_t timeout_s);

/* Blockwise Download */
typedef enum golioth_status (*write_block_cb)(uint32_t block_idx,
                                              const uint8_t *block_buffer,
                                              size_t block_buffer_len,
                                              bool is_last,
                                              size_t negotiated_block_size,
                                              void *callback_arg);

/* Begin a blockwise download from a given block index
 *
 * This function will block until the download finishes or an error has occurred.
 * - The block_idx parameter out will be set to the next expected block.
 * - This value may be used as input to resume after a block download has failed.
 * - block_idx may be NULL, in which case 0 will be used for first block_idx and no value will be
 *   passed out.
 */
enum golioth_status golioth_blockwise_get(struct golioth_client *client,
                                          const char *path_prefix,
                                          const char *path,
                                          enum golioth_content_type content_type,
                                          uint32_t *block_idx,
                                          write_block_cb cb,
                                          void *callback_arg);
