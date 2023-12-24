/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <golioth/golioth_status.h>
#include <golioth/client.h>
#include <golioth/config.h>
#include <stdint.h>
#include <stdbool.h>

/// @defgroup golioth_settings golioth_settings
/// Functions for interacting with the Golioth Settings service
///
/// The Settings service is for long-lived persistent configuration data.
/// Settings are configured/written from the cloud and read
/// by the device. The device observes
/// for settings updates, and reports status of applying the
/// settings to the cloud.
///
/// Each setting is a key/value pair, where the key is a string
/// and the value can be int32_t, bool, float, or string.
///
/// Overall, the flow is:
///
/// 1. Application registers a callback to handle each setting.
/// 2. This library observes for settings changes from cloud.
/// 3. Cloud pushes settings changes to device.
/// 4. For each setting, this library calls user-registered callbacks.
/// 5. This library reports status of applying settings to cloud.
///
/// For each setting recieved, this library will check:
///     * The key is known/registered
///     * The type matches the registered type
///     * (integer only) The value is within the min/max range
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
    /// Other general error (e.g. I/O error)
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

/// Callback function types for golioth_settings_register_*
///
/// @param new_value The setting value received from Golioth cloud
/// @param arg User's registered callback arg
///
/// @return GOLIOTH_SETTINGS_OK - the setting was applied successfully
/// @return Otherwise - there was an error applying the setting
typedef golioth_settings_status_t (*golioth_int_setting_cb)(int32_t new_value, void* arg);
typedef golioth_settings_status_t (*golioth_bool_setting_cb)(bool new_value, void* arg);
typedef golioth_settings_status_t (*golioth_float_setting_cb)(float new_value, void* arg);
typedef golioth_settings_status_t (
        *golioth_string_setting_cb)(const char* new_value, size_t new_value_len, void* arg);

/// Private struct for storing a single setting
typedef struct {
    bool is_valid;
    const char* key;  // aka name
    golioth_settings_value_type_t type;
    union {
        golioth_int_setting_cb int_cb;
        golioth_bool_setting_cb bool_cb;
        golioth_float_setting_cb float_cb;
        golioth_string_setting_cb string_cb;
    };
    int32_t int_min_val;  // applies only to integers
    int32_t int_max_val;  // applies only to integers
    void* cb_arg;
} golioth_setting_t;

/// Private struct to contain settings state data, stored in
/// the golioth_coap_client_t struct.
typedef struct {
    bool initialized;
    golioth_setting_t settings[CONFIG_GOLIOTH_MAX_NUM_SETTINGS];
    size_t num_settings;
} golioth_settings_t;

/// Register a specific setting of type int
///
/// @param client Client handle
/// @param setting_name The name of the setting. This is expected to be a literal
///     string, therefore on the pointer is registered (not a full copy of the string).
/// @param callback Callback function that will be called when the setting value is
///     received from Golioth cloud
/// @param callback_arg General-purpose user argument, forwarded as-is to
///     callback, can be NULL.
///
/// @return GOLIOTH_OK - Setting registered successfully
/// @return GOLIOTH_ERR_MEM_ALLOC - Max number of registered settings exceeded
/// @return GOLIOTH_ERR_NOT_IMPLEMENTED - If Golioth settings are disabled in config
/// @return GOLIOTH_ERR_NULL - callback is NULL
golioth_status_t golioth_settings_register_int(
        golioth_client_t client,
        const char* setting_name,
        golioth_int_setting_cb callback,
        void* callback_arg);

/// Same as @ref golioth_settings_register_int, but with specific min and
/// max value which will be checked by this library.
golioth_status_t golioth_settings_register_int_with_range(
        golioth_client_t client,
        const char* setting_name,
        int32_t min_val,
        int32_t max_val,
        golioth_int_setting_cb callback,
        void* callback_arg);

/// Same as @ref golioth_settings_register_int, but for type bool.
golioth_status_t golioth_settings_register_bool(
        golioth_client_t client,
        const char* setting_name,
        golioth_bool_setting_cb callback,
        void* callback_arg);

/// Same as @ref golioth_settings_register_int, but for type float.
golioth_status_t golioth_settings_register_float(
        golioth_client_t client,
        const char* setting_name,
        golioth_float_setting_cb callback,
        void* callback_arg);

/// @}
