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
                                                   golioth_get_block_cb_fn get_cb,
                                                   golioth_end_block_cb_fn end_cb,
                                                   void *callback_arg,
                                                   void *rsp_callback_arg,
                                                   int32_t timeout_s);

/* Blockwise Download */

/* Begin a blockwise download from a given block index
 *
 * This function is non-blocking. If the return value is GOLIOTH_OK, then
 * end_cb will be called exactly one time. block_cb may be called 0 or more
 * times. Blockwise downloads may be resumed by passing in a non-zero value
 * for block_idx.
 */
enum golioth_status golioth_blockwise_get(struct golioth_client *client,
                                          const char *path_prefix,
                                          const char *path,
                                          enum golioth_content_type content_type,
                                          uint32_t block_idx,
                                          golioth_get_block_cb_fn block_cb,
                                          golioth_end_block_cb_fn end_cb,
                                          void *callback_arg);
