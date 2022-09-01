/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "golioth.h"

/// @defgroup golioth_fw_update golioth_fw_update
/// Create a background task that will execute Over-the-Air (OTA) updates
///
/// https://docs.golioth.io/reference/protocols/coap/ota
/// @{

/// Create a task that will perform firmware updates.
///
/// The task will observe OTA manifests then execute the OTA update,
/// including state reporting to Golioth and device firmware updates
/// using Espressif's app_update component.
///
/// Will ignore any received OTA manifests where the firmware already
/// matches current_version.
///
/// @param client The client handle from @ref golioth_client_create
/// @param current_version The current firmware version (e.g. "1.2.3")
void golioth_fw_update_init(golioth_client_t client, const char* current_version);

/// @}
