# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Kconfig: reorganized how Kconfig options are presented in the
  menuconfig interface.

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
