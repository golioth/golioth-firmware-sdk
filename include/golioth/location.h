/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <golioth/client.h>
#include <golioth/golioth_status.h>
#include <stdint.h>
#include <zcbor_encode.h>

struct golioth_location_rsp
{
    /** Latitudal position in nanodegrees (0 to +-180E9) */
    int64_t latitude;
    /** Longitudal position in nanodegrees (0 to +-180E9) */
    int64_t longitude;

    /** Accuracy in meters */
    int64_t accuracy;
};

#define GOLIOTH_LOCATION_FLAG_WIFI (1 << 0)

/**
 * @brief Wi-Fi location request, used with golioth_location_init(), golioth_location_finish() and
 *        golioth_location_get_sync()
 */
struct golioth_location_req
{
#ifdef CONFIG_GOLIOTH_LOCATION
    uint8_t buf[CONFIG_GOLIOTH_LOCATION_REQUEST_BUFFER_SIZE];
    zcbor_state_t zse[2 /* backup */ + 1 /* WiFi only */];
    enum golioth_status status;
    int flags;

    struct golioth_location_rsp *rsp;
#endif
};

void golioth_location_init(struct golioth_location_req *req);
enum golioth_status golioth_location_finish(struct golioth_location_req *req);

enum golioth_status golioth_location_get_sync(struct golioth_client *client,
                                              struct golioth_location_req *req,
                                              struct golioth_location_rsp *rsp);
