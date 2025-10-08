# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [0.21.0] 2025-10-08

### Breaking Changes
- The function for subscribing to changes to the OTA manifest has
  been renamed from `golioth_ota_observe_manifest_async()` to
  `golioth_ota_manifest_subscribe()`. This name change reflects the
  new subscription behavior described below. Applications using
  Golioth's reference firmware update implementation do not require
  any changes.
- Applications which use Golioth's sample library for storing WiFi
  credentials will need to maintain that functionality themselves
  going forward or migrate to Zephyr's WiFi Credentials library
  (see below).

### Added

- The OTA service now periodically polls for updates to the OTA
  manifest, in addition to using CoAP observations to receive
  asynchronous notifications of changes to the manifest. The
  polling period defaults to 1 day and is configurable using
  `CONFIG_GOLIOTH_OTA_MANIFEST_SUBSCRITION_POLL_INTERVAL_S`.

### Changed

- Zephyr samples use Zephyr's native WiFi Credentials library for
  storing WiFi network information, instead of the previous custom
  solution. This provides greater flexibility in the selection of
  WiFi security and storage backend options.
- Zephyr support has been upgraded to v4.2.1.
- nRF Connect SDK support has been upgraded to v3.1.1.

## [0.20.0] 2025-08-25

### Added

- The Gateway service supports certificate operations

### Changed

- The Gateway service pulls downlink data as a response to uplink
- The Gateway service targets a different path

## [0.19.1] 2025-08-13

### Fixed:

- Fixed compilation errors and warnings when disabling Golioth logging
- Fixed an error log related to CoAP option lengths
- Fixed garbage appended to path when receiving blockwise responses

## [0.19.0] 2025-07-24

### Changed

- Based on experience gained during the Private Access phase, the
  Golioth Location service has been changed from a dedicated service
  to a Pipelines Transformer. The experimental firmware service has
  been removed and the Location examples have been updated to use
  Pipelines. A new `net_info` utility has been added to facilitate
  constructing payloads appropriate for use with the transformer.

### Fixed:

- Fixed a NULL dereference that occurred when processing certain errors
  in CoAP blockwise operations.
- Fixed some internal log messages being to sent to Golioth even when
  turned off in Kconfig.
- Fixed unsigned-compare-against zero in certain error paths
- Fixed memory leak during blockwise transfers

### Added

- The CoAP client can now receive blockwise responses to blockwise
  posts.
- Added an API for receiving OTA manifests blockwise, to support
  manifests larger than 1 kB.
- The SDK is now scanned by Coverity Static Analysis

## [0.18.1] 2025-06-02

### Fixed

- OTA: Fixed a bug that could result in hung downloads during high
  traffic events.
- Zephyr: Fixed a bug that prevented the keepalive timer from working,
  resulting in disconnects during periods of no traffic.

### Added

- Examples: Added an option to get credentials from the host
  environment when using Zephyr's `native_sim` platform.

## [0.18.0] 2025-05-07

### Breaking Changes

- Zephyr: The default maximum path length for Golioth APIs has changed
  from 39 characters to 12 characters and the
  `CONFIG_GOLIOTH_COAP_MAX_PATH_LEN` symbol can no longer be changed by
  the user. Update this limit using the following Kconfig symbols
  supplied by Zephyr's CoAP library:
    ```
    CONFIG_COAP_EXTENDED_OPTIONS_LEN=y
    CONFIG_COAP_EXTENDED_OPTIONS_LEN_VALUE=39
    ```
- OTA: golioth_ota_download_component is now non-blocking, and the
  function signature has changed to support two callbacks - one for
  reading blocks called 0 or more times, and one that is called exactly
  one time at the end of the download.

### Removed

- QEMU is no longer continually verified. Users should use `native_sim`
  for emulated testing.
- i.MX RT1024 is no longer continually verified. It is replaced with
  the FRDM-RW612.
