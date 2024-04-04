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

### Command-line

First, setup the environment. This step assumes you've installed esp-idf
to `~/esp/esp-idf`. If you haven't, follow the initial steps in
examples/esp_idf/README.md

```sh
source ~/esp/esp-idf/export.sh
```
You may have to set target based on the ESP32 chip you are using.
For example, if you are using ESP32-C3, enter this:

```
idf.py set-target esp32c3
```

Next, `cd` to the fw_update example where you can build/flash/monitor:

```
cd examples/esp_idf/fw_update
idf.py build
idf.py flash
idf.py monitor
```
## Adding credentials

After building and flashing this app, you will need to add WiFi and Golioth
credentials using the device shell.

### Shell Credentials

```console
settings set wifi/ssid "YourWiFiAccessPointName"
settings set wifi/psk "YourWiFiPassword"
settings set golioth/psk-id "YourGoliothDevicePSK-ID"
settings set golioth/psk "YourGoliothDevicePSK"
```

Type `reset` to restart the app with the new credentials.

### Prepare new firmware

Edit the app_main.c file and update the firmware version number:

```config
static const char *_current_version = "1.2.4";
```

Build the firmware update but do not flash it to the device. The binary update file
fw_update.bin needs to be uploaded to Golioth for the OTA update as an Artifact.
After uploading the artifact, you can create a new release using tht artifact.

### Start FW Update using `goliothctl`

FW update requires fw_update.bin to be uploaded as an artifact and released.

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

The device initially reports firmware version `1.2.3` and the release is rolled out,
it receives notification of a new firmware release (`1.2.4`) from Golioth which
triggers the update process.

```console
I (400) main_task: Calling app_main()
I (400) fw_update: Start FW Update sample.
I (4730) esp_netif_handlers: sta ip: 10.0.0.202, mask: 255.255.255.0, gw: 10.0.0.1
I (4730) example_wifi: WiFi Connected. Got IP:10.0.0.202
W (4750) fw_update: Waiting for connection to Golioth...
I (4780) golioth_coap_client_libcoap: Start CoAP session with host: coaps://coap.golioth.io
I (4780) golioth_coap_client_libcoap: Session PSK-ID: 2device-id@project-id
I (4790) libcoap: Setting PSK key
I (4800) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (5270) golioth_coap_client_libcoap: Golioth CoAP client connected
I (5280) fw_update: Golioth client connected
W (5280) fw_update: Current FW Version: 1.2.3
I (5290) golioth_fw_update: Current firmware version: main - 1.2.3
I (5290) main_task: Returned from app_main()
I (6610) golioth_fw_update: Waiting to receive OTA manifest
I (6710) golioth_fw_update: Received OTA manifest
I (6710) golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
I (6720) golioth_fw_update: Waiting to receive OTA manifest
I (27250) golioth_fw_update: Received OTA manifest
I (27250) golioth_fw_update: Current version = 1.2.3, Target version = 1.2.4
I (27260) golioth_fw_update: Current version = 1.2.3, Target version = 1.2.4
I (27270) golioth_fw_update: State = Downloading
I (27560) golioth_fw_update: Image size = 950464
I (27560) fw_block_processor: Downloading block index 0 (1/929)
I (28020) fw_update_esp_idf: Writing to partition subtype 17 at offset 0x1a0000
I (28020) fw_update_esp_idf: Erasing flash
I (35340) fw_block_processor: Downloading block index 1 (2/929)
I (35490) fw_block_processor: Downloading block index 2 (3/929)
I (35650) fw_block_processor: Downloading block index 3 (4/929)

...

I (206790) fw_block_processor: Downloading block index 927 (928/929)
I (206910) fw_block_processor: Downloading block index 928 (929/929)
I (207030) golioth_fw_update: Download took 179470 ms
I (207030) fw_block_processor: Block Latency Stats:
I (207030) fw_block_processor:    Min: 100 ms
I (207030) fw_block_processor:    Max: 1420 ms
I (207040) fw_block_processor: Total bytes written: 950464
I (207050) esp_image: segment 0: paddr=001a0020 vaddr=3c0b0020 size=2daa8h (187048) map
I (207080) esp_image: segment 1: paddr=001cdad0 vaddr=3fc90a00 size=02548h (  9544)
I (207080) esp_image: segment 2: paddr=001d0020 vaddr=42000020 size=a72b8h (684728) map
I (207190) esp_image: segment 3: paddr=002772e0 vaddr=3fc92f48 size=00524h (  1316)
I (207200) esp_image: segment 4: paddr=0027780c vaddr=40380000 size=10890h ( 67728)
I (207220) golioth_fw_update: State = Downloaded
I (207600) golioth_fw_update: State = Updating
I (207830) fw_update_esp_idf: Setting boot partition
I (207830) esp_image: segment 0: paddr=001a0020 vaddr=3c0b0020 size=2daa8h (187048) map
I (207860) esp_image: segment 1: paddr=001cdad0 vaddr=3fc90a00 size=02548h (  9544)
I (207860) esp_image: segment 2: paddr=001d0020 vaddr=42000020 size=a72b8h (684728) map
I (207960) esp_image: segment 3: paddr=002772e0 vaddr=3fc92f48 size=00524h (  1316)
I (207960) esp_image: segment 4: paddr=0027780c vaddr=40380000 size=10890h ( 67728)
I (208030) golioth_fw_update: Rebooting into new image in 5 seconds
I (209030) golioth_fw_update: Rebooting into new image in 4 seconds
I (210030) golioth_fw_update: Rebooting into new image in 3 seconds
I (211030) golioth_fw_update: Rebooting into new image in 2 seconds
I (212030) golioth_fw_update: Rebooting into new image in 1 seconds
I (436) main_task: Started on CPU0
I (436) main_task: Calling app_main()
I (436) fw_update: Start FW Update sample.
esp32> I (2246) esp_netif_handlers: sta ip: 10.0.0.202, mask: 255.255.255.0, gw: 10.0.0.1
I (2246) example_wifi: WiFi Connected. Got IP:10.0.0.202
W (2266) fw_update: Waiting for connection to Golioth...
I (2416) golioth_coap_client_libcoap: Start CoAP session with host: coaps://coap.golioth.io
I (2416) golioth_coap_client_libcoap: Session PSK-ID: 2device-id@project-id
I (2426) libcoap: Setting PSK key
I (2436) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (2926) golioth_coap_client_libcoap: Golioth CoAP client connected
I (2926) fw_update: Golioth client connected
W (2926) fw_update: Current FW Version: 1.2.4
I (2936) golioth_fw_update: Current firmware version: main - 1.2.4
I (2946) golioth_fw_update: Waiting for golioth client to connect before cancelling rollback
I (2956) golioth_fw_update: Firmware updated successfully!
I (2946) main_task: Returned from app_main()
I (3006) golioth_fw_update: State = Idle
I (4246) golioth_fw_update: Waiting to receive OTA manifest
I (4366) golioth_fw_update: Received OTA manifest
I (4366) golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
I (4376) golioth_fw_update: Waiting to receive OTA manifest

```

After a successful update the new version number will be printed out on
the serial terminal and displayed on the Golioth web console.
