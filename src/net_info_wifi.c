/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/net_info/wifi.h>

#include "net_info.h"

#if defined(CONFIG_GOLIOTH_NET_INFO_WIFI)

LOG_TAG_DEFINE(net_info_wifi);

enum golioth_status golioth_net_info_wifi_append(struct golioth_net_info *info,
                                                 const struct golioth_wifi_scan_result *result)
{
    char mac_str[6 * 3];
    enum golioth_status status;
    bool ok;

    status = golioth_net_info_append(info, GOLIOTH_NET_INFO_FLAG_WIFI, "wifi");
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

    ok = zcbor_map_start_encode(info->zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to %s %s encoding", "start", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(info->zse, "mac");
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode %s %s", "mac", "key");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_term(info->zse, mac_str, 6 * 2 + 5);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode %s %s", "mac", "val");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(info->zse, "rss") && zcbor_int32_put(info->zse, result->rssi);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode RSSI");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_map_end_encode(info->zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to %s %s encoding", "end", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    GLTH_LOGD(TAG, "Encoded successfully %s", mac_str);

    return 0;
}

#endif  // CONFIG_GOLIOTH_NET_INFO_WIFI
