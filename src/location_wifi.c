/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/location/wifi.h>

#include "location.h"

#if defined(CONFIG_GOLIOTH_LOCATION_WIFI)

LOG_TAG_DEFINE(location_wifi);

enum golioth_status golioth_location_wifi_append(struct golioth_location_req *req,
                                                 const struct golioth_wifi_scan_result *result)
{
    char mac_str[6 * 3];
    enum golioth_status status;
    bool ok;

    status = golioth_location_append(req, GOLIOTH_LOCATION_FLAG_WIFI, "wifi");
    if (status != GOLIOTH_OK)
    {
        return status;
    }

    sprintf(mac_str,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            result->mac[0],
            result->mac[1],
            result->mac[2],
            result->mac[3],
            result->mac[4],
            result->mac[5]);

    ok = zcbor_map_start_encode(req->zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to %s %s encoding", "start", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(req->zse, "mac");
    if (!ok)
    {
        LOG_ERR("Failed to encode %s %s", "mac", "key");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_term(req->zse, mac_str, 6 * 2 + 5);
    if (!ok)
    {
        LOG_ERR("Failed to encode %s %s", "mac", "val");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(req->zse, "rss") && zcbor_int32_put(req->zse, result->rssi);
    if (!ok)
    {
        LOG_ERR("Failed to encode RSSI");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_map_end_encode(req->zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to %s %s encoding", "end", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    LOG_DBG("Encoded successfully %s", mac_str);

    return 0;
}

#endif  // CONFIG_GOLIOTH_LOCATION_WIFI