- Zephyr: Hardcoded WiFi and TLS credentials are no longer supported.
  Users should use the provided shell functions to set WiFi and TLS
  credentials instead.

### Changed

- Zephyr support upgraded to v4.1.0
- nRF Connect SDK support upgraded to v3.0.1
- ESP-IDF support upgraded to v5.4.1
- The FW Update rollback timer is now configurable, and the default is
  changed from 60 seconds to 300 seconds.

### Added:

- FW Update now checks if an artifact is already stored before
  initiating a download. This will prevent excessive network and flash
  operations in this case that an update cannot be applied.
- Multipart API for blockwise uploads.
- Continually verify on FRDM-RW612
- Added an API for on-demand fetching of the OTA manifest
- Support PSA crypto API
- EXPERIMENTAL: Gateway service for proxying pouches to Golioth

### Fixed

- Zephyr: correctly detect path names that are longer than the maximum
  setting.
- Various typos and formatting
- Plugged a memory leak when using SHA256

## [0.17.0] 2025-01-23

### Highlights:

- Added New Golioth Location Service
- Improved OTA stability and robustness
- ESP-IDF port updated to ESP-IDF v5.4
- ModusToolbox port updated to ModusToolbox v3.3

### Added:

- New APIs for interacting with the Golioth Location service. Golioth
  Location can be used to resolve WiFi scan results and cellular tower
  information into approximate geolocation.
- New Zephyr examples for the Golioth Location service.
- fw_update: Resume downloads
- fw_update: Retry downloads with backoff
- fw_update: Add retries for reporting state

### Changed:

- Default PSK max length set to 32 to match mbedTLS defaults
- Improved WiFi handling in samples
- Improved handling of new OTA manifests when download is in progress

## [0.16.0] 2024-11-25

### Breaking Changes:

- All asynchronous callbacks now have both a `status` member and a
  `coap_rsp_code` member to replace the `response` member. All of the
  same information remains accessible. Update callback functions to
  match the new declaration and change any `response->status` checks to
  `status`.
- `golioth_ota_download_component()` has a new `uint32_t
  *next_block_idx` parameter. Use this to resume block download. Set to
  `NULL` to use previous functionality in existing code.
- The parameters for `ota_component_block_write_cb()` have changed to
  include `block_buffer_len` for the actual length of data and
  `negotiated_block_size` to indicate the maximum block size (may be
  used along with `block_idx` to calculate a byte offset).
- `golioth_ota_component->hash` is now stored as an array of bytes
  instead of as a hex string.

### Highlights:

- Zephyr port updated to Zephyr v4.0
- NCS port updated to NCS v2.8
- Improved OTA stablility

### Added:

- ESP-IDF: optional ipv6 support enabled by
  `CONFIG_LIBCOAP_ENABLE_IPV6_SUPPORT`
- LightDB/OTA/RPC: log message when an error response is received from
  server
- CoAP: Server-negotiated block size for blockwise uploads
- CoAP: optionally call a `set_cb` callback at the end of a blockwise
  upload operation
- OTA: ability to resume a component download
- `golioth_sys_sha256_*()` API for calculating OTA component hash
- `CONFIG_GOLIOTH_OTA` to enable OTA component separately from fw_update
  component
- Numerous hardware-in-the-loop (HIL) testing improvements for both code
  samples and integration tests

### Changed:

- Certificates: Replace ISRG Root X2 CA certificate with Golioth
  Root X1 CA certificate.
- Zephyr: Samples: kconfig and devicetree settings common to an SoC
  moved from `boards` directory to `socs` directory.

### Fixed:

- Zephyr: Golioth coap client log messages now honor changes to the
  logging level.
- Zephyr: Fixed off-by-one error in Golioth backend logging message
  length limit.
- Zephyr: Connection ID is now properly enabled by Kconfig setting.
- Zephyr: Run user callbacks when cancelling requests.
- Linux: Error checks and max PEM size for certificate_auth sample.

