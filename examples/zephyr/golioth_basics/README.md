# Golioth Basics Example

This example will connect to Golioth and demonstrate Logging, Over-the-Air (OTA)
firmware updates, and sending data to both LightDB state and Stream.
Please see the comments in main.c for a thorough explanation of the expected
behavior.

## Requirements

* Golioth credentials
* Network connectivity

## Building and Running

### Authentication specific configuration

#### Configure PSK and PSK-ID credentials

Add credentials to
``~/golioth-workspace/modules/lib/golioth-firmware-sdk/examples/zephyr/golioth_basics/prj.conf``
file:

``` cfg
CONFIG_GOLIOTH_SAMPLE_PSK_ID="my-psk-id@my-project"
CONFIG_GOLIOTH_SAMPLE_PSK="my-psk"
```

### Platform specific configuration

#### QEMU

This application has been built and tested with QEMU x86 (qemu_x86).

On your Linux host computer, open a terminal window, locate the source
code of this sample application (i.e., `examples/zephyr/golioth_basics`) and
type:

```console
$ west build -b qemu_x86 examples/zephyr/golioth_basics
$ west build -t run
```

See [Networking with
QEMU](https://docs.zephyrproject.org/3.3.0/connectivity/networking/qemu_setup.html)
on how to setup networking on host and configure NAT/masquerading to
access Internet.

#### ESP32-DevKitC-WROVER

Configure the following Kconfig options based on your WiFi AP
credentials:

- GOLIOTH_SAMPLE_WIFI_SSID  - WiFi SSID
- GOLIOTH_SAMPLE_WIFI_PSK   - WiFi PSK

by adding these lines to configuration file (e.g. `prj.conf` or
`board/esp32_devkitc_wrover.conf`):

```cfg
CONFIG_GOLIOTH_SAMPLE_WIFI_SSID="my-wifi"
CONFIG_GOLIOTH_SAMPLE_WIFI_PSK="my-psk"
```

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/golioth_basics`) and
type:

```console
$ west build -b esp32_devkitc_wrover --sysbuild examples/zephyr/golioth_basics
$ west flash
```

#### nRF52840 DK + ESP32-WROOM-32

This subsection documents using nRF52840 DK running Zephyr with
offloaded ESP-AT WiFi driver and ESP32-WROOM-32 module based board (such
as ESP32 DevkitC rev. 4) running WiFi stack. See [AT Binary
Lists](https://docs.espressif.com/projects/esp-at/en/latest/AT_Binary_Lists/index.html)
for links to ESP-AT binaries and details on how to flash ESP-AT image on
ESP chip. Flash ESP chip with following command:

```console
esptool.py write_flash --verify 0x0 PATH_TO_ESP_AT/factory/factory_WROOM-32.bin
```

Connect nRF52840 DK and ESP32-DevKitC V4 (or other ESP32-WROOM-32 based
board) using wires:

| nRF52840 DK | ESP32-WROOM-32  | ESP32-WROVER-32 |
| ----------- | --------------- | ----------------|
| P1.01 (RX)  | IO17 (TX)       | IO22 (TX)       |
| P1.02 (TX)  | IO16 (RX)       | IO19 (RX)       |
| P1.03 (CTS) | IO14 (RTS)      | IO14 (RTS)      |
| P1.04 (RTS) | IO15 (CTS)      | IO15 (CTS)      |
| P1.05       | EN              | EN              |
| GND         | GND             | GND             |

Configure the following Kconfig options based on your WiFi AP
credentials:

* GOLIOTH_SAMPLE_WIFI_SSID - WiFi SSID
* GOLIOTH_SAMPLE_WIFI_PSK - WiFi PSK

by adding these lines to configuration file (e.g. `prj.conf` or
`board/nrf52840dk_nrf52840.conf`):

```cfg
CONFIG_GOLIOTH_SAMPLE_WIFI_SSID="my-wifi"
CONFIG_GOLIOTH_SAMPLE_WIFI_PSK="my-psk"
```

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/golioth_basics`) and type:

```console
$ west build -b nrf52840dk_nrf52840 --sysbuild examples/zephyr/golioth_basics
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/golioth_basics`) and type:

```console
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/golioth_basics
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010_nrf52840 --sysbuild examples/zephyr/golioth_basics
$ west flash
```

## Device Firmware Upgrade via OTA

Golioth Basics includes support for Golioth Over-the-Air (OTA) Updates.
To test this feature, increment the app version, rebuild the firmware,
and upload the binary to Golioth as a new artifact.

### Prepare new firmware

Edit the `examples/common/golioth_basics.c` file and update the firmware
version number:

```config
// Current firmware version
static const char* _current_version = "1.2.6";
```

Build the firmware update but do not flash it to the device. The binary
update file will be uploaded to Golioth for the OTA update.

```console
# For esp32_devkitc_wrover
$ west build -b esp32_devkitc_wrover --sysbuild examples/zephyr/golioth_basics

# For nRF52840dk (Zephyr):
$ west build -b nrf52840dk_nrf52840 --sysbuild examples/zephyr/golioth_basics

# For nRF9160dk (NCS):
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/golioth_basics
```

### Start DFU using `goliothctl`

DFU requires one of two files based on the which platform you are using:

* Zephyr: `build/golioth_basics/zephyr/zephyr.signed.bin`
* NCS (Nordic version of Zephyr): `build/zephyr/app_update.bin`

Use the correct file from your build to replace `<binary_file>` in the
commands below.

1. Run the following command on a host PC to upload the new firmware as
   an artifact to Golioth:

    ```console
    $ goliothctl dfu artifact create <binary_file> --version 1.2.6
    ```

2. Create a new release consisting of this single firmware and roll it
out to all devices in the project:

    ```console
    $ goliothctl dfu release create --release-tags 1.2.6 --components main@1.2.6 --rollout true
    ```

Note: the artifact upload and release rollout process is also available
using the [Golioth web console](https://console.golioth.io).
