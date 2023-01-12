# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
