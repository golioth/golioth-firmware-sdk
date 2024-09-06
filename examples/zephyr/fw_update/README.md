# Golioth FW Update sample

## Overview

This sample application demonstrates how to perform a Firmware
Update using Golioth's Over-the-Air (OTA) update service. This is
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

#### PSK based auth - Hardcoded

Configure the following Kconfig options based on your Golioth
credentials:

* GOLIOTH_SAMPLE_PSK_ID - PSK ID of registered device
* GOLIOTH_SAMPLE_PSK - PSK of registered device

by adding these lines to configuration file (e.g. `prj.conf`):

```cfg
CONFIG_GOLIOTH_SAMPLE_PSK_ID="my-psk-id"
CONFIG_GOLIOTH_SAMPLE_PSK="my-psk"
```

#### PSK based auth - Runtime

We provide an option for setting Golioth credentials through the Zephyr
shell. This is based on the Zephyr Settings subsystem.

Enable the settings shell by including the following configuration overlay
file:

```sh
$ west build -- -DEXTRA_CONF_FILE=../common/runtime_settings.conf
```

Alternatively, you can add the following options to ``prj.conf``:

```cfg
CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS=n

CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_NVS=y

CONFIG_SETTINGS=y
CONFIG_SETTINGS_RUNTIME=y
CONFIG_GOLIOTH_SAMPLE_SETTINGS=y
CONFIG_GOLIOTH_SAMPLE_WIFI_SETTINGS=y
CONFIG_GOLIOTH_SAMPLE_SETTINGS_AUTOLOAD=y
CONFIG_GOLIOTH_SAMPLE_SETTINGS_SHELL=y
```

At runtime, configure PSK-ID and PSK using the device shell based on your
Golioth credentials:

```sh
uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
uart:~$ settings set golioth/psk <my-psk>
uart:-$ kernel reboot cold
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
this sample application (i.e., `examples/zephyr/fw_update`) and type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu --sysbuild examples/zephyr/fw_update
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
this sample application (i.e., `examples/zephyr/fw_update`) and type:

```console
$ west build -b nrf52840dk/nrf52840 --sysbuild examples/zephyr/fw_update
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/hello`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/fw_update
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 --sysbuild examples/zephyr/fw_update
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

Edit the `VERSION` file and update the firmware version number:

```config
VERSION_MAJOR = 1
VERSION_MINOR = 2
PATCHLEVEL = 4
```

Build the firmware update but do not flash it to the device. The binary
update file will be uploaded to Golioth for the OTA update.

```console
# For esp32_devkitc_wrover/esp32/procpu
$ west build -b esp32_devkitc_wrover/esp32/procpu --sysbuild examples/zephyr/fw_update

# For nRF52840dk (Zephyr):
$ west build -b nrf52840dk/nrf52840 --sysbuild examples/zephyr/fw_update

# For nRF9160dk (NCS):
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/fw_update
```

### Start FW Update using `goliothctl`

FW update requires one of two files based on the which platform you are using:

* Zephyr: `build/fw_update/zephyr/zephyr.signed.bin`
* NCS (Nordic version of Zephyr): `build/zephyr/app_update.bin`

Use the correct file from your build to replace `<binary_file>` in the
commands below.

1. Run the following command on a host PC to upload the new firmware as
   an artifact to Golioth:

    ```console
    $ goliothctl dfu artifact create <binary_file> --version 1.2.4
    ```

2. Create a new release consisting of this single firmware and roll it
out to all devices in the project:

    ```console
    $ goliothctl dfu release create --release-tags 1.2.4 --components main@1.2.4 --rollout true
    ```

Note: the artifact upload and release rollout process is also available
using the [Golioth web console](https://console.golioth.io).

### Sample output

A sample output from the serial console is found below.

The device initially reports firmware version `1.2.3` and about 90
seconds later it receives notification of a new firmware release
(`1.2.4`) from Golioth which triggers the update process.

```console
[00:00:00.102,783] <inf> wifi_esp_at: Waiting for interface to come up
[00:00:02.326,904] <inf> wifi_esp_at: AT version: 2.4.0.0(s-4c6eb5e - ESP32 - May 20 2022 03:12:58)
[00:00:02.330,078] <inf> wifi_esp_at: SDK version: qa-test-v4.3.3-20220423
[00:00:02.336,486] <inf> wifi_esp_at: Bin version: 2.4.0(WROVER-32)
[00:00:02.771,179] <inf> wifi_esp_at: ESP Wi-Fi ready
*** Booting Zephyr OS build v3.6.0 ***
[00:00:02.779,083] <inf> fs_nvs: 8 Sectors of 4096 bytes
[00:00:02.779,113] <inf> fs_nvs: alloc wra: 0, f78
[00:00:02.779,113] <inf> fs_nvs: data wra: 0, e8
[00:00:02.779,937] <dbg> fw_update_sample: main: Start FW Update sample
[00:00:02.779,968] <inf> golioth_samples: Bringing up network interface
[00:00:02.779,968] <inf> golioth_samples: Waiting to obtain IP address
[00:00:02.780,059] <inf> golioth_wifi: Connecting to 'golioth'
[00:00:05.923,553] <inf> golioth_mbox: Mbox created, bufsize: 1232, num_items: 10, item_size: 112
[00:00:05.924,255] <inf> golioth_fw_update: Current firmware version: main - 1.2.3
[00:00:06.359,771] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:00:06.359,985] <inf> fw_update_sample: Golioth client connected
[00:00:06.360,015] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:00:06.479,553] <inf> golioth_fw_update: Waiting to receive OTA manifest
[00:00:06.860,839] <inf> golioth_fw_update: Received OTA manifest
[00:00:06.860,839] <inf> golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
[00:00:06.860,839] <inf> golioth_fw_update: Waiting to receive OTA manifest
[00:00:45.483,612] <inf> golioth_fw_update: Received OTA manifest
[00:00:45.483,673] <inf> golioth_fw_update: Current version = 1.2.3, Target version = 1.2.4
[00:00:45.483,673] <inf> golioth_fw_update: State = Downloading
[00:00:46.278,076] <inf> golioth_fw_update: Received block 0/290
[00:00:46.278,137] <inf> mcuboot_util: Image index: 0, Swap type: none
[00:00:46.278,167] <inf> golioth_fw_zephyr: swap type: none
[00:00:46.552,581] <inf> golioth_fw_update: Received block 1/290
[00:00:46.821,960] <inf> golioth_fw_update: Received block 2/290
[00:00:47.102,203] <inf> golioth_fw_update: Received block 3/290

