# Golioth Firmware SDK

[![codecov](https://codecov.io/github/golioth/golioth-firmware-sdk/graph/badge.svg?token=IQSG01ZIOP)](https://codecov.io/github/golioth/golioth-firmware-sdk)

A software development kit for connecting embedded devices to the
[Golioth](https://golioth.io) IoT cloud.

This SDK can be used on any preemptive operating system that provides an
Internet stack. Golioth maintains ports for the following platforms:

* Espressif ESP-IDF
* Infineon ModusToolbox
* Zephyr & NCS
* Linux (Experimental)

More information on these ports can be found in their respective READMEs at
`examples/<platform>/README.md`

The SDK can be ported to additional platforms by following the Porting Guide in
the `docs` folder.

SDK source: https://github.com/golioth/golioth-firmware-sdk

Documentation: https://docs.golioth.io/firmware/golioth-firmware-sdk

API documentation: https://firmware-sdk-docs.golioth.io/

## Getting Started

### Getting the Code

This repo uses git submodules, so you will need to clone with the `--recursive` option:

```sh
git clone --recursive https://github.com/golioth/golioth-firmware-sdk.git -b v0.15.0
```

Or, if you've already cloned but forgot the `--recursive`, you can update and
initialize submodules with this command:

```sh
cd golioth-firmware-sdk
git submodule update --init --recursive
```


> :warning: **Note:** Zephyr-based projects should use the West tool to clone the repo. See the
Zephyr Quick Start Guide at `examples/zephyr/README.md`.

### Trying the SDK examples

The `examples` directory contains example apps which you can build and run.
It is organized by platform (e.g. ESP-IDF, ModusToolbox, etc),
so navigate to a specific platform directory and check out the README for further
instructions to build and run the examples.

The `golioth_basics` example (`examples/<platform>/golioth_basics`) is recommended
as a starting point, to learn how to connect to Golioth and use services like
[LightDB State](https://docs.golioth.io/cloud/services/lightdb),
[Stream](https://docs.golioth.io/data-routing),
[Logging](https://docs.golioth.io/cloud/services/logging),
and [OTA](https://docs.golioth.io/cloud/services/ota).

### Additional Documentation

The `docs` folder contains additional documentation, such as a platform
integration guide and a platform porting guide.

## Verified Devices

The following table lists the different hardware configurations against which
we test the SDK. Each of these devices is verified on each commit by our
Continuous Integration system.

The tests fall into three categories. Hardware-in-the-Loop (HIL) integration
tests are located in `tests/hil`. Unit tests are in `tests/unit_tests`. Finally,
each of the samples is also continously verified on target.

| Board                | Platform                 |
| ---                  | ---                      |
| ESP32-S3-DevKitC-1   | ESP-IDF (v5.3.0)         |
| ESP32-C3-DevKitM-1   | ESP-IDF (v5.3.0)         |
| ESP32-DevKitC-WROVER | ESP-IDF (v5.3.0)         |
| ESP32-DevKitC-WROVER | Zephyr (v4.0.0)          |
| nRF52840 DK + ESP32  | Zephyr (v4.0.0)          |
| MIMXRT1024-EVK       | Zephyr (v4.0.0)          |
| RAK5010              | Zephyr (v4.0.0)          |
| nRF9160 DK           | nRF Connect SDK (v2.8.0) |
| CY8CPROTO-062-4343W  | ModusToolbox (3.0.0)     |
