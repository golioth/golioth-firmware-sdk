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

/// @}

#ifdef __cplusplus
}
#endif
