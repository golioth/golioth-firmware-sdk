# RPC Example

## Overview

This sample application demonstrates how to use Remote Procedure calls
with Golioth to issue commands to the device and receive a status
response and optional data payload in return.

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

Next, `cd` to the hello example where you can build/flash/monitor:

```
cd examples/esp_idf/rpc
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

### Issue an RPC

This sample implements a `multiply` RPC that takes two arguments,
multiplies them together, and returns the result. Use the Golioth
Console to issue the RPC. The resulting data from the device can be
shown by clicking the three dots menu.

![Golioth Console issuing a Remote Procedure
Call](../../zephyr/rpc/img/golioth-rpc-submit.jpg)

![Golioth Console viewing result of a Remote Procedure
Call](../../zephyr/rpc/img/golioth-rpc-result.jpg)

### Sample output

```console
I (403) app_start: Starting scheduler on CPU0
I (407) main_task: Started on CPU0
I (407) main_task: Calling app_main()
I (407) rpc: Start RPC sample
I (477) pp: pp rom version: 9387209
I (487) net80211: net80211 rom version: 9387209
I (497) wifi_init: rx ba win: 6
I (497) wifi_init: tcpip mbox: 32
I (497) wifi_init: udp mbox: 6
I (507) wifi_init: tcp mbox: 6
I (507) wifi_init: tcp tx win: 5744
I (507) wifi_init: tcp rx win: 5744
I (507) wifi_init: tcp mss: 1440
I (517) wifi_init: WiFi IRAM OP enabled
I (517) wifi_init: WiFi RX IRAM OP enabled
I (537) phy_init: phy_version 970,1856f88,May 10 2023,17:44:12
I (1277) example_wifi: retry to connect to the AP
I (9637) esp_netif_handlers: sta ip: 10.0.0.202, mask: 255.255.255.0, gw: 10.0.0.1
I (9637) example_wifi: WiFi Connected. Got IP:10.0.0.202
I (9647) example_wifi: Connected to AP SSID: rpc_test
I (9647) golioth_mbox: Mbox created, bufsize: 2352, num_items: 20, item_size: 112
W (9657) rpc: Waiting for connection to Golioth...
I (9897) golioth_coap_client_libcoap: Start CoAP session with host: coaps://coap.golioth.io
I (9897) golioth_coap_client_libcoap: Session PSK-ID: 20240130145047-calico-indie-kitten@chocolate-brown-cat
I (9907) libcoap: Setting PSK key
I (9917) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (10397) golioth_coap_client_libcoap: Golioth CoAP client connected
I (10407) rpc: Golioth client connected
I (14917) rpc: 21.000000 * 2.000000 = 42.000000
```