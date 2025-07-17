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

#include <golioth/client.h>
#include <golioth/golioth_status.h>
#include <stdint.h>
#include <zcbor_encode.h>

/**
 * @brief Network information, used with golioth_net_info_create(), golioth_net_info_destroy(),
 * golioth_net_info_finish(), golioth_net_info_get_buf(), golioth_net_info_get_buf_len()
 */
struct golioth_net_info;

/// Create network information
///
/// @return pointer to golioth_net_info struct
/// @return NULL - Error initializing
struct golioth_net_info *golioth_net_info_create();

/// Destroy network information
///
/// Free the network information after no longer in use.
///
/// @param info pointer to golioth_net_info struct
///
/// @return GOLIOTH_OK - Network information successfully deinitialized
/// @return otherwise - Error deinitializing the network information
enum golioth_status golioth_net_info_destroy(struct golioth_net_info *info);

/// Finish building network information
///
/// Needs to be called after filling data with golioth_net_info_*_append() APIs
///
/// @param info Network information
///
/// @retval GOLIOTH_OK Network information encoding finished successfully
/// @retval GOLIOTH_ERR_NULL No network information encoded within request
/// @retval GOLIOTH_ERR_MEM_ALLOC Not enough memory in network information buffer
enum golioth_status golioth_net_info_finish(struct golioth_net_info *info);

/// Get serialized CBOR network information
///
/// Needs to be called after filling data with golioth_net_info_*_append() APIs, and finishing with
/// golioth_net_info_finish()
///
/// @param info Network information
///
/// @return buffer containing serialized network information
const uint8_t *golioth_net_info_get_buf(const struct golioth_net_info *info);

/// Get length of serialized CBOR network information
///
/// Needs to be called after filling data with golioth_net_info_*_append() APIs, and finishing with
/// golioth_net_info_finish()
///
/// @param info Network information
///
/// @return length of buffer containing serialized network information
size_t golioth_net_info_get_buf_len(const struct golioth_net_info *info);

#ifdef __cplusplus
}
#endif
