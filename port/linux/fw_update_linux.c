// TODO - add implementation
//
// One idea is to do a "lightweight" OTA, where there is a loader process
// that dynamically loads the app as a .so.

#include "golioth_fw_update.h"

#define TAG "fw_update_linux"

// If set to 1, any received FW blocks will be written to file DOWNLOADED_FILE_NAME
// This is mostly used for testing right now.
#define ENABLE_DOWNLOAD_TO_FILE 0
#define DOWNLOADED_FILE_NAME "downloaded.bin"

bool fw_update_is_pending_verify(void) {
    return false;
}

void fw_update_rollback(void) {}

void fw_update_reboot(void) {}

void fw_update_cancel_rollback(void) {}

#if ENABLE_DOWNLOAD_TO_FILE
static FILE* _fp;
golioth_status_t fw_update_handle_block(
        const uint8_t* block,
        size_t block_size,
        size_t offset,
        size_t total_size) {
    if (!_fp) {
        _fp = fopen(DOWNLOADED_FILE_NAME, "w");
    }
    GLTH_LOGD(
            TAG,
            "block_size 0x%08lX, offset 0x%08lX, total_size 0x%08lX",
            block_size,
            offset,
            total_size);
    fwrite(block, block_size, 1, _fp);
    return GOLIOTH_OK;
}

void fw_update_post_download(void) {
    if (_fp) {
        fclose(_fp);
        _fp = NULL;
    }
}
#else
golioth_status_t fw_update_handle_block(
        const uint8_t* block,
        size_t block_size,
        size_t offset,
        size_t total_size) {
    return GOLIOTH_OK;
}
void fw_update_post_download(void) {}
#endif

golioth_status_t fw_update_validate(void) {
    return GOLIOTH_OK;
}

golioth_status_t fw_update_change_boot_image(void) {
    return GOLIOTH_OK;
}

void fw_update_end(void) {}
