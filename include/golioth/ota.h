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

#include <stdint.h>
#include <golioth/golioth_status.h>
#include <golioth/client.h>
#include <golioth/config.h>

/// @defgroup golioth_ota golioth_ota
/// Functions for interacting with Golioth OTA services
///
/// https://docs.golioth.io/reference/protocols/coap/ota
/// @{

/// Size of a SHA256 of Artifact hex string in bytes
#define GOLIOTH_OTA_COMPONENT_HEX_HASH_LEN 64
/// Size of a SHA256 of Artifact bin array in bytes
#define GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN 32
/// Maximum size of Binary Detected Type in bytes
#define GOLIOTH_OTA_MAX_COMPONENT_BOOTLOADER_NAME_LEN 7
/// Maximum size of Relative URI to download binary (+ 7 bytes for Path)
#define GOLIOTH_OTA_MAX_COMPONENT_URI_LEN \
    (CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN + CONFIG_GOLIOTH_OTA_MAX_VERSION_LEN + 7)

/// State of OTA update, reported to Golioth server
enum golioth_ota_state
{
    /// No OTA update in progress
    GOLIOTH_OTA_STATE_IDLE,
    /// OTA is being downloaded and written to flash
    GOLIOTH_OTA_STATE_DOWNLOADING,
    /// OTA has been downloaded and written to flash
    GOLIOTH_OTA_STATE_DOWNLOADED,
    /// OTA is being applied to the system, but is not yet complete
    GOLIOTH_OTA_STATE_UPDATING,
};

/// A reason associated with state changes
enum golioth_ota_reason
{
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
    /// IO error while trying to store component
    GOLIOTH_OTA_REASON_IO,
    /// Awaiting retry
    GOLIOTH_OTA_REASON_AWAIT_RETRY,
};

/// A component/artifact within an OTA manifest
struct golioth_ota_component
{
    /// Artifact package name (e.g. "main")
    char package[CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN + 1];
    /// Artifact version (e.g. "1.0.0")
    char version[CONFIG_GOLIOTH_OTA_MAX_VERSION_LEN + 1];
    /// Size of the artifact, in bytes
    int32_t size;
    /// Artifact Hash
    uint8_t hash[GOLIOTH_OTA_COMPONENT_BIN_HASH_LEN];
    /// Artifact uri (e.g. "/.u/c/main@1.2.3")
    char uri[GOLIOTH_OTA_MAX_COMPONENT_URI_LEN + 1];
    /// Artifact bootloader ("mcuboot" or "default"")
    char bootloader[GOLIOTH_OTA_MAX_COMPONENT_BOOTLOADER_NAME_LEN + 1];
};

/// An OTA manifest, composed of multiple components/artifacts
struct golioth_ota_manifest
{
    /// OTA release sequence number
    int32_t seqnum;
    /// An array of artifacts
    struct golioth_ota_component components[CONFIG_GOLIOTH_OTA_MAX_NUM_COMPONENTS];
    /// Number of artifacts
    size_t num_components;
};

/// Convert raw byte payload into a @ref golioth_ota_manifest
///
/// @param payload Pointer to payload data
/// @param payload_size Size of payload, in bytes
/// @param manifest Output param, memory allocated by caller, populated with manifest
///
/// @retval GOLIOTH_OK payload converted to struct golioth_ota_manifest
/// @retval GOLIOTH_ERR_INVALID_FORMAT failed to parse manifest
enum golioth_status golioth_ota_payload_as_manifest(const uint8_t *payload,
                                                    size_t payload_size,
                                                    struct golioth_ota_manifest *manifest);

/// Convert a size in bytes to the number of blocks required (of size up to
/// GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE)
size_t golioth_ota_size_to_nblocks(size_t component_size);

/// Find a component by package name in a manifest, or NULL if not found.
///
/// @param manifest manifest to find component in
/// @param package package name to find in manifest
///
/// @return if found - pointer to component
/// @return if not found - NULL
const struct golioth_ota_component *golioth_ota_find_component(
    const struct golioth_ota_manifest *manifest,
    const char *package);

/// Observe for the OTA manifest asynchronously
///
/// This function will enqueue a request and return immediately without
/// waiting for a response from the server. The callback will be invoked whenever
/// the manifest is changed on the Golioth server.
///
/// @param client The client handle from @ref golioth_client_create
/// @param callback Callback function to register
/// @param arg Optional argument, forwarded directly to the callback when invoked. Can be NULL.
enum golioth_status golioth_ota_observe_manifest_async(struct golioth_client *client,
                                                       golioth_get_cb_fn callback,
                                                       void *arg);

