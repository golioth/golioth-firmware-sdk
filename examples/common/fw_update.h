/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <golioth/client.h>
#include <golioth/ota.h>
#include <stdbool.h>

struct golioth_fw_update_config
{
    /// The current firmware version, NULL-terminated, shallow-copied from user. (e.g. "1.2.3")
    const char *current_version;
    /// The name of the package in the manifest for the main firmware, NULL-terminated,
    /// shallow-copied from user (e.g. "main").
    const char *fw_package_name;
};

/// @defgroup golioth_fw_update golioth_fw_update
/// Create a background thread that will execute Over-the-Air (OTA) updates
///
/// https://docs.golioth.io/reference/protocols/coap/ota
/// @{

/// Create a thread that will perform firmware updates.
///
/// The thread will observe OTA manifests then execute the OTA update,
/// including state reporting to Golioth and updating firmware on the device.
///
/// Will ignore any received OTA manifests where the firmware already
/// matches current_version.
///
/// The current_version parameter is assumed to be a static string and is therefore
/// not copied (just a shallow copy of the pointer is stored internally).
/// Calling code should ensure that current_version is not pointing to a string
/// which might go out of scope.
///
/// @param client The client handle from @ref golioth_client_create
/// @param current_version The current firmware version (e.g. "1.2.3"), shallow copy, must be
///     NULL-terminated
void golioth_fw_update_init(struct golioth_client *client, const char *current_version);

/// Same as golioth_fw_update_init, but with additional configuration specified via struct.
///
/// @param client The client handle from @ref golioth_client_create
/// @param config The configuration struct (see @ref golioth_fw_update_config).
void golioth_fw_update_init_with_config(struct golioth_client *client,
                                        const struct golioth_fw_update_config *config);

/// Function callback type, for FW update state change listeners
///
/// @param state The new state being transitioned to
/// @param reason The reason the state transition is happening
/// @param user_arg Arbitraty user argument, can be NULL.
typedef void (*golioth_fw_update_state_change_callback)(enum golioth_ota_state state,
                                                        enum golioth_ota_reason reason,
                                                        void *user_arg);

/// Register listener for FW update state changes.
///
/// This is useful if your app needs to react or track changes
/// in the state of the FW update process (e.g. DOWNLOADING, DOWNLOADED, etc).
///
/// @param callback Function to be called when a state change happens
/// @param user_arg Arbitraty user argument, can be NULL
void golioth_fw_update_register_state_change_callback(
    golioth_fw_update_state_change_callback callback,
    void *user_arg);

//---------------------------------------------------------------------------
// Backend API for firmware updates. Required to be implemented by port.
// Not intended to be called by user code.
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
/// This function must return GOLIOTH_ERR_IO when there is an issue storing the block.
///
/// @param block The block data buffer
/// @param block_size The block data size, in bytes
/// @param offset The offset of this block in the overall firmware image
/// @param total_size The total firmware image size
///
/// @return GOLIOTH_OK - Block handled
/// @return GOLIOTH_ERR_IO - error handling block, abort firmware update
enum golioth_status fw_update_handle_block(const uint8_t *block,
                                           size_t block_size,
                                           size_t offset,
                                           size_t total_size);

/// Post-download hook.
///
/// Called by golioth_fw_update.c after downloading the full image.
/// Can be used by backend ports to do any final work, if needed.
enum golioth_status fw_update_post_download(void);

/// Check if candidate image matches target.
///
/// Called by golioth_fw_update.c prior to downloading a new image
/// to check whether it is already present in candidate slot.
///
/// @param hash Image SHA256 hash of size GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN
/// @param img_size Image size, in bytes
///
/// @return GOLIOTH_OK - image validated
/// @return Otherwise - error in validation, abort firmware update
enum golioth_status fw_update_check_candidate(const uint8_t *hash, size_t img_size);

/// Switch to the new boot image. This will cause the new image
/// to be booted next time.
///
/// @return GOLIOTH_OK - changed boot image
/// @return Otherwise - Error changing boot image, abort firmware update
enum golioth_status fw_update_change_boot_image(void);

/// Called when firmware update aborted
///
/// Can be used by backend ports to clean up after a firmware update
/// is aborted.
void fw_update_end(void);
//---------------------------------------------------------------------------

/// @}

#ifdef __cplusplus
}
#endif
