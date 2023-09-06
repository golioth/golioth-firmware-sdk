# Golioth DFU sample

## Overview

This sample application demonstrates how to perform Device Firmware
Upgrade (DFU) using Golioth's Over-the-Air (OTA) update service. This is
a two step process:

* Build initial firmware and flash it to the device.
* Build new firmware to use as the upgrade and upload it to Golioth.

Your device automatically detects available releases from Golioth and
performs an OTA update when a newer version is available.

## Requirements

* Golioth credentials
* Network connectivity

## Building and Running

### Authentication specific configuration

#### PSK based auth

Configure the following Kconfig options based on your Golioth
credentials:

* GOLIOTH_SAMPLE_PSK_ID - PSK ID of registered device
* GOLIOTH_SAMPLE_PSK - PSK of registered device

by adding these lines to configuration file (e.g. `prj.conf`):

```cfg
CONFIG_GOLIOTH_SAMPLE_PSK_ID="my-psk-id"
CONFIG_GOLIOTH_SAMPLE_PSK="my-psk"
```

#### Certificate based auth

Configure the following Kconfig options based on your Golioth
credentials:

* CONFIG_GOLIOTH_AUTH_METHOD_CERT - use certificate-based
    authentication
* CONFIG_GOLIOTH_SAMPLE_HARDCODED_CRT_PATH - device certificate
* CONFIG_GOLIOTH_SAMPLE_HARDCODED_KEY_PATH - device private key

by adding these lines to configuration file (e.g. `prj.conf`):

```cfg
CONFIG_GOLIOTH_AUTH_METHOD_CERT=y
CONFIG_GOLIOTH_SAMPLE_HARDCODED_CRT_PATH="keys/device.crt.der"
CONFIG_GOLIOTH_SAMPLE_HARDCODED_KEY_PATH="keys/device.key.der"
```

### Platform specific configuration

ESP32-DevKitC-WROVER
--------------------

Configure the following Kconfig options based on your WiFi AP credentials:

- GOLIOTH_SAMPLE_WIFI_SSID  - WiFi SSID
- GOLIOTH_SAMPLE_WIFI_PSK   - WiFi PSK

by adding these lines to configuration file (e.g. ``prj.conf`` or
``board/esp32_devkitc_wrover.conf``):

.. code-block:: cfg

   CONFIG_GOLIOTH_SAMPLE_WIFI_SSID="my-wifi"
   CONFIG_GOLIOTH_SAMPLE_WIFI_PSK="my-psk"

On your host computer open a terminal window, locate the source code of this
sample application (i.e., ``examples/zephyr/dfu``) and type:

.. code-block:: console

   $ west build -b esp32_devkitc_wrover examples/zephyr/dfu
   $ west flash

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

Connect nRF52840 DK and ESP32-DevKitC V4 (or other
ESP32-WROOM-32/ESP32-ROVER-32 based board) using wires:

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
this sample application (i.e., `examples/zephyr/dfu`) and type:

```console
$ west build -b nrf52840dk_nrf52840 --sysbuild examples/zephyr/dfu
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/hello`) and type:

```console
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/dfu
$ west flash
```
### Verify flashed application

```console
uart:~$ mcuboot
swap type: none
confirmed: 1

primary area (1):
  version: 1.2.3+0
  image size: 348864
  magic: unset
  swap type: none
  copy done: unset
  image ok: unset

failed to read secondary area (2) header: -5
```

### Prepare new firmware

Edit the `prj.conf` file and update the firmware version number:

```config
# Firmware version used in DFU process
CONFIG_GOLIOTH_SAMPLE_FW_VERSION="1.2.4"
```

Build the firmware update but do not flash it to the device. The binary
update file will be uploaded to Golioth for the OTA update.

```console
# For nRF52840dk (Zephyr):
$ west build -b nrf52840dk_nrf52840 --sysbuild examples/zephyr/dfu

# For nRF9160dk (NCS):
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/dfu
```

### Start DFU using `goliothctl`

DFU requires one of two files based on the which platform you are using:

* Zephyr: `build/dfu/zephyr/zephyr.signed.bin`
* NCS (Nordic version of Zephyr): `build/zephyr/app_update.bin`

Use the correct file from your build to replace `<binary_file>` in the
commands below.

1. Run the following command on a host PC to upload the new firmware as
   an artifact to Golioth:

    ```console
    $ goliothctl dfu artifact create <binary_file> --version 1.2.3
    ```

2. Create a new release consisting of this single firmware and roll it
out to all devices in the project:

    ```console
    $ goliothctl dfu release create --release-tags 1.2.3 --components main@1.2.3 --rollout true
    ```

