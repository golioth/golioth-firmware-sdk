/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <golioth/location.h>
#include <golioth/zcbor_utils.h>
#include <zcbor_decode.h>

#include "coap_client.h"

#if defined(CONFIG_GOLIOTH_LOCATION)

LOG_TAG_DEFINE(location);

struct golioth_location_get_sync_data
{
    enum golioth_status status;
    struct golioth_location_rsp *rsp;
};

static enum golioth_status location_decode(struct golioth_location_rsp *rsp,
                                           const uint8_t *payload,
                                           size_t payload_size)
{
    ZCBOR_STATE_D(zsd, 1, payload, payload_size, 1, 0);
    struct zcbor_map_entry map_entries[] = {
        ZCBOR_TSTR_LIT_MAP_ENTRY("lat", zcbor_map_int64_decode, &rsp->latitude),
        ZCBOR_TSTR_LIT_MAP_ENTRY("lon", zcbor_map_int64_decode, &rsp->longitude),
        ZCBOR_TSTR_LIT_MAP_ENTRY("acc", zcbor_map_int64_decode, &rsp->accuracy),
    };
    int err;

    err = zcbor_map_decode(zsd, map_entries, ARRAY_SIZE(map_entries));
    if (err)
    {
        LOG_ERR("Failed to parse position");
        return GOLIOTH_ERR_INVALID_FORMAT;
    }

    return GOLIOTH_OK;
}

static void location_cb(struct golioth_client *client,
                        enum golioth_status status,
                        const struct golioth_coap_rsp_code *coap_rsp_code,
                        const char *path,
                        const uint8_t *payload,
                        size_t payload_size,
                        void *arg)
{
    struct golioth_location_get_sync_data *data = arg;

    data->status = status;

    if (status != GOLIOTH_OK)
    {
        if (status == GOLIOTH_ERR_COAP_RESPONSE && payload_size == 0)
        {
            LOG_WRN("Location not found");
            data->status = GOLIOTH_ERR_NULL;
        }
        else
        {
            LOG_ERR("Error status: %d (%s)", status, golioth_status_to_str(status));
        }
        return;
    }

    if (data->rsp)
    {
        data->status = location_decode(data->rsp, payload, payload_size);
    }
}

void golioth_location_init(struct golioth_location_req *req)
{
    zcbor_new_encode_state(req->zse, ZCBOR_ARRAY_SIZE(req->zse), req->buf, sizeof(req->buf), 1);

    req->flags = 0;
}

enum golioth_status golioth_location_finish(struct golioth_location_req *req)
{
    bool ok;

    if (!req->flags)
    {
        return GOLIOTH_ERR_NULL;
    }

    ok = zcbor_list_end_encode(req->zse, 1) && zcbor_map_end_encode(req->zse, 1);
    if (!ok)
    {
        LOG_ERR("Failed to finish location request");
        return GOLIOTH_ERR_MEM_ALLOC;
    }

    return GOLIOTH_OK;
}

enum golioth_status golioth_location_get_sync(struct golioth_client *client,
                                              const struct golioth_location_req *req,
                                              struct golioth_location_rsp *rsp,
                                              int32_t timeout_s)
{
    struct golioth_location_get_sync_data data = {
        .rsp = rsp,
        .status = GOLIOTH_OK,
    };
    enum golioth_status status;

    uint8_t token[GOLIOTH_COAP_TOKEN_LEN];
    golioth_coap_next_token(token);

    status = golioth_coap_client_post(client,
                                      token,
                                      ".l/",
                                      "v1/net",
                                      GOLIOTH_CONTENT_TYPE_CBOR,
                                      req->buf,
                                      req->zse->payload - req->buf,
                                      location_cb,
                                      &data,
                                      true,
                                      timeout_s);
    if (status != GOLIOTH_OK)
    {
        return status;
    }

    return data.status;
}

#endif  // CONFIG_GOLIOTH_LOCATION
