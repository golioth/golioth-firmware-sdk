/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "golioth_status.h"
#include "golioth_client.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/// @defgroup golioth_settings golioth_settings
/// Functions for interacting with the Golioth Settings service
///
/// The Settings service is for long-lived persistent configuration data.
/// Settings are configured/written from the cloud, read
/// by the device, and written to device flash. The device observes
/// for settings updates, and reports status of applying the
/// settings to the cloud.
///
/// Each setting is a key/value pair, where the key is a string
/// and the value can be int32_t, bool, float, or string. The
/// settings are stored in non-volatile storage (NVS), via
/// Espressif's nvs_flash component. Keys can be no more than
/// 15 characters (limitation in nvs_flash component) and values
/// can be no more than 1000 characters.
///
/// Overall, the flow is:
///
/// 1. Application registers a callback to handle settings.
/// 2. This library observes for settings changes from cloud.
/// 3. Cloud pushes settings changes to device.
/// 4. For each setting, this library calls the user-registered callback.
/// 5. If the callback returns GOLIOTH_SETTINGS_SUCCESS, this library will
///    save the setting to NVS. Otherwise, it will not be saved to NVS.
/// 6. This library reports status of applying settings to cloud.
///
/// This library is responsible for interfacing with the cloud
/// and saving settings to flash, but has no knowledge of the specific settings.
///
/// @{

/// Enumeration of Settings status codes
typedef enum {
    /// Setting applied successfully to the device, stored in NVS
    GOLIOTH_SETTINGS_SUCCESS = 0,
    /// The setting key is not recognized, this setting is unknown
    GOLIOTH_SETTINGS_KEY_NOT_RECOGNIZED = 1,
    /// The setting key is too long, ill-formatted
    GOLIOTH_SETTINGS_KEY_NOT_VALID = 2,
    /// The setting value is improperly formatted
    GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID = 3,
    /// The setting value is outside of allowed range
    GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE = 4,
    /// The setting value string is too long, exceeds max length
    GOLIOTH_SETTINGS_VALUE_STRING_TOO_LONG = 5,
    /// Other general error (e.g. Flash I/O error)
    GOLIOTH_SETTINGS_GENERAL_ERROR = 6,
} golioth_settings_status_t;

/// Different types of setting values
typedef enum {
    GOLIOTH_SETTINGS_VALUE_TYPE_UNKNOWN,
    GOLIOTH_SETTINGS_VALUE_TYPE_INT,
    GOLIOTH_SETTINGS_VALUE_TYPE_BOOL,
    GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT,
    GOLIOTH_SETTINGS_VALUE_TYPE_STRING,
} golioth_settings_value_type_t;

/// A setting value. The type will dictate which of the
/// union fields to use.
typedef struct {
    golioth_settings_value_type_t type;
    union {
        int32_t i32;
        bool b;
        float f;
        struct {
            const char* ptr;
            size_t len;  // not including NULL terminator
        } string;
    };
} golioth_settings_value_t;

/// Callback for an individual setting
///
/// @param key The setting key
/// @param value The setting value
///
/// @return GOLIOTH_SETTINGS_SUCCESS - setting is valid
/// @return Otherwise - setting is not valid
typedef golioth_settings_status_t (
        *golioth_settings_cb)(const char* key, const golioth_settings_value_t* value);

/// Register callback for handling settings.
///
/// The client will be used to observe for settings from the cloud.
///
/// @param client Client handle
/// @param callback Callback function to call for settings changes
///
/// @return GOLIOTH_OK - Callback registered successfully
/// @return Otherwise - Error, callback not registered
golioth_status_t golioth_settings_register_callback(
        golioth_client_t client,
        golioth_settings_cb callback);

/// @}