Note: the artifact upload and release rollout process is also available
using the [Golioth web console](https://console.golioth.io).

### Sample output

A sample output from the serial console is found below.

The device initially reports firmware version `1.2.3` and about 90
seconds later it receives notification of a new firmware release
(`1.2.4`) from Golioth which triggers the update process.

```console
*** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.101,074] <dbg> dfu_sample: main: Start DFU sample
[00:00:00.101,104] <inf> golioth_samples: Waiting for interface to be up
[00:00:02.248,626] <inf> wifi_esp_at: AT version: 2.4.0.0(s-4c6eb5e - ESP32 - May 20 2022 03:12:58)
[00:00:02.251,800] <inf> wifi_esp_at: SDK version: qa-test-v4.3.3-20220423
[00:00:02.258,209] <inf> wifi_esp_at: Bin version: 2.4.0(WROVER-32)
[00:00:02.692,840] <inf> wifi_esp_at: ESP Wi-Fi ready
[00:00:02.692,932] <inf> golioth_samples: Connecting to WiFi
[00:00:02.787,139] <inf> golioth_wifi: Connected with status: 0
Connected
[00:00:02.787,200] <inf> golioth_wifi: Successfully connected to WiFi
[00:00:02.787,292] <inf> golioth_mbox: Mbox created, bufsize: 1144, num_items: 10, item_size: 104
[00:00:02.787,445] <inf> golioth_fw_update: Current firmware version: main - 1.2.3
[00:00:03.721,893] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:03.721,954] <inf> golioth_coap_client: Session PSK-ID: devboard-one-id@ttgo-demo
[00:00:03.722,320] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:05.139,770] <inf> golioth_fw_update: Waiting to receive OTA manifest
[00:00:05.139,831] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:05.140,045] <inf> dfu_sample: Golioth client connected
[00:00:05.362,274] <inf> golioth_fw_update: Received OTA manifest
[00:00:05.362,304] <inf> golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
[00:00:05.362,304] <inf> golioth_fw_update: Waiting to receive OTA manifest
[00:01:28.757,141] <inf> golioth_fw_update: Received OTA manifest
[00:01:28.757,232] <inf> golioth_fw_update: Current version = 1.2.3, Target version = 1.2.4
[00:01:28.757,232] <inf> golioth_fw_update: State = Downloading
[00:01:29.021,575] <inf> golioth_fw_update: Image size = 349712
[00:01:29.021,606] <inf> fw_block_processor: Downloading block index 0 (1/342)
[00:01:29.324,279] <inf> mcuboot_util: Swap type: none
[00:01:29.324,310] <inf> golioth_fw_zephyr: swap type: none
[00:01:39.122,497] <inf> fw_block_processor: Downloading block index 1 (2/342)
[00:01:39.383,209] <inf> fw_block_processor: Downloading block index 2 (3/342)
[00:01:39.647,644] <inf> fw_block_processor: Downloading block index 3 (4/342)

...

[00:03:33.643,737] <inf> fw_block_processor: Downloading block index 339 (340/342)
[00:03:33.938,903] <inf> fw_block_processor: Downloading block index 340 (341/342)
[00:03:34.241,119] <inf> fw_block_processor: Downloading block index 341 (342/342)
[00:03:34.505,767] <inf> golioth_fw_update: Download took 125484 ms
[00:03:34.505,798] <inf> fw_block_processor: Block Latency Stats:
[00:03:34.505,798] <inf> fw_block_processor:    Min: 240 ms
[00:03:34.505,828] <inf> fw_block_processor:    Ave: %.3f ms
[00:03:34.505,828] <inf> fw_block_processor:    Max: 3387 ms
[00:03:34.505,828] <inf> fw_block_processor: Total bytes written: 349712
[00:03:34.506,042] <inf> golioth_fw_update: State = Downloaded
[00:03:34.771,270] <inf> golioth_fw_update: State = Updating
[00:03:35.183,532] <inf> golioth_fw_update: Rebooting into new image in 5 seconds
[00:03:36.183,593] <inf> golioth_fw_update: Rebooting into new image in 4 seconds
[00:03:37.183,685] <inf> golioth_fw_update: Rebooting into new image in 3 seconds
[00:03:38.183,776] <inf> golioth_fw_update: Rebooting into new image in 2 seconds
[00:03:39.183,959] <inf> golioth_fw_update: Rebooting into new image in 1 seconds
uart:~$ *** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
I: Starting bootloader
I: Primary image: magic=good, swap_type=0x2, copy_done=0x1, image_ok=0x1
I: Secondary image: magic=good, swap_type=0x2, copy_done=0x3, image_ok=0x3
I: Boot source: none
I: Swap type: test
I: Starting swap using move algorithm.
I: Bootloader chainload address offset: 0xc000
I: Jumping to the first image slot


*** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.101,104] <dbg> dfu_sample: main: Start DFU sample
[00:00:00.101,135] <inf> golioth_samples: Waiting for interface to be up
[00:00:02.246,490] <inf> wifi_esp_at: AT version: 2.4.0.0(s-4c6eb5e - ESP32 - May 20 2022 03:12:58)
[00:00:02.249,633] <inf> wifi_esp_at: SDK version: qa-test-v4.3.3-20220423
[00:00:02.256,072] <inf> wifi_esp_at: Bin version: 2.4.0(WROVER-32)
[00:00:02.690,704] <inf> wifi_esp_at: ESP Wi-Fi ready
[00:00:02.690,795] <inf> golioth_samples: Connecting to WiFi
[00:00:02.778,930] <inf> golioth_wifi: Connected with status: 0
Connected
[00:00:02.778,991] <inf> golioth_wifi: Successfully connected to WiFi
[00:00:02.779,083] <inf> golioth_mbox: Mbox created, bufsize: 1144, num_items: 10, item_size: 104
[00:00:02.779,235] <inf> golioth_fw_update: Current firmware version: main - 1.2.4
[00:00:02.779,357] <inf> golioth_fw_update: Waiting for golioth client to connect before cancelling rollback
[00:00:03.843,505] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:03.843,536] <inf> golioth_coap_client: Session PSK-ID: devboard-one-id@ttgo-demo
[00:00:03.843,933] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:05.226,623] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:05.226,867] <inf> dfu_sample: Golioth client connected
[00:00:05.779,571] <inf> golioth_fw_update: Firmware updated successfully!
[00:00:05.779,663] <inf> golioth_fw_update: State = Idle
[00:00:05.939,239] <inf> golioth_fw_update: Waiting to receive OTA manifest
[00:00:06.041,503] <inf> golioth_fw_update: Received OTA manifest
[00:00:06.041,503] <inf> golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
[00:00:06.041,534] <inf> golioth_fw_update: Waiting to receive OTA manifest
```

After a successful update the new version number will be printed out on
the serial terminal and displayed on the Golioth web console.
