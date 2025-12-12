#include <golioth/client.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include <golioth/stream.h>
#include <string.h>
#include "test_data.h"

LOG_TAG_DEFINE(test_stream);

static golioth_sys_sem_t connected_sem;
static golioth_sys_sem_t multi_block_sem;

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    if (event == GOLIOTH_CLIENT_EVENT_CONNECTED)
    {
        golioth_sys_sem_give(connected_sem);
    }
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

    if (bu_offset >= bu_source->len)
    {
        GLTH_LOGE(TAG, "Calculated offset is past end of buffer: %zu", bu_offset);
        goto bu_error;
    }

    if (bu_size <= bu_max_block_size)
    {
        *block_size = bu_size;
        *is_last = true;
    }

    GLTH_LOGI(TAG,
              "block-idx: %" PRIu32 " bu_offset: %zu bytes_remaining: %zu",
              block_idx,
              bu_offset,
              bu_size);

    memcpy(block_buffer, bu_source->buf + bu_offset, *block_size);
    return GOLIOTH_OK;

bu_error:
    *block_size = 0;
    *is_last = true;

    return GOLIOTH_ERR_NO_MORE_DATA;
}

void test_block_upload(struct golioth_client *client)
{
    struct block_upload_source bu_source = {.buf = (uint8_t *) test_data_json,
                                            .len = test_data_json_len};

    int err = golioth_stream_set_blockwise_sync(client,
                                                "data/block_upload",
                                                GOLIOTH_CONTENT_TYPE_JSON,
                                                block_upload_read_chunk,
                                                (void *) &bu_source);

    if (err)
    {
        GLTH_LOGE(TAG, "Stream error: %d", err);
    }
    else
    {
        GLTH_LOGI(TAG, "Block upload successful!");
    }
}

void on_multi_block(struct golioth_client *client,
                    enum golioth_status status,
                    const struct golioth_coap_rsp_code *coap_rsp_code,
                    const char *path,
                    size_t block_szx,
                    void *arg)
{
    uintptr_t block_idx = (uintptr_t) arg;
    GLTH_LOGI(TAG,
              "Multi-block callback; Block: %" PRIuPTR ", Status: %d, CoAP: %d.%02d",
              block_idx,
              status,
              coap_rsp_code->code_class,
              coap_rsp_code->code_detail);

    golioth_sys_sem_give(multi_block_sem);
}

void test_multi_block_upload(struct golioth_client *client)
{
    enum golioth_status status;
    size_t bu_offset, bu_data_len, bu_data_remaining;
    uint32_t block_idx = 0;
    bool is_last = false;

    struct blockwise_transfer *ctx =
        golioth_stream_blockwise_start(client, "data/multi_upload", GOLIOTH_CONTENT_TYPE_JSON);
    if (!ctx)
    {
        GLTH_LOGE(TAG, "Error allocating context");
    }

    while (true)
    {
        bu_offset = block_idx * CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE;
        bu_data_len = CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE;
        bu_data_remaining = test_data_json_len - bu_offset;

        if (bu_data_remaining <= CONFIG_GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE)
        {
            bu_data_len = bu_data_remaining;
            is_last = true;
        }

        GLTH_LOGI(TAG,
                  "block-idx: %" PRIu32 " bu_offset: %zu bytes_remaining: %zu",
                  block_idx,
                  bu_offset,
                  bu_data_remaining);


        status = golioth_stream_blockwise_set_block_async(ctx,
                                                          block_idx,
                                                          ((uint8_t *) test_data_json) + bu_offset,
                                                          bu_data_len,
                                                          is_last,
                                                          on_multi_block,
                                                          (void *) block_idx);

        if (status)
        {
            GLTH_LOGE(TAG, "Error during multi-block upload: %d", status);
            goto cleanup;
        }

        golioth_sys_sem_take(multi_block_sem, GOLIOTH_SYS_WAIT_FOREVER);

        if (is_last)
        {
            break;
        }

        block_idx += 1;
    }

    GLTH_LOGI(TAG, "Multi-block upload successful!");

cleanup:
    golioth_stream_blockwise_finish(ctx);
}

void hil_test_entry(const struct golioth_client_config *config)
{
    connected_sem = golioth_sys_sem_create(1, 0);
    multi_block_sem = golioth_sys_sem_create(1, 0);

    golioth_debug_set_cloud_log_enabled(false);

    struct golioth_client *client = golioth_client_create(config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_sys_sem_take(connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    /* Let log messages from connection clear */
    golioth_sys_msleep(1000);

    test_block_upload(client);
    test_multi_block_upload(client);

    /* Wait for stream to propagate from CDG to LightDB Stream */
    golioth_sys_msleep(6000);

    GLTH_LOGI(TAG, "All stream data has been sent");

    /* Let log messages process before returning */
    golioth_sys_msleep(5000);
}