...

[00:01:55.889,648] <inf> golioth_fw_update: Received block 288/290
[00:01:56.168,334] <inf> golioth_fw_update: Received block 289/290
[00:01:57.032,562] <inf> golioth_fw_update: Received block 290/290
[00:01:57.126,739] <inf> golioth_fw_update: Successfully downloaded 297884 bytes in 71455 ms
[00:01:57.126,770] <inf> golioth_fw_update: State = Downloaded
[00:01:57.283,966] <inf> golioth_fw_update: State = Updating
[00:01:57.760,375] <inf> golioth_fw_update: Rebooting into new image in 5 seconds
[00:01:58.760,498] <inf> golioth_fw_update: Rebooting into new image in 4 seconds
[00:01:59.760,681] <inf> golioth_fw_update: Rebooting into new image in 3 seconds
[00:02:00.760,833] <inf> golioth_fw_update: Rebooting into new image in 2 seconds
[00:02:01.761,016] <inf> golioth_fw_update: Rebooting into new image in 1 seconds
uart:~$ *** Booting Zephyr OS build v3.6.0 ***
I: Starting bootloader
I: Primary image: magic=good, swap_type=0x2, copy_done=0x1, image_ok=0x1
I: Secondary image: magic=good, swap_type=0x2, copy_done=0x3, image_ok=0x3
I: Boot source: none
I: Image index: 0, Swap type: test
I: Starting swap using move algorithm.
I: Bootloader chainload address offset: 0xc000
I: Jumping to the first image slot


[00:00:00.102,783] <inf> wifi_esp_at: Waiting for interface to come up
[00:00:02.232,513] <inf> wifi_esp_at: AT version: 2.4.0.0(s-4c6eb5e - ESP32 - May 20 2022 03:12:58)
[00:00:02.235,687] <inf> wifi_esp_at: SDK version: qa-test-v4.3.3-20220423
[00:00:02.242,095] <inf> wifi_esp_at: Bin version: 2.4.0(WROVER-32)
[00:00:02.677,001] <inf> wifi_esp_at: ESP Wi-Fi ready
*** Booting Zephyr OS build v3.6.0 ***
[00:00:02.684,936] <inf> fs_nvs: 8 Sectors of 4096 bytes
[00:00:02.684,936] <inf> fs_nvs: alloc wra: 0, f78
[00:00:02.684,936] <inf> fs_nvs: data wra: 0, e8
[00:00:02.685,791] <dbg> fw_update_sample: main: Start FW Update sample
[00:00:02.685,791] <inf> golioth_samples: Bringing up network interface
[00:00:02.685,821] <inf> golioth_samples: Waiting to obtain IP address
[00:00:02.685,882] <inf> golioth_wifi: Connecting to 'golioth'
[00:00:05.922,149] <inf> golioth_mbox: Mbox created, bufsize: 1232, num_items: 10, item_size: 112
[00:00:05.922,851] <inf> golioth_fw_update: Current firmware version: main - 1.2.4
[00:00:05.923,492] <inf> golioth_fw_update: Waiting for golioth client to connect before cancelling rollback
[00:00:06.617,523] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:00:06.617,736] <inf> fw_update_sample: Golioth client connected
[00:00:06.617,858] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:00:06.923,614] <inf> golioth_fw_update: Firmware updated successfully!
[00:00:06.923,706] <inf> golioth_fw_update: State = Idle
[00:00:07.285,919] <inf> golioth_fw_update: Waiting to receive OTA manifest
[00:00:07.376,953] <inf> golioth_fw_update: Received OTA manifest
[00:00:07.376,953] <inf> golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
[00:00:07.376,953] <inf> golioth_fw_update: Waiting to receive OTA manifest
```

After a successful update the new version number will be printed out on
the serial terminal and displayed on the Golioth web console.
