/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <golioth/golioth_status.h>
#include <golioth/client.h>
#include <golioth/config.h>

/// @defgroup golioth_ota golioth_ota
/// Functions for interacting with Golioth OTA services
///
/// https://docs.golioth.io/reference/protocols/coap/ota
/// @{

/// Maximum size of an OTA block, in bytes
#define GOLIOTH_OTA_BLOCKSIZE 1024

/// State of OTA update, reported to Golioth server
typedef enum {
    /// No OTA update in progress
    GOLIOTH_OTA_STATE_IDLE,
    /// OTA is being downloaded and written to flash
    GOLIOTH_OTA_STATE_DOWNLOADING,
    /// OTA has been downloaded and written to flash
    GOLIOTH_OTA_STATE_DOWNLOADED,
    /// OTA is being applied to the system, but is not yet complete
    GOLIOTH_OTA_STATE_UPDATING,
} golioth_ota_state_t;

/// A reason associated with state changes
typedef enum {
    /// OTA update is ready to go. Also used for "no reason".
    GOLIOTH_OTA_REASON_READY,
    /// Firmware update was successful
    GOLIOTH_OTA_REASON_FIRMWARE_UPDATED_SUCCESSFULLY,
    /// Insufficient flash memory on device
    GOLIOTH_OTA_REASON_NOT_ENOUGH_FLASH_MEMORY,
    /// Insufficient RAM on device
    GOLIOTH_OTA_REASON_OUT_OF_RAM,
    /// Lost connection to server during OTA update
    GOLIOTH_OTA_REASON_CONNECTION_LOST,
    /// Data integrity check of downloaded artifact failed
    GOLIOTH_OTA_REASON_INTEGRITY_CHECK_FAILURE,
    /// Package type not supported
    GOLIOTH_OTA_REASON_UNSUPPORTED_PACKAGE_TYPE,
    /// URI not valid
    GOLIOTH_OTA_REASON_INVALID_URI,
    /// Firmware update was not successful
    GOLIOTH_OTA_REASON_FIRMWARE_UPDATE_FAILED,
    /// Protocol not supported
    GOLIOTH_OTA_REASON_UNSUPPORTED_PROTOCOL,
} golioth_ota_reason_t;

/// A component/artifact within an OTA manifest
typedef struct {
    /// Artifact package name (e.g. "main")
    char package[CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN + 1];
    /// Artifact version (e.g. "1.0.0")
    char version[CONFIG_GOLIOTH_OTA_MAX_VERSION_LEN + 1];
    /// Size of the artifact, in bytes
    int32_t size;
    /// True, if the component is compressed and requires decompression
    bool is_compressed;
} golioth_ota_component_t;

/// An OTA manifest, composed of multiple components/artifacts
typedef struct {
    /// OTA release sequence number
    int32_t seqnum;
    /// An array of artifacts
    golioth_ota_component_t components[CONFIG_GOLIOTH_OTA_MAX_NUM_COMPONENTS];
    /// Number of artifacts
    size_t num_components;
} golioth_ota_manifest_t;

/// Convert raw byte payload into a @ref golioth_ota_manifest_t
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
/// @param manifest Output param, memory allocated by caller, populated with manifest
///
/// @return GOLIOTH_OK - payload converted to golioth_ota_manifest_t
/// @return GOLIOTH_ERR_INVALID_FORMAT - failed to parse manifest
golioth_status_t golioth_ota_payload_as_manifest(
        const uint8_t* payload,
        size_t payload_size,
        golioth_ota_manifest_t* manifest);

/// Convert a size in bytes to the number of blocks required (of size up to GOLIOTH_OTA_BLOCKSIZE)
size_t golioth_ota_size_to_nblocks(size_t component_size);

/// Find a component by package name in a manifest, or NULL if not found.
///
/// @param manifest manifest to find component in
/// @param package package name to find in manifest
///
/// @return if found - pointer to component
/// @return if not found - NULL
const golioth_ota_component_t* golioth_ota_find_component(
        const golioth_ota_manifest_t* manifest,
        const char* package);

/// Observe for the OTA manifest asynchronously
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. The callback will be invoked whenever
/// the manifest is changed on the Golioth server.
golioth_status_t golioth_ota_observe_manifest_async(
        golioth_client_t client,
        golioth_get_cb_fn callback,
        void* arg);

/// Get a single artfifact block synchronously
///
/// Since some artifacts (e.g. "main" firmware) are larger than the amount of RAM
/// the device has, it is assumed the user will call this function multiple times
/// to handle blocks one at a time.
///
/// This function will block until one of three things happen (whichever comes first):
///
/// 1. A response is received from the server
/// 2. The user-provided timeout_s period expires without receiving a response
/// 3. The default GOLIOTH_COAP_RESPONSE_TIMEOUT_S period expires without receiving a response
///
/// @param client The client handle from @ref golioth_client_create
/// @param package Package name, to identify the artifact
/// @param version Version of package, to identify the artifact
/// @param block_index 0-based index of block to get
/// @param buf Output param, memory allocated by caller, block data will be copied here.
///           Must be at least GOLIOTH_OTA_BLOCKSIZE bytes.
/// @param block_nbytes Output param, memory allocated by caller, populated with number
///             of bytes in the block, 0 to GOLIOTH_OTA_BLOCKSIZE.
/// @param is_last Set to true, if this is the last block
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @return GOLIOTH_OK - response received from server, get was successful
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
/// @return GOLIOTH_ERR_TIMEOUT - response not received from server, timeout occurred
golioth_status_t golioth_ota_get_block_sync(
        golioth_client_t client,
        const char* package,
        const char* version,
        size_t block_index,
        uint8_t* buf,
        size_t* block_nbytes,
        bool* is_last,
        int32_t timeout_s);

/// Report the state of OTA update to Golioth server synchronously
///
/// @param client The client handle from @ref golioth_client_create
/// @param state The OTA update state
/// @param reason The reason for this state change
/// @param package The artifact package name
/// @param current_version The artifact current_version. Can be NULL.
/// @param target_version The artifact new/target version from manifest. Can be NULL.
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @return GOLIOTH_OK - response received from server, get was successful
/// @return GOLIOTH_ERR_NULL - invalid client handle
/// @return GOLIOTH_ERR_INVALID_STATE - client is not running, currently stopped
/// @return GOLIOTH_ERR_QUEUE_FULL - request queue is full, this request is dropped
/// @return GOLIOTH_ERR_TIMEOUT - response not received from server, timeout occurred
golioth_status_t golioth_ota_report_state_sync(
        golioth_client_t client,
        golioth_ota_state_t state,
        golioth_ota_reason_t reason,
        const char* package,
        const char* current_version,
        const char* target_version,
        int32_t timeout_s);

/// Get the current state of OTA update
///
/// @return The current OTA update state
golioth_ota_state_t golioth_ota_get_state(void);

/// @}
