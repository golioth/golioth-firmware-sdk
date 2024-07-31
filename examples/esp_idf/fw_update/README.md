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
I (590) main_task: Started on CPU0
I (600) main_task: Calling app_main()
I (600) fw_update: Start FW Update sample.

Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
I (680) wifi_init: rx ba win: 6
I (680) wifi_init: tcpip mbox: 32
I (680) wifi_init: udp mbox: 6
I (680) wifi_init: tcp mbox: 6
                                                                                                                                                                                              I (690) wifi_init: tcp tx win: 5760
I (700) wifi_init: tcp rx win: 5760
I (700) wifi_init: tcp mss: 1440
I (700) wifi_init: WiFi IRAM OP enabled
I (700) wifi_init: WiFi RX IRAM OP enabled
esp32> I (720) phy_init: phy_version 4791,2c4672b,Dec 20 2023,16:06:06
W (1180) wifi:[ADDBA]rx delba, code:39, delete tid:0
W (1180) wifi:[ADDBA]rx delba, code:39, delete tid:0
I (3200) esp_netif_handlers: sta ip: 192.168.200.7, mask: 255.255.255.0, gw: 192.168.200.1
I (3200) example_wifi: WiFi Connected. Got IP:192.168.200.7
I (3210) example_wifi: Connected to AP SSID: golioth
I (3210) golioth_mbox: Mbox created, bufsize: 2520, num_items: 20, item_size: 120
W (3220) fw_update: Waiting for connection to Golioth...
I (3250) golioth_coap_client_libcoap: Start CoAP session with host: coaps://coap.golioth.io
I (3250) golioth_coap_client_libcoap: Session PSK-ID: devboard-one-id@ttgo-demo
I (3270) libcoap: Setting PSK key

I (3270) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (3720) golioth_coap_client_libcoap: Golioth CoAP client connected
I (3730) fw_update: Golioth client connected
W (3730) fw_update: Current FW Version: 1.2.3
I (3730) golioth_fw_update: Current firmware version: main - 1.2.3
I (3750) main_task: Returned from app_main()
I (4990) golioth_fw_update: Waiting to receive OTA manifest
I (5310) golioth_fw_update: Received OTA manifest
I (5320) golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
I (5320) golioth_fw_update: Waiting to receive OTA manifest
I (31160) golioth_fw_update: Received OTA manifest
I (31160) golioth_fw_update: Current version = 1.2.3, Target version = 1.2.4
I (31160) golioth_fw_update: State = Downloading
I (31940) golioth_fw_update: Received block 0/921
I (31950) fw_update_esp_idf: Writing to partition subtype 17 at offset 0x1a0000
I (31950) fw_update_esp_idf: Erasing flash
W (34400) golioth_coap_client_libcoap: CoAP message retransmitted
I (36970) golioth_fw_update: Received block 1/921
I (37480) golioth_fw_update: Received block 2/921
I (38030) golioth_fw_update: Received block 3/921

...

I (243900) golioth_fw_update: Received block 919/921
I (244010) golioth_fw_update: Received block 920/921
I (244110) golioth_fw_update: Received block 921/921
I (244120) golioth_fw_update: Successfully downloaded 943248 bytes in 212390 ms
I (244120) esp_image: segment 0: paddr=001a0020 vaddr=3f400020 size=2efach (192428) map
I (244200) esp_image: segment 1: paddr=001cefd4 vaddr=3ffb0000 size=01044h (  4164)
I (244210) esp_image: segment 2: paddr=001d0020 vaddr=400d0020 size=9d4d4h (644308) map
I (244430) esp_image: segment 3: paddr=0026d4fc vaddr=3ffb1044 size=02dc0h ( 11712)
I (244440) esp_image: segment 4: paddr=002702c4 vaddr=40080000 size=161a0h ( 90528)
I (244480) golioth_fw_update: State = Downloaded
I (244680) golioth_fw_update: State = Updating
I (244780) fw_update_esp_idf: Setting boot partition
I (244780) esp_image: segment 0: paddr=001a0020 vaddr=3f400020 size=2efach (192428) map
I (244850) esp_image: segment 1: paddr=001cefd4 vaddr=3ffb0000 size=01044h (  4164)
I (244860) esp_image: segment 2: paddr=001d0020 vaddr=400d0020 size=9d4d4h (644308) map
I (245080) esp_image: segment 3: paddr=0026d4fc vaddr=3ffb1044 size=02dc0h ( 11712)
I (245090) esp_image: segment 4: paddr=002702c4 vaddr=40080000 size=161a0h ( 90528)
I (245160) golioth_fw_update: Rebooting into new image in 5 seconds
I (246170) golioth_fw_update: Rebooting into new image in 4 seconds
I (247170) golioth_fw_update: Rebooting into new image in 3 seconds
I (248170) golioth_fw_update: Rebooting into new image in 2 seconds
I (249170) golioth_fw_update: Rebooting into new image in 1 seconds

...

I (622) main_task: Started on CPU0
I (632) main_task: Calling app_main()
I (632) fw_update: Start FW Update sample.

Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
I (712) wifi_init: rx ba win: 6
I (712) wifi_init: tcpip mbox: 32
I (712) wifi_init: udp mbox: 6
I (712) wifi_init: tcp mbox: 6
I (722) wifi_init: tcp tx win: 5760
I (732) wifi_init: tcp rx win: 5760
I (732) wifi_init: tcp mss: 1440
I (732) wifi_init: WiFi IRAM OP enabled
I (732) wifi_init: WiFi RX IRAM OP enabled
esp32> I (752) phy_init: phy_version 4791,2c4672b,Dec 20 2023,16:06:06
I (5262) esp_netif_handlers: sta ip: 192.168.200.7, mask: 255.255.255.0, gw: 192.168.200.1
I (5262) example_wifi: WiFi Connected. Got IP:192.168.200.7
I (5272) example_wifi: Connected to AP SSID: golioth
I (5282) golioth_mbox: Mbox created, bufsize: 2520, num_items: 20, item_size: 120
W (5282) fw_update: Waiting for connection to Golioth...
I (5312) golioth_coap_client_libcoap: Start CoAP session with host: coaps://coap.golioth.io
I (5322) golioth_coap_client_libcoap: Session PSK-ID: devboard-one-id@ttgo-demo
I (5332) libcoap: Setting PSK key

I (5342) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (5632) golioth_coap_client_libcoap: Golioth CoAP client connected
I (5642) fw_update: Golioth client connected
W (5642) fw_update: Current FW Version: 1.2.4
I (5652) golioth_fw_update: Current firmware version: main - 1.2.4
I (5662) golioth_fw_update: Waiting for golioth client to connect before cancelling rollback
I (5662) golioth_fw_update: Firmware updated successfully!
I (5662) main_task: Returned from app_main()
I (5732) golioth_fw_update: State = Idle
I (6922) golioth_fw_update: Waiting to receive OTA manifest
I (6972) golioth_fw_update: Received OTA manifest
I (6982) golioth_fw_update: Manifest does not contain different firmware version. Nothing to do.
I (6992) golioth_fw_update: Waiting to receive OTA manifest
```

After a successful update the new version number will be printed out on
the serial terminal and displayed on the Golioth web console.
