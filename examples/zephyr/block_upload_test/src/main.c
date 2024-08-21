/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_block_upload, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <samples/common/sample_credentials.h>
#include <samples/common/net_connect.h>
#include <stdlib.h>
#include <string.h>
#include <zcbor_encode.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "mooltipass.h"

#define SYNC_TIMEOUT_S 2

struct golioth_client *client;
static K_SEM_DEFINE(connected, 0, 1);

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        k_sem_give(&connected);
    }
    LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

struct block_upload_source
{
    uint8_t *buf;
    size_t len;
};

static enum golioth_status block_upload_read_chunk(uint32_t block_idx,
                                                   uint8_t *block_buffer,
                                                   size_t *block_size,
                                                   bool *is_last,
                                                   void *arg)
{
    size_t bu_max_block_size = *block_size;
    const struct block_upload_source *bu_source = arg;
    size_t bu_offset = block_idx * bu_max_block_size;
    size_t bu_size = bu_source->len - bu_offset;

    LOG_DBG("block-idx: %u bu_offset: %u bytes_remaining: %u", block_idx, bu_offset, bu_size);

    if (bu_offset >= bu_source->len)
    {
        LOG_ERR("Calculated offset is past end of buffer: %d", bu_offset);
        goto bu_error;
    }

    if (bu_size < 0)
    {
        LOG_ERR("Calculated size for next block is < 0: %d", bu_size);
        goto bu_error;
    }

    if (bu_size <= bu_max_block_size)
    {
        *block_size = bu_size;
        *is_last = true;
    }
    else
    {
        *block_size = bu_max_block_size;
        *is_last = false;
    }

    memcpy(block_buffer, bu_source->buf + bu_offset, *block_size);
    return GOLIOTH_OK;

bu_error:
    *block_size = 0;
    *is_last = true;

    return GOLIOTH_ERR_NO_MORE_DATA;
}

int main(void)
{
    LOG_DBG("Start Golioth stream sample");

    IF_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON, (net_connect();))

    /* Note: In production, you would provision unique credentials onto each
     * device. For simplicity, we provide a utility to hardcode credentials as
     * kconfig options in the samples.
     */
    const struct golioth_client_config *client_config = golioth_sample_credentials_get();

    client = golioth_client_create(client_config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    k_sem_take(&connected, K_FOREVER);

    int counter = 0;

    while (true)
    {
        LOG_INF("Counter: %d", counter);
        if (counter == 3)
        {
            struct block_upload_source bu_ctx = {.buf = (uint8_t *) &mooltipass,
                                                 .len = mooltipass_len};

            int err = golioth_stream_set_blockwise_sync(client,
                                                        "upload",
                                                        GOLIOTH_CONTENT_TYPE_OCTET_STREAM,
                                                        block_upload_read_chunk,
                                                        &bu_ctx);

            if (err)
            {
                LOG_ERR("Block upload error: %d", err);
            }
            else
            {
                LOG_INF("Block upload successful!");
            }
        }

        counter++;
        k_sleep(K_SECONDS(5));
    }

    return 0;
}
