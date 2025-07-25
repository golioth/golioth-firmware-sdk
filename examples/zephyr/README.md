# Golioth Firmware SDK: Zephyr Support

The Golioth Firmware SDK has built-in support for Zephyr as well as
the Zephyr-based nRF Connect SDK from Nordic Semiconductor.

API documentation: <https://firmware-sdk-docs.golioth.io/>

## Requirements:

* Install the latest version of the [Zephyr
  SDK](https://github.com/zephyrproject-rtos/sdk-ng/releases/latest).
* Install [Python](https://www.python.org/downloads/)
* Install [the West
  tool](https://docs.zephyrproject.org/latest/develop/west/install.html)


Most platforms are already supported with mainline [Zephyr
RTOS](https://www.zephyrproject.org/). This repository can be added to
any Zephyr based project as new West module. However, for making things
simple, this repository can also serve as a West manifest repo.

## Using the Golioth Firmware SDK as a West manifest repository

### Using with mainline Zephyr

Execute this command to download this repository together with all
dependencies:

```console
west init -m https://github.com/golioth/golioth-firmware-sdk.git --mr v0.19.0 --mf west-zephyr.yml
west update
cd modules/lib/golioth-firmware-sdk && git submodule update --init --recursive
```

Follow [Zephyr Getting
Started](https://docs.zephyrproject.org/latest/getting_started/index.html)
for details on how to setup Zephyr based projects.

### Using with Nordic's nRF Connect SDK

Execute this command to download this repository together with all
dependencies:

```console
west init -m https://github.com/golioth/golioth-firmware-sdk.git --mr v0.19.0 --mf west-ncs.yml
west update
cd modules/lib/golioth-firmware-sdk && git submodule update --init --recursive
```

Follow [nRF Connect SDK Getting
Started](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html)
for details on how to setup nRF Connect SDK based projects.

### Adding the Golioth Firmware SDK to an existing Zephyr West project

Alternatively, add the following entry to the `west.yml` file of an
existing
[West](https://docs.zephyrproject.org/latest/west/index.html)
based project (e.g. Zephyr RTOS):

```yaml
# Golioth repository.
- name: golioth
  path: modules/lib/golioth-firmware-sdk
  revision: v0.19.0
  url: https://github.com/golioth/golioth-firmware-sdk.git
  submodules: true
```

> [!WARNING]
> To ensure that default Kconfig values are propagated correctly, place
> the golioth entry first in your west manifest.

and clone all repositories including that one by running:

```console
west update
```

> [!NOTE]
> When the Golioth Firmware SDK is added as a project in a manifest
> file, the `submodules` keyword ensures that submodules are updated
> recursively each time `west update` is used.

# Sample applications

  - [Golioth Certificate Provisioning sample](certificate_provisioning/README.md)
  - [Golioth FW Update sample](fw_update/README.md)
  - [Golioth Hello sample](hello/README.md)
  - [Golioth LightDB DELETE sample](lightdb/delete/README.md)
  - [Golioth LightDB GET sample](lightdb/get/README.md)
  - [Golioth LightDB OBSERVE sample](lightdb/observe/README.md)
  - [Golioth LightDB SET sample](lightdb/set/README.md)
  - [Golioth Location sample](location/README.md)
  - [Golioth Logging sample](logging/README.md)
  - [Golioth RPC sample](rpc/README.md)
  - [Golioth Settings sample](settings/README.md)
  - [Golioth Stream sample](stream/README.md)

# Golioth Services

  - [Golioth Cloud](https://docs.golioth.io/cloud)
  - [Authentication](https://docs.golioth.io/reference/device-api/device-auth)
  - [LightDB](https://docs.golioth.io/reference/device-api/api-docs/lightdb)
  - [Location](https://docs.golioth.io/reference/device-api/api-docs/location)
  - [Logging](https://docs.golioth.io/reference/device-api/api-docs/logging)
  - [OTA](https://docs.golioth.io/reference/device-api/api-docs/ota)
  - [RPC](https://docs.golioth.io/reference/device-api/api-docs/rpc)
  - [Stream](https://docs.golioth.io/reference/device-api/api-docs/streaming-data)
