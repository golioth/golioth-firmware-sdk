/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/net_info.h>
#include <golioth/zcbor_utils.h>
#include <zcbor_decode.h>

#include "coap_client.h"
#include "net_info.h"

#if defined(CONFIG_GOLIOTH_NET_INFO)

LOG_TAG_DEFINE(net_info);

struct golioth_net_info *golioth_net_info_create()
{
    struct golioth_net_info *info = golioth_sys_malloc(sizeof(struct golioth_net_info));

    if (info != NULL)
    {
        info->flags = 0;
    }

    return info;
}

enum golioth_status golioth_net_info_destroy(struct golioth_net_info *info)
{

    if (info == NULL)
    {
        GLTH_LOGE(TAG, "Network information must not be NULL");
        return GOLIOTH_ERR_NULL;
    }

    free(info);
    return GOLIOTH_OK;
}

enum golioth_status golioth_net_info_append(struct golioth_net_info *info,
                                            int flag,
                                            const char *name)
{
    /* If no info has been apppended, create a new encode state and start the map */
    if (0 == info->flags)
    {
        zcbor_new_encode_state(info->zse,
                               ZCBOR_ARRAY_SIZE(info->zse),
                               info->buf,
                               sizeof(info->buf),
                               1);
        zcbor_map_start_encode(info->zse, 1);
    }

    int started = info->flags & GOLIOTH_NET_INFO_STARTED_MASK;
    int finished = info->flags >> GOLIOTH_NET_INFO_SHIFT_FINISHED;
    bool ok;

    if (finished & flag)
    {
        GLTH_LOGE(TAG,
                  "Interchangably calling different golioth_net_info_*_append() is not supported");
        return GOLIOTH_ERR_NOT_ALLOWED;
    }

    if (started & flag)
    {
        return GOLIOTH_OK;
    }

    if (started)
    {
        ok = zcbor_list_end_encode(info->zse, 1);
        if (!ok)
        {
            GLTH_LOGE(TAG, "Failed to close network info group");
            return GOLIOTH_ERR_MEM_ALLOC;
        }
    }

    /* Mark all STARTED as FINISHED */
    info->flags |= started << GOLIOTH_NET_INFO_SHIFT_FINISHED;

    /* Mark new flag as STARTED */
    info->flags |= flag;

    /* Start encoding new list */
    ok = zcbor_tstr_put_term(info->zse, name, 8) && zcbor_list_start_encode(info->zse, 1);
    if (!ok)
    {
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    return GOLIOTH_OK;
}

enum golioth_status golioth_net_info_finish(struct golioth_net_info *info)
{
    bool ok;

    if (0 == info->flags)
    {
        return GOLIOTH_ERR_NULL;
    }

    ok = zcbor_list_end_encode(info->zse, 1) && zcbor_map_end_encode(info->zse, 1);
    if (!ok)
    {
        GLTH_LOGE(TAG, "Failed to finish network info");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    /* Reset flags to signal new payload on next append */
    info->flags = 0;

    return GOLIOTH_OK;
}

const uint8_t *golioth_net_info_get_buf(const struct golioth_net_info *info)
{
    return info->buf;
}

size_t golioth_net_info_get_buf_len(const struct golioth_net_info *info)
{
    return info->zse->payload - info->buf;
}

#endif  // CONFIG_GOLIOTH_NET_INFO