### Removed:

- OTA compression was removed as the feature is currently unsupported on
  the servers side.
- Golioth Basics sample removed from Zephyr and ESP-IDF. Existing
  per-feature sample code for these platforms covers everything
  demonstrated in that sample.

### Known Issues:
- [Zephyr only] examples won't build for esp32_devkitc_wrover with
  support for certificates due to bugs in Zephyr that prevent all RAM
  banks from being made available to the application.

## [0.15.0] 2024-09-04

### Highlights:

- Zephyr port updated to Zephyr v3.7
- NCS port updated to NCS v2.7
- ESP-IDF port updated to ESP-IDF v5.3

### Added:

- `native_sim` platform support in Zephyr's twister execution of
  `fw_update` example

### Changed:

- Use `VERSION` file to specify application version in `fw_update`
  example
- Better error message on manifest decode error
- Updated libcoap version to v4.3.4a
- Merged `GOLIOTH_SAMPLE_{,PSK_}SETTINGS` Kconfig settings
- Added `extern "C"` in headers for improved C++ compatibility

### Fixed:

- CoAP keepalive behavior with Zephyr
- Memory leak by freeing post_block memory in `purge_request_mbox()`
- Disabled FreeRTOS logging backend on client disconnect
- Stop client during client destroy in `golioth_client_destroy()`

### Known Issues:

- [Zephyr only] Espressif devices have a regression in Zephyr 3.7 that
  frequently causes crashes during flash operations. We recommend remaining
  on Zephyr 3.6 and Golioth Firmware SDK [0.14.0] until the issue is
  is resolved. For more information see
  https://github.com/zephyrproject-rtos/zephyr/issues/77952
