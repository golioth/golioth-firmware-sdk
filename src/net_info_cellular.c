/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/net_info/cellular.h>

#include "net_info.h"

#if defined(CONFIG_GOLIOTH_NET_INFO_CELLULAR)

LOG_TAG_DEFINE(net_info_cellular);

static const char *cellular_type_to_str(enum golioth_cellular_type type)
{
    switch (type)
    {
        case GOLIOTH_CELLULAR_TYPE_LTECATM:
            return "ltecatm";
        case GOLIOTH_CELLULAR_TYPE_NBIOT:
            return "nbiot";
    }

    return "";
}

enum golioth_status golioth_net_info_cellular_append(struct golioth_net_info *info,
                                                     const struct golioth_cellular_info *cell)
{
    const char *type_str = cellular_type_to_str(cell->type);
    enum golioth_status status;
    bool ok;

    status = golioth_net_info_append(info, GOLIOTH_NET_INFO_FLAG_CELLULAR, "cell");
    if (status != GOLIOTH_OK)
    {
        return status;
    }

    ok = zcbor_map_start_encode(info->zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to %s %s encoding", "start", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(info->zse, "type") && zcbor_tstr_put_term(info->zse, type_str, 16)
        && zcbor_tstr_put_lit(info->zse, "mcc") && zcbor_uint32_put(info->zse, cell->mcc)
        && zcbor_tstr_put_lit(info->zse, "mnc") && zcbor_uint32_put(info->zse, cell->mnc)
        && zcbor_tstr_put_lit(info->zse, "id") && zcbor_uint32_put(info->zse, cell->id);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to encode %s", "type, mcc, mnc, id");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    if (cell->strength)
    {
        ok =
            zcbor_tstr_put_lit(info->zse, "strength") && zcbor_int32_put(info->zse, cell->strength);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to encode %s", "strength");
            return GOLIOTH_ERR_MEM_ALLOC;
        }
    }

    ok = zcbor_map_end_encode(info->zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to %s %s encoding", "end", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    GLTH_LOGD(TAG,
              "Encoded successfully %s mcc=%" PRIu16 " mnc=%" PRIu16 " id=%" PRIu32,
              type_str,
              cell->mcc,
              cell->mnc,
              cell->id);

    return 0;
}

#endif  // CONFIG_GOLIOTH_NET_INFO_CELLULAR
