# Golioth Firmware SDK

A software development kit for connecting embedded devices to the
[Golioth](https://golioth.io) IoT cloud.

This SDK can be used on any preemptive operating system that provides an
Internet stack. Golioth maintains ports for the following platforms:

* [Espressif ESP-IDF](examples/esp_idf/README.md)
* [Infineon ModusToolbox](examples/modus_toolbox/README.md)
* [Zephyr & NCS](examples/zephyr/README.md) (Beta)
* [Linux](examples/linux/README.md) (Experimental)

More information on these ports can be found at the links above.

The SDK can be ported to additional platforms by following the Porting Guide in
the `docs` folder.

SDK source: https://github.com/golioth/golioth-firmware-sdk

Documentation: https://docs.golioth.io/firmware/golioth-firmware-sdk

API documentation: https://firmware-sdk-docs.golioth.io/

## Getting Started

### Getting the Code

This repo uses git submodules, so you will need to clone with the `--recursive` option:

```sh
git clone --recursive https://github.com/golioth/golioth-firmware-sdk.git -b v0.7.0
```

Or, if you've already cloned but forgot the `--recursive`, you can update and
initialize submodules with this command:

```sh
cd golioth-firmware-sdk
git submodule update --init --recursive
```


> :warning: **Note:** Zephyr-based projects should use the West tool to clone the repo. See the
[Zephyr Quick Start Guide](exampls/zephyr/README.md).

### Trying the SDK examples

The `examples` directory contains example apps which you can build and run.
It is organized by platform (e.g. ESP-IDF, ModusToolbox, etc),
so navigate to a specific platform directory and check out the README for further
instructions to build and run the examples.

The `golioth_basics` example (`examples/<platform>/golioth_basics`) is recommended
as a starting point, to learn how to connect to Golioth and use services like
[LightDB State](https://docs.golioth.io/cloud/services/lightdb),
[LightDB Stream](https://docs.golioth.io/cloud/services/lightdb-stream),
[Logging](https://docs.golioth.io/cloud/services/logging),
and [OTA](https://docs.golioth.io/cloud/services/ota).

### Additional Documentation

The `docs` folder contains additional documentation, such as a platform
integration guide and a platform porting guide.

## Verified Devices

The following table lists the different hardware configurations we test the SDK against,
and when it was last tested.

The test itself covers most major functionality of the SDK, including connecting
to the Golioth server, interacting with LightDB State and Stream, and performing
OTA firmware updates.

The test procedure is (e.g. for ESP-IDF):

```
cd examples/esp_idf/test
idf.py build
idf.py flash
python verify.py /dev/ttyUSB0
```

The `verify.py` script will return 0 on success (all tests pass), and non-zero otherwise.

| Board               | Platform             | Module           | Last Tested Commit    |
| ---                 | ---                  | ---              | ---                   |
| ESP32-S3-DevKitC-1  | ESP-IDF (v5.1.1)     | ESP32-S3-WROOM-1 | Every commit, CI/CD   |
| ESP32-DevKitC-32E   | ESP-IDF (v5.1.1)     | ESP32-WROOM-32E  | v0.7.0 (Jun 15, 2023) |
| ESP32-C3-DevKitM-1  | ESP-IDF (v5.1.1)     | ESP32-C3-MINI-1  | v0.7.0 (Jun 15, 2023) |
| CY8CPROTO-062-4343W | ModusToolbox (3.0.0) | PSoC® 6 CYW4343W | v0.7.0 (Jun 15, 2023) |
