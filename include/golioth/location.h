/*
 * Copyright (c) 2024 Golioth, Inc.
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

struct golioth_location_rsp
{
    /** Latitudinal position in nanodegrees (0 to +-180E9) */
    int64_t latitude;
    /** Longitudinal position in nanodegrees (0 to +-180E9) */
    int64_t longitude;

    /** Accuracy in meters */
    int64_t accuracy;
};

#define GOLIOTH_LOCATION_FLAG_WIFI (1 << 0)
#define GOLIOTH_LOCATION_FLAG_CELLULAR (1 << 1)

/**
 * @brief Wi-Fi location request, used with golioth_location_init(), golioth_location_finish() and
 *        golioth_location_get_sync()
 */
struct golioth_location_req
{
    uint8_t buf[CONFIG_GOLIOTH_LOCATION_REQUEST_BUFFER_SIZE];
    zcbor_state_t zse[2 /* backup */ + 1 /* WiFi only */];
    int flags;
};

/// Initialize location request
///
/// Needs to be called before filling data with golioth_location_*_append() APIs.
///
/// @param req Location request
void golioth_location_init(struct golioth_location_req *req);

/// Finish building location request
///
/// Needs to be called after filling data with golioth_location_*_append() APIs and before
/// requesting location with golioth_location_get_sync().
///
/// @param req Location request
///
/// @retval GOLIOTH_OK Location request finished successfully
/// @retval GOLIOTH_ERR_NULL No location information encoded within request
/// @retval GOLIOTH_ERR_MEM_ALLOC Not enough memory in request buffer
////
enum golioth_status golioth_location_finish(struct golioth_location_req *req);

/// Get location from cloud
///
/// @param client The client handle from @ref golioth_client_create
/// @param req Location request sent to Golioth cloud
/// @param rsp Location response received from Golioth cloud
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @retval GOLIOTH_OK response received from server, set was successful
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
/// @retval GOLIOTH_ERR_TIMEOUT response not received from server, timeout occurred
enum golioth_status golioth_location_get_sync(struct golioth_client *client,
                                              const struct golioth_location_req *req,
                                              struct golioth_location_rsp *rsp,
                                              int32_t timeout_s);

#ifdef __cplusplus
}
#endif