/// Callback for OTA download component request
///
/// Will be called repeatedly, once for each block received from the server.
///
/// @param component The @ref golioth_ota_component pointer from the original request
/// @param block_idx The block number in sequence (starting with 0)
/// @param block_buffer The component payload in the response packet.
/// @param block_buffer_len The length of the component payload, in bytes.
/// @param is_last true if this is the final block of the request
/// @param negotiated_block_size The maximum block size negotated with the server, in bytes
/// @param arg User argument, copied from the original request. Can be NULL.
typedef enum golioth_status (*ota_component_block_write_cb)(
    const struct golioth_ota_component *component,
    uint32_t block_idx,
    const uint8_t *block_buffer,
    size_t block_buffer_len,
    bool is_last,
    size_t negotiated_block_size,
    void *arg);

/// Begin or resume downloading an OTA component synchronously.
///
/// This function will block until the final block of the component download is received from the
/// server or an error is received on any block. The `next_block_idx` is used to pass in the first
/// block index to be downloaded (components begin with the 0th index). After returning, the same
/// parameter has been updated to indicate the next expected block in the download process.
///
/// Use this function to resume OTA component downloads. Pass the `next_block_idx` from a failed run
/// to resume the download. The `ota_component_block_write_cb` function must be designed to handle
/// storage when resuming a download. Upon successful completion, it is up to the application to
/// verify the integrity of the component (eg: compare to the SHA256 in `struct
/// golioth_ota_component`).
///
/// @param client The client handle from @ref golioth_client_create
/// @param component One @ref golioth_ota_component instance present in the @ref
/// golioth_ota_manifest
/// @param[in,out] block_idx Pointer to the next expected block index. The function will
/// request the block index supplied as an input, when returning, this variable indicates the next
/// block index expected. When an error status is returned, use this value as the input for the next
/// resume operation. May be NULL to begin download with block_idx 0.
/// @param cb Callback function to register
/// @param arg Optional argument, forwarded directly to the callback when invoked. Can be NULL.
///
/// @retval GOLIOTH_OK the final block of package was received
/// @retval GOLIOTH_ERR_FAIL invalid client handle, path, or callback
/// @retval GOLIOTH_ERR_MEM_ALLOC unable to allocate necessary memory
enum golioth_status golioth_ota_download_component(struct golioth_client *client,
                                                   const struct golioth_ota_component *component,
                                                   uint32_t *block_idx,
                                                   ota_component_block_write_cb cb,
                                                   void *arg);

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
///           Must be at least GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE bytes.
/// @param block_nbytes Output param, memory allocated by caller, populated with number
///             of bytes in the block, 0 to GOLIOTH_BLOCKWISE_DOWNLOAD_MAX_BLOCK_SIZE.
/// @param is_last Set to true, if this is the last block
/// @param timeout_s The timeout, in seconds, for receiving a server response
///
/// @retval GOLIOTH_OK response received from server, get was successful
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
/// @retval GOLIOTH_ERR_TIMEOUT response not received from server, timeout occurred
enum golioth_status golioth_ota_get_block_sync(struct golioth_client *client,
                                               const char *package,
                                               const char *version,
                                               size_t block_index,
                                               uint8_t *buf,
                                               size_t *block_nbytes,
                                               bool *is_last,
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
/// @retval GOLIOTH_OK response received from server, get was successful
/// @retval GOLIOTH_ERR_NULL invalid client handle
/// @retval GOLIOTH_ERR_INVALID_STATE client is not running, currently stopped
/// @retval GOLIOTH_ERR_QUEUE_FULL request queue is full, this request is dropped
/// @retval GOLIOTH_ERR_TIMEOUT response not received from server, timeout occurred
enum golioth_status golioth_ota_report_state_sync(struct golioth_client *client,
                                                  enum golioth_ota_state state,
                                                  enum golioth_ota_reason reason,
                                                  const char *package,
                                                  const char *current_version,
                                                  const char *target_version,
                                                  int32_t timeout_s);

/// Get the current state of OTA update
///
/// @return The current OTA update state
enum golioth_ota_state golioth_ota_get_state(void);

/// @}

#ifdef __cplusplus
}
#endif
