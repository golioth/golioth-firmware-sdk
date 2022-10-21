#include "golioth_packet_capture.h"
#include "golioth_debug.h"
#include "golioth_statistics.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdbool.h>
#include <sys/param.h>  // MIN
#include <string.h>     // memcpy

#define TAG "golioth_packet_capture"

// TODO - stub functions conditioned on config value


// TODO - move to golioth_config.h/Kconfig
#define CONFIG_GOLIOTH_PACKET_CAPTURE_MAX_PACKET_LEN 128
#define CONFIG_GOLIOTH_PACKET_CAPTURE_MAX_QUEUE_ITEMS 64
#define CONFIG_GOLIOTH_PACKET_CAPTURE_TASK_DELAY_MS 5000

#define MAX_LIGHTDB_STREAM_BUFFER_SIZE 1024

static golioth_client_t _client;
static QueueHandle_t _packet_queue;
static uint8_t _ldb_stream_buffer[MAX_LIGHTDB_STREAM_BUFFER_SIZE];
static uint32_t _ldb_stream_buffer_wr_offset;
static bool _enable;

typedef struct {
    uint8_t* buffer;
    size_t len;
} packet_info_t;

static bool copy_packet_to_stream_buffer(const packet_info_t* info) {
    uint32_t bytes_remaining = sizeof(_ldb_stream_buffer) - _ldb_stream_buffer_wr_offset;
    bool have_enough_space = (info->len <= bytes_remaining);

    if (!have_enough_space) {
        return false;
    }

    memcpy(&_ldb_stream_buffer[_ldb_stream_buffer_wr_offset], info->buffer, info->len);
    _ldb_stream_buffer_wr_offset += info->len;
}

static void reset_stream_buffer(void) {
    _ldb_stream_buffer_wr_offset = 0;
}

static void send_stream_buffer(void) {
    //    Is there a way to send binary data to lightdb stream?
    //          Not directly, no.
    //    If not, is there a way to encode binary data in CBOR?
    //          Yes, using major type 2. See QCBOREncode_AddBytes.
}

void packet_capture_task(void* arg) {
    assert(_packet_queue);

    while (1) {
        int32_t num_packets = (int32_t)uxQueueMessagesWaiting(_packet_queue);

        while (num_packets-- > 0) {
            packet_info_t info;
            assert(xQueuePeek(_packet_queue, &info, 0));

            bool copied = copy_packet_to_stream_buffer(&info);
            if (copied) {
                // Receive the item, to remove it from the queue
                assert(xQueueReceive(_packet_info, &info, 0));

                // Free the packet buffer, now that it's copied out
                free(info.buffer);
                GSTATS_INC_FREE("packet_buffer");
            } else {
                // There wasn't enough room in the stream buffer,
                // so leave the packet in the queue.

                // Since the stream buffer is full, it's ready to send to Golioth
                send_stream_buffer();

                // Reset the stream buffer, so we can fill it with more packets
                reset_stream_buffer();
            }
        }

        vTaskDelay(CONFIG_GOLIOTH_PACKET_CAPTURE_TASK_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void golioth_packet_capture_init(void) {
    // Create packet queue
    _packet_queue =
            xQueueCreate(CONFIG_GOLIOTH_PACKET_CAPTURE_MAX_QUEUE_ITEMS, sizeof(packet_info_t));
    if (!_packet_queue) {
        GLTH_LOGE(TAG, "Failed to create packet queue");
        return;
    }

    // Create task that will pull items out of queue
    bool task_created = xTaskCreate(
            packet_capture_task,
            "packet_capture",
            4096,
            NULL,  // task arg
            CONFIG_GOLIOTH_COAP_TASK_PRIORITY,
            NULL);  // task handle (not used)
    if (!task_created) {
        GLTH_LOGE(TAG, "Failed to create packet capture task");
        vQueueDelete(_packet_queue);
        return;
    }
}

void golioth_packet_capture_push(const uint8_t* data, size_t data_len) {
    if (!_packet_queue) {
        GLTH_LOGW(TAG, "Packet queue not initialized");
        return;
    }

    if (!_enable) {
        return;
    }

    if (!data || data_len == 0) {
        return;
    }

    // Dynamically allocate a buffer to copy the packet into.
    // Will be freed after the packet buffer is handled in packet_capture_task.
    size_t bytes_to_capture = MIN(data_len, MAX_LIGHTDB_STREAM_BUFFER_SIZE);
    uint8_t* packet_buffer = (uint8_t*)malloc(bytes_to_capture);
    if (!packet_buffer) {
        GLTH_LOGW(TAG, "Failed to allocate packet buffer");
        return;
    }
    GSTATS_INC_ALLOC("packet_buffer");
    memcpy(packet_buffer, data, bytes_to_capture);

    // Push packet to queue, no wait
    packet_info_t info = {
            .buffer = packet_buffer,
            .len = bytes_to_capture,
    };
    BaseType_t sent = xQueueSend(_packet_queue, &info, 0);
    if (!sent) {
        GLTH_LOGW(TAG, "Failed to enqueue packet buffer, queue full");
        free(packet_buffer);
        GSTATS_INC_FREE("packet_buffer");
    }
}

void golioth_packet_capture_set_client(golioth_client_t client) {
    _client = client;
}

void golioth_packet_capture_set_enable(bool enable) {
    _enable = enable;
}
