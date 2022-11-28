/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "golioth.h"
#include <stdbool.h>

/// @defgroup golioth_fw_update golioth_fw_update
/// Create a background task that will execute Over-the-Air (OTA) updates
///
/// https://docs.golioth.io/reference/protocols/coap/ota
/// @{

/// Create a task that will perform firmware updates.
///
/// The task will observe OTA manifests then execute the OTA update,
/// including state reporting to Golioth and updating firmware on the device.
///
/// Will ignore any received OTA manifests where the firmware already
/// matches current_version.
///
/// @param client The client handle from @ref golioth_client_create
/// @param current_version The current firmware version (e.g. "1.2.3")
void golioth_fw_update_init(golioth_client_t client, const char* current_version);

/// Function callback type, for FW update state change listeners
///
/// @param state The new state being transitioned to
/// @param reason The reason the state transition is happening
/// @param user_arg Arbitraty user argument, can be NULL.
typedef void (*golioth_fw_update_state_change_callback)(
        golioth_ota_state_t state,
        golioth_ota_reason_t reason,
        void* user_arg);

/// Register listener for FW update state changes.
///
/// This is useful if your app needs to react or track changes
/// in the state of the FW update process (e.g. DOWNLOADING, DOWNLOADED, etc).
///
/// @param callback Function to be called when a state change happens
/// @param user_arg Arbitraty user argument, can be NULL
void golioth_fw_update_register_state_change_callback(
        golioth_fw_update_state_change_callback callback,
        void* user_arg);

//---------------------------------------------------------------------------
// Backend API for firmware updates. Required to be implemented by port.
//---------------------------------------------------------------------------

/// Returns true if this is the first boot of a new candidate image
/// that is pending/not-yet-confirmed as "good".
bool fw_update_is_pending_verify(void);

/// Initiate a firmware rollback
void fw_update_rollback(void);

/// Reboot the device.
///
/// Called by library during a rollback event or after writing
/// a new pending/candidate image to flash.
void fw_update_reboot(void);

/// Cancel the rollback and commit to the current firmware image.
///
/// Marks the image as "good" and prevents rollback to the old image.
void fw_update_cancel_rollback(void);

/// Handle a single block of a new firmware image (e.g. write to flash
/// in the secondary firmware slot).
///
/// @param block The block data buffer
/// @param block_size The block data size, in bytes
/// @param offset The offset of this block in the overall firmware image
/// @param total_size The total firmware image size
///
/// @return GOLIOTH_OK - Block handled
/// @return Otherwise - error handling block, abort firmware update
golioth_status_t fw_update_handle_block(
        const uint8_t* block,
        size_t block_size,
        size_t offset,
        size_t total_size);

/// Post-download hook.
///
/// Called by golioth_fw_update.c after downloading the full image.
/// Can be used by backend ports to do any final work, if needed.
void fw_update_post_download(void);

/// Validate new image after downloading
///
/// Called by golioth_fw_update.c after downloading the full image.
/// Can be used by backend ports to validate the image before
/// attempting to boot into it.
///
/// @return GOLIOTH_OK - image validated
/// @return Otherwise - error in validation, abort firmware update
golioth_status_t fw_update_validate(void);

/// Switch to the new boot image. This will cause the new image
/// to be booted next time.
///
/// @return GOLIOTH_OK - changed boot image
/// @return Otherwise - Error changing boot image, abort firmware update
golioth_status_t fw_update_change_boot_image(void);

/// Called when firmware update aborted
///
/// Can be used by backend ports to clean up after a firmware update
/// is aborted.
void fw_update_end(void);
//---------------------------------------------------------------------------

/// @}
