#include "golioth_mbox.h"
#include "golioth_debug.h"
#include <assert.h>
#include <stdlib.h>

#define TAG "golioth_mbox"

golioth_mbox_t golioth_mbox_create(size_t num_items, size_t item_size) {
    golioth_mbox_t new_mbox = (golioth_mbox_t)calloc(1, sizeof(struct golioth_mbox));
    assert(new_mbox);

    // Allocate storage for the items in the ringbuffer
    size_t bufsize = RINGBUF_BUFFER_SIZE(item_size, num_items);
    new_mbox->ringbuf.buffer = (uint8_t*)malloc(bufsize);
    assert(new_mbox->ringbuf.buffer);

    new_mbox->ringbuf.buffer_size = bufsize;
    new_mbox->ringbuf.item_size = item_size;
    new_mbox->fill_count_sem = golioth_sys_sem_create(num_items, 0);
    new_mbox->ringbuf_mutex = golioth_sys_sem_create(1, 1);

    assert(ringbuf_capacity(&new_mbox->ringbuf) == num_items);
    assert(ringbuf_size(&new_mbox->ringbuf) == 0);

    GLTH_LOGI(
            TAG,
            "Mbox created, bufsize: %u, num_items: %u, item_size: %u",
            bufsize,
            num_items,
            item_size);

    return new_mbox;
}

size_t golioth_mbox_num_messages(golioth_mbox_t mbox) {
    assert(mbox);
    return ringbuf_size(&mbox->ringbuf);
}

bool golioth_mbox_try_send(golioth_mbox_t mbox, const void* item) {
    assert(mbox);

    assert(golioth_sys_sem_take(mbox->ringbuf_mutex, GOLIOTH_SYS_WAIT_FOREVER));
    bool sent = ringbuf_put(&mbox->ringbuf, item);
    golioth_sys_sem_give(mbox->ringbuf_mutex);

    if (sent) {
        assert(golioth_sys_sem_give(mbox->fill_count_sem));
    }

    return sent;
}

bool golioth_mbox_recv(golioth_mbox_t mbox, void* item, int32_t timeout_ms) {
    assert(mbox);
    bool received = golioth_sys_sem_take(mbox->fill_count_sem, timeout_ms);
    if (received) {
        assert(ringbuf_get(&mbox->ringbuf, item));
    }
    return received;
}

void golioth_mbox_destroy(golioth_mbox_t mbox) {
    assert(mbox);
    // free stuff in the mbox
    free(mbox->ringbuf.buffer);
    golioth_sys_sem_destroy(mbox->fill_count_sem);
    golioth_sys_sem_destroy(mbox->ringbuf_mutex);
    // free the mbox itself
    free(mbox);
}
