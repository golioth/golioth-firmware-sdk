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

// TODO - documentation
// Required to be implemented by port
bool fw_update_is_pending_verify(void);
void fw_update_rollback(void);
void fw_update_reboot(void);
void fw_update_cancel_rollback(void);
golioth_status_t fw_update_handle_block(
        const uint8_t* block,
        size_t block_size,
        size_t offset,
        size_t total_size);
void fw_update_post_download(void);
golioth_status_t fw_update_validate(void);
golioth_status_t fw_update_change_boot_image(void);
void fw_update_end(void);

/// @}
