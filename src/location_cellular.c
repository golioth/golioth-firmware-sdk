/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/location/cellular.h>

#include "location.h"

#if defined(CONFIG_GOLIOTH_LOCATION_CELLULAR)

LOG_TAG_DEFINE(location_cellular);

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

enum golioth_status golioth_location_cellular_append(struct golioth_location_req *req,
                                                     const struct golioth_cellular_info *cell)
{
    const char *type_str = cellular_type_to_str(cell->type);
    enum golioth_status status;
    bool ok;

    status = golioth_location_append(req, GOLIOTH_LOCATION_FLAG_CELLULAR, "cell");
    if (status != GOLIOTH_OK)
    {
        return status;
    }

    ok = zcbor_map_start_encode(req->zse, 1);
    if (!ok)
    {
        GLTH_LOGE("Failed to %s %s encoding", "start", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    ok = zcbor_tstr_put_lit(req->zse, "type") && zcbor_tstr_put_term(req->zse, type_str, 16)
        && zcbor_tstr_put_lit(req->zse, "mcc") && zcbor_uint32_put(req->zse, cell->mcc)
        && zcbor_tstr_put_lit(req->zse, "mnc") && zcbor_uint32_put(req->zse, cell->mnc)
        && zcbor_tstr_put_lit(req->zse, "id") && zcbor_uint32_put(req->zse, cell->id);
    if (!ok)
    {
        GLTH_LOGE("Failed to encode %s", "type, mcc, mnc, id");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    if (cell->strength)
    {
        ok = zcbor_tstr_put_lit(req->zse, "strength") && zcbor_int32_put(req->zse, cell->strength);
        if (!ok)
        {
            GLTH_LOGE("Failed to encode %s", "strength");
            return GOLIOTH_ERR_MEM_ALLOC;
        }
    }

    ok = zcbor_map_end_encode(req->zse, 1);
    if (!ok)
    {
        GLTH_LOGE("Failed to %s %s encoding", "end", "map");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    GLTH_LOGD("Encoded successfully %s mcc=%" PRIu16 " mnc=%" PRIu16 " id=%" PRIu32,
            type_str,
            cell->mcc,
            cell->mnc,
            cell->id);

    return 0;
}

#endif  // CONFIG_GOLIOTH_LOCATION_CELLULAR
