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

typedef enum golioth_status (*write_block_cb)(uint32_t offset,
                                              uint8_t *block_buffer,
                                              size_t block_size,
                                              bool is_last,
                                              void *callback_arg);

enum golioth_status golioth_blockwise_get(struct golioth_client *client,
                                          const char *path_prefix,
                                          const char *path,
                                          write_block_cb cb,
                                          void *callback_arg);
