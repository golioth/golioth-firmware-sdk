# Golioth Basics Example

This example will connect to Golioth and demonstrate Logging, Over-the-Air (OTA)
firmware updates, and sending data to both LightDB state and LightDB Stream.
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

ESP32-DevKitC-WROVER
--------------------

Configure the following Kconfig options based on your WiFi AP credentials:

- GOLIOTH_SAMPLE_WIFI_SSID  - WiFi SSID
- GOLIOTH_SAMPLE_WIFI_PSK   - WiFi PSK

by adding these lines to configuration file (e.g. ``prj.conf`` or
``board/esp32_devkitc_wrover.conf``):

```cfg
CONFIG_GOLIOTH_SAMPLE_WIFI_SSID="my-wifi"
CONFIG_GOLIOTH_SAMPLE_WIFI_PSK="my-psk"
```

On your host computer open a terminal window, locate the source code of this
sample application (i.e., ``examples/zephyr/golioth_basics``) and type:

```console
$ west build -b esp32_devkitc_wrover examples/zephyr/golioth_basics
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
$ west build -b nrf52840dk_nrf52840 examples/zephyr/golioth_basics
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/golioth_basics`) and type:

```console
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/golioth_basics
$ west flash
```