- [Zephyr only] Boards utilizing the [ESP AT WiFi
  driver](https://docs.zephyrproject.org/3.7.0/kconfig.html#CONFIG_WIFI_ESP_AT),
  such as the nRF52840 DK + ESP32, frequently fail to complete DTLS
  handshakes when using certificate authentication due to an issue with
  DTLS fragment handling when operating in passive mode, which is
  required for DNS resolution in Zephyr 3.7. For more information see
  https://github.com/zephyrproject-rtos/zephyr/issues/77993

### Workarounds:

- The MIMXRT1024-EVK received a new upstream Ethernet driver that was
  found to have sporadic "Link Down/Up" events that cause a loss of
  connectivity when downloading binaries during OTA Firmware Update. All
  Zephyr examples in this release include an overlay file for this board
  that reverts to the old driver.

## [0.14.0] 2024-06-21

### Highlights:

- Added blockwise uploads for streaming data to Pipelines
- Improved efficiency of blockwise operations
- Reduced log verbosity

### Added:

- A new `golioth_stream_set_blockwise_sync()` API for uploading
  larger payloads to Pipelines. This is useful when the size of your
  payload exceeds the MTU of your underlying transport (typically 1024
  bytes).
- native_sim Zephyr target is now tested in CI

### Changed:

- CoAP retransmissions are no longer reported individually. Instead, a
  count of the number of retransmissions in the last 10 seconds is
  reported. This reduces the chattiness of CoAP client logs.
- Blockwise transfers make more efficient use of semaphores and
  allocations, reducing CPU overhead, overall memory usage, and
  heap fragmentation.
- Blockwise upload user callbacks are now passed the size of the block,
  and should no longer rely on a hardcoded block size.
- Reduced log levels of some messages from INFO to DEBUG.

### Fixed:

- Gracefully handle allocation errors during synchronous operations.
- Honor content-type in blockwise uploads
- Small typo in Kconfig help

### Removed

- Removed BLE provisioning service from Golioth ESP-IDF examples. Users
  are free to copy this utility into their own repos and use it if they
  desire. This code is offered as-is and without warranty.

## [0.13.1] 2024-05-31

### Fixed:

- Infinite loop in OTA manifest observation retry logic that occurred
  when retries were exhausted.
- Documentation: update links to Stream service

### Changed

- Registering an OTA manifest observation will be retried until
  successful, with a delay that doubles after each unsuccessful attempt.
  Use CONFIG_GOLIOTH_FW_OBSERVATION_RETRY_MAX_DELAY_MS to configure the
  maximum delay (defaults to 5 minutes).

## [0.13.0] 2024-05-28

### Breaking Changes:

- Added content type parameter to golioth_lightdb_observe_async().
  Previously this defaulted to JSON. Existing projects can add
  GOLIOTH_CONTENT_TYPE_JSON as the third param of this API call to
  replicate behavior prior to this change.

### Highlights:

- Added the ability to cancel RPC and Settings observations
- Updated Stream example and documentation to show usage of Golioth's
  Pipelines feature.
- Exposed additional APIs for downloading OTA components.
  - This can be used to implement more complex update scenarios in the user
    application (i.e. outside of the SDK).
  - Note that users who choose to implement their own update logic will need to
    manage applying the update and reporting state back to Golioth, tasks which
    are handled by the built-in, but more limited, firmware update module in
    the SDK.

### Added:

- Stream example for ESP-IDF
- Stream service now accepts binary (OCTET_STREAM) data
- Log on failure to send settings response
- Add retry logic to firmware update block download
- Settings response length is now configurable
- New Blockwise Transfer module, for CoAP blockwise uploads and downloads
- Add support to the CoAP client for eager release of observations
- RPC: add ability to cancel observations
- Settings: add ability to cancel observations
- Added support for resolving IPv6 addresses on Thread networks
- Added `golioth_ota_download_component()` API to OTA service

### Fixed:

- Fixed swapped sync/async messages in Stream example
- Stream example now works correctly with on-board temperature sensors
- Target version is now set when reporting OTA status to UPDATING
- Fixed memory leak when maxing out observations
- LightDB State gets of JSON objects would strip leading and trailing
  curly brackets
- Fixed potential buffer overflow during string copy
- Removed unused Kconfig symbols

### Changed

- Kconfig: reorganized how Kconfig options are presented in the
  menuconfig interface.
- Use CBOR in Stream example instead of JSON
- Contribution and Style guides have been updated

## [0.12.2] - 2024-05-02

### Removed:

- Removed automatic ESP-IDF logging - this will be reintroduced in a future
  release with more configuration options.

### Added:

- Improved "client not running" error logs with additional context

### Fixed:

- Zephyr: Fix crash when repeatedly stopping and starting the client
- Zephyr: Fix up to 10 second delay when stopping client
- Zephyr: Fixed hang when attemtping to stop an already stopped client
- Fixed dropped function call when `assert()` is disabled
- Return error instead of crashing on NULL input

## [0.12.1] - 2024-04-22

### Added:

- ESP-IDF: added ability to send native logging system messages to the
  Golioth remote logging service.

### Changed:

- OTA firmware update observation will retry multiple times when failing
  to register.
- Status added to the CoAP request struct for passing error codes.

### Fixed:

- Zephyr: correctly handle the client connection stop command.
- Zephyr: reestablish observations after reconnect.

## [0.12.0] - 2024-04-15

### Breaking Changes:

- Previously, it was possible to implicitly include default Kconfig
  settings from the Zephyr examples into client applications. This was
  unintentional and has been fixed. As a result, if your application was
  relying on those defaults, you'll now need to set those Kconfig values
  explicitly in your application.

### Added:

- Add socket hello_nrf91_offload Zephyr example to demonstrate DTLS
  sockets offload.
- Zephyr: add rak5010_nrf52840 support to all examples
- ESP-IDF: add fw_update example to demonstrate OTA firmware update
- ESP-IDF: add settings example to demonstrate device settings service

### Changed:

- Zephyr: example code now explicitly sources Kconfig.defconfig file to
  select necessary Kconfig symbols from the example common files.
- Zephyr: update runtime settings in examples to take effect without
  needing a reboot
- Update libcoap version to v4.3.4a

### Fixed:

- Documentation updates
- Certificate Provisioning: Increase nRF52840 network buffers to avoid
  failing handshakes

## [0.11.1] - 2024-03-20

### Known Issues:

- Default Kconfig values may not propagate correctly if the Golioth SDK
  is not listed first in the West manifest. To workaround this, ensure
  that Golioth is listed first in your application's manifest.

### Added:

- CONFIG_GOLIOTH_RPC_MAX_RESPONSE_LEN to control the size of the buffer
  used to hold the RPC response

### Changed:

- Improved how the Golioth Zephyr log backend handles network disconnects
  to avoid interfering with the application's control of logging levels
- Removed default dependency on floating point support

### Fixed:

- Enable log shell commands in Zephyr Certificate Provisioning example
- Build error when compiling with NCS and without newlib
- Correctly handle TOO_MANY_REQUESTS responses from the server in NCS

## [0.11.0] - 2024-03-13

### Highlights

- Zephyr port updated to Zephyr v3.6
- NCS port updated to NCS v2.5.2
- ESP-IDF port updated to ESP-IDF v5.2.1
- New examples for ESP-IDF
- Initial support for `native_sim` target in Zephyr

### Added:

- New examples for ESP-IDF: `hello`, `rpc`, `lightdb state`
- Additional hardware-in-the-loop integration tests
- `west patch` command for applying git patches
- `native_sim` support (requires patches on top of Zephyr v3.6)
- nRF91 LTE monitor log level is now configurable
- Zephyr console now returns an error when attempting to store a PSK-ID
  or PSK that is too long

### Changed:

- Improved NACK handling during initial connection
- OTA module now decodes all items in a manifest
- RPC returns `NOT_FOUND` instead of `UNKNOWN` when receiving an RPC
  for an unregistered method
- Improved and clarified documentation
- Improved and stablized HIL test infrastructure
- Documentation updated to reflect change from Device Name to Certificate
  ID in device certificates

### Removed:

- Individual type functions for Stream service
- Zephyr port no longer depends on `CONFIG_POSIX_API`

### Fixed:

- Fixed range checking for int type settings
- Fixed incorrect error code for receiving an unknown setting
- Fixed crash when disabling Zephyr log backend
- Fixed buffer allocation failures on nRF52840DK
- Fixed `free()` of unallocated buffer
- Zephyr CoAP client was not notifying port layer of disconnections
- Some kconfig settings had no effect in the ESP-IDF port
- Some kconfig options could not be overriden in the Zephyr port

## [0.10.0] - 2024-01-31

### Highlights
- The Zephyr CoAP library is now used instead of libcoap for Zephyr ports
- Zephyr ports can specify a TLS credential tag to use for authentication,
  in particular this will enable the use of offloaded DTLS sockets.

### Breaking Changes
- Remove `golioth_` prefix from filenames and move header files under
  `golioth/` folder.
- Remove `golioth.h` header - users must now include individual headers
  for each service that they use.
- Stream and State: change `json` and `cbor` object set calls to specify
  type in the parameters (instead of separate function calls).
- RPC: This service must now be initialized prior to use by calling
  `golioth_rpc_init(client);`
- Settings: This service must now be initialzied prior to use by calling
  `golioth_settings_init(client);`
- Services (e.g. stream, fw_update, etc.) are disabled by default - be
  sure to enable them in your project configuration.
- Typedefs have been removed throughout the codebase - instead use the
  underlying struct or enum types
- Remove `lightdb_` from `lightdb_stream` functions to differentiate from LIghtDB State

### Added:
- Automated Hardware-in-the-Loop test framework
- Basic cross-platform connection HIL test
- Tests for all Zephyr examples
- Add kconfig option for default log level for SDK logs
- Lightb State examples now show how to use CBOR

### Changed:
- Improved network connection management
- Wait for network traffic and request queue concurrently - this
  removes the need to wakeup every 1 second to check for requests
- Pass structs by reference instead of value in golioth_sys_* APIs
- Use bool instead of int for binary Kconfig options
- Improved organization of Kconfig options
- ESP-IDF examples use FreeRTOS APIs directly instead of Golioth
  abstractions
- LightDB State and Stream are now separate services in the SDK
- DFU example has been renamed to `fw_update`
- Use golioth_content_type enum instead of libcoap content types

### Removed:
- Magtag demo for ESP-IDF - this hardware is out of stock
- Golioth time module - this was a thin wrapper around golioth_sys
- golioth_statistics module - this internal memory tracking module
  is being replaced by out-of-source tooling
- remote_shell - This was an experimental feature that was never
  released. If you are interested in remote shell functionality in
  the future, please reach out to Golioth!
- Integration guide - see docs.golioth.io for the latest documentation
  on integration Golioth into your product

### Fixed:
- Various documentation fixes
- Removed double check for CONFIG_GOLIOTH_SAMPLE_COMMON
- Fix stack overflow in logging thread
- Drop logs with unsupported log levels
- Attempting to use a disabled service now fails at build time instead
  of at run time

## [0.9.0] - 2023-11-08

### Added:
- zephyr: Add CONFIG_GOLIOTH_USE_CONNECTION_ID symbol to enable
  Connection IDs
- zephyr: support DTLS 1.2 Connection IDs with NCS (disabled by default)
- zephyr: samples: enable reboot
- ci: add HIL test workflow
- zephyr: samples: add connection test

### Changed:
- zephyr: Use Zephyr 3.5.0
- zephyr: Use NCS 2.5.0
- moved src/include folder to root directory
- moved src/priv-include folder to src/include
- zephyr: Use random subsystem provided by Zephyr
- ci: parallelize workflows

### Known Issues:
- FlexSPI issue in Zephyr 3.5.0 may cause DFU to hang for NXP chips
  during the flash memory erase step. This issue has already been
  addressed with a recent commit:
  https://github.com/zephyrproject-rtos/zephyr/commit/9dd8f94fd47b8b399c230fac2c24e4949cc25b99
  Golioth has confirmed that this commit fixes the DFU issue on NXP
  boards, but we have not performed full verification of the rest of the
  SDK against this revision of Zephyr. Work around the issue by updating
  the Zephyr revision number in west-zephyr.yml to that commit SHA
  number (or newer).

## [0.8.0] - 2023-09-22

### Highlights
- Beta support for Zephyr and NCS
- DTLS 1.2 Connection ID Support
- Update ESP-IDF support to v5.1.1
- Use CBOR instead of JSON internally
### Breaking Changes
- Add `GOLIOTH_` to each RPC status (e.g. `GOLIOTH_RPC_OK`)
### Added:
- zephyr: add support for certificate authentication
- zephyr: add Golioth log backend
- zephyr: examples: add examples for each Golioth service
- zephyr: examples: add support for runtime DTLS PSK and WiFi settings
- zephyr: examples: add hardcoded credentials module
- zephyr: examples: add support for ESP32-DevKitC-WROVER
- zephyr: examples: add support for MIMXRT1024-EVK
- ports: add connect/disconnect notification to port layer
- ncs: add support for Nordic nRF SDK
- ncs: examples: add support for nRF9160 DK
### Changed:
- auth: use DER formatted certificates
- linux: examples: source PSK and PSK-ID from environment
- treewide: use CBOR (zcbor) instead of JSON for Golioth services
- zephyr: update zephyr revision
- libcoap: update libcoap revision
### Fixed:
- docs: fixed typos and broken links
- coap_client: handle NACKed CoAP requests
- zephyr: libcoap: removed broken assert
- zephyr: use work thread for timer handlers
- mbox: fixed mbox failure when asserts enabled

## [0.7.0] - 2023-06-15

Highlights:

- Initial support for Zephyr targets

### Added:
- rpc: add unit tests
- zephyr: add Zephyr port
- zephyr: add DFU support
- examples: add golioth_basics example for Zephyr
- golioth_coap_client: introduce GOLIOTH_OVERRIDE_LIBCOAP_LOG_HANDLER
- remote_shell: add CONFIG_GOLIOTH_REMOTE_SHELL_THREAD_STACK_SIZE
- ota: add CONFIG_GOLIOTH_OTA_THREAD_STACK_SIZE
### Changed:
- treewide: change 'task' nomenclature to thread
- treewide: replace '#define TAG ...' with LOG_TAG_DEFINE(...)
- esp_idf: convert GOLIOTH_REMOTE_SHELL_ENABLE to bool
- port: modus_toolbox: define HAVE_PTHREAD_{H,MUTEX_LOCK}
- port: esp_idf: define HAVE_PTHREAD_{H,MUTEX_LOCK}
- port: remove COAP_RESOURCES_NOHASH definition
- README: clarify the support for OTA on Linux
- libcoap: bump to tip of develop branch
### Fixed:
- settings: allow float type settings to be whole numbers
- settings: update device_settings_register_bool docstring
- ota: fix typo in error message when creating thread
- scripts: only suggest running clang-format on noncompliant files
- rpc: fixed typos in doc strings

## [0.6.0] - 2023-02-24

Highlights:

- New feature: OTA zlib decompression (alternative to heatshrink)
- New feature: OTA delta updates (bsdiff + zlib)

### Added
- .gitmodules: add miniz library
- .gitmodules: add bsdiff library
- .github: build modus_toolbox project in CI
- port: add read_current_image_at_offset() function, required to implement by ports
- fw_update: new option to decompress using zlib algorithm
- fw_update: new feature, delta FW updates
- scripts: add OTA scripts to compress and generate patch files
- docs: add Modus Toolbox to integration guide
### Changed
- golioth_basics: change RPC method from "double" to "multiply"
- libcoap: update to tip of develop branch
- heatshrink: update to tip of develop branch, build without dyn mem allocation
- fw_update: internal restructuring, process blocks through a chain of functions
- config: for decompression, must select either heatshrink or zlib algorithm
### Fixed
- shell: verify key is correct before accessing NVS
- port/linux: handle EINTR correctly in golioth_sys_sem_take()
- coap_client: allow for no block2 option in block req response, fixes single-block downloads
- examples/esp_idf: allow for up to 32 chars for wifi creds (was 31)

## [0.5.0] - 2023-01-12

Highlights:

- Support for ESP-IDF v5.0. This is the new recommended version. IDF 4.x is still supported.
- Support for decompressing compressed OTA artifacts
- Support for certificate authentication when connecting to Golioth
- No breaking changes from prior release

### Added
- client: new API golioth_client_wait_for_connect(), to block until connected to Golioth
- new submodule: heatshrink
- fw_update: support for decompressing compressed OTA artifacts, using heatshrink library
- fw_update: new API golioth_fw_update_state_change_callback(), notification of state changes
- fw_update: new API golioth_fw_update_init_with_config(), for more detailed configuration
- config: new configuration to enable/disable OTA decompression
- config: new configuration to set the default log level of the Golioth SDK
- linux: option to download OTA images and store as a file
- scripts: generate project root and device certificates
- support for using SDK in C++ projects, extern "C" in headers, new esp-idf example
- examples/esp_idf: new example `cpp`, demonstrating usage of SDK in C++ project
- examples/{esp_idf,linux}: new `certificate_auth` example
- docs: new document, Flash_and_RAM_Usage.md
### Changed
- esp-idf: libcoap is a separate IDF component now
- docs: improved integration guide for ESP-IDF
- esp-idf: configuration optimizations to reduce total flash size of binary
- wifi: reduce log level to WARN, to avoid spammy logs at init time
### Fixed
- Miscellaneous printf formatting warnings, e.g. converting `%d` to `PRId32`
- examples,wifi: fix potential strncpy size issue, NULL termination
- Where malloc is called, check for NULL before attempting to memset
- log: dynamically allocate JSON serialized string, to avoid 100 character limit
- linux: golioth_sys_msleep now sleeps the correct amount of time, uses nanosleep
- coap_client: only add URI path option to packet if path length is > 0
- coap_client: use the accept option on all GET requests (was incorrectly using content-type)

## [0.4.0] - 2022-11-18

### Breaking Changes
- Removed `golioth_settings_register_callback` function and redesigned the API.
  The new API functions are oriented around registering a callback for an
  individual setting of a known type
  (e.g. `golioth_settings_register_int(client, "MY_SETTING", on_my_setting, NULL);`).
  The new APIs are demonstrated in the `golioth_basics` example code, which can be
  used as a guide for migrating existing code that uses the old APIs.
- golioth_client: remove golioth_client_task_stack_min_remaining()
### Added
- Introduced a platform abstration layer (golioth_sys), to make porting easier
- Linux port, for building SDK natively on Linux (or cross-compile for embedded Linux)
- docs: new porting guide
- Experimental "remote shell" feature, disabled by default
- New API: lightdb_stream_set_cbor_async/sync
- settings: support for reporting an array of settings errors to cloud
- unit test framework
- new git submodules: unity, fff, cJSON
- golioth_log: add log level NONE, to disable logging entirely
- golioth_log: GLTH_LOG_BUFFER_HEXDUMP macro
- ci: run test_esp32s3.yml on a daily schedule
- docs: Added Style_Guide.md
### Changed
- libcoap: upgrade submodule to 4.3.1
- DTLS: better detection and logging of handshake failures
- esp_idf: upgrade from v4.4.1 to v4.4.2
- settings: removed name limit of 15 chars, now not limited
- golioth_client: on stop(), block until client thread actually stopped
- golioth_client: log "Golioth connected" when first connected
- GLTH_LOGX macro now logs to Golioth cloud, if configured (enabled in examples)
- golioth_basics: replace golioth_log* with GLTH_LOGX
- ci: update to actions/checkout@v3 to avoid warnings
- examples,modus_toolbox: require user to create credentials.inc with WiFi/Golioth creds.
- ci: test against golioth.dev (dev server) instead golioth.io (prod server)
- port:esp_idf,modus_toolbox: use golioth_sys abstraction layer
- README: table of SDK features, by platform
### Fixed
- README: fix missing nvs_init in minimal example
- Use inttypes.h for printf formatters (e.g. PRIu64), to avoid compiler warnings
- coap_client: purge request queue on golioth_client_destroy()
- port,mtb: override GLTH_LOGX to avoid 64-bit printf formwatters (not supported on MTB)
- esp_idf,shell: better support for different serial configs, e.g. Adafruit ESP32-S3 USB JTAG

## [0.3.0] - 2022-10-04

### Added
- Added support for ESP-IDF platform. Imported all code, docs, and scripts from the
  golioth-esp-idf-sdk repo to here.
- Imported ModusToolbox example code under examples/modus_toolbox/golioth_basics.
- libcoap is now a git submodule
### Changed
- Applied clang-format code formatting to all source files
### Fixed
- golioth_fw_update: abort OTA if block handler fails
- esp_idf,test: use custom partition table, larger image size

<!-- Note: Skipped over version 0.2.0, to avoid confusion with 0.2.0 release
of the (now outdated) golioth-esp-idf-sdk repo. -->

## [0.1.0] - 2022-09-29
### Added
- Initial release, only supports ModusToolbox.
