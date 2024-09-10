#include <golioth/client.h>
#include <golioth/golioth_debug.h>
#include <golioth/golioth_sys.h>
#include <golioth/stream.h>
#include <string.h>
#include "test_data.h"

LOG_TAG_DEFINE(test_stream);

static golioth_sys_sem_t connected_sem;

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
                                                "block_upload",
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

void hil_test_entry(const struct golioth_client_config *config)
{
    connected_sem = golioth_sys_sem_create(1, 0);

    golioth_debug_set_cloud_log_enabled(false);

    struct golioth_client *client = golioth_client_create(config);
    golioth_client_register_event_callback(client, on_client_event, NULL);

    golioth_sys_sem_take(connected_sem, GOLIOTH_SYS_WAIT_FOREVER);

    /* Let log messages from connection clear */
    golioth_sys_msleep(1000);

    test_block_upload(client);

    /* Let log messages process before returning */
    golioth_sys_msleep(5000);
}
