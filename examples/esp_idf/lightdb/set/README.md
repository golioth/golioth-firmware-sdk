# Golioth LightDB Set Sample

## Overview

This sample demonstrates how to connect to Golioth and set values inside
of LightDB State.

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

Next, `cd` to the lightdb/set example where you can build/flash/monitor:
```
cd examples/esp_idf/lightdb/set
```

Make sure that CONFIG_GOLIOTH_LIGHTDB_STATE is set by going into the interactive menuconfig
```
idf.py menuconfig
```
Navigate to Component config -> Golioth SDK Configuration -> Golioth LightDB State service

If it is not set, press Enter, then save and exit

Now you can build the code, flash and monitor

```
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

### Sample output

This is the output from the serial console:

```console
I (398) main_task: Started on CPU0
I (398) main_task: Calling app_main()
I (398) lightdb: Start LightDB set sample
I (4638) esp_netif_handlers: sta ip: 10.0.0.202, mask: 255.255.255.0, gw: 10.0.0.1
I (4638) example_wifi: WiFi Connected. Got IP:10.0.0.202
W (4658) lightdb: Waiting for connection to Golioth...
I (5988) golioth_coap_client_libcoap: Session PSK-ID: device-id@project-id
I (5998) libcoap: Setting PSK key
I (6008) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (6418) golioth_coap_client_libcoap: Golioth CoAP client connected
I (6418) lightdb: Golioth client connected
I (5289) lightdb: Setting counter to 0
I (5289) lightdb: Before request (async)
I (5299) lightdb: After request (async)
I (6209) lightdb: Counter successfully set
I (10309) lightdb: Setting counter to 1
I (10309) lightdb: Before request (sync)
I (10719) lightdb: Counter successfully set
I (10719) lightdb: After request (sync)
I (15719) lightdb: Setting counter to 2
I (15719) lightdb: Before request (json async)
I (15719) lightdb: After request (json async)
I (15939) lightdb: Counter successfully set
I (20719) lightdb: Setting counter to 3
I (20719) lightdb: Before request (cbor sync)
I (21129) lightdb: Counter successfully set
I (21139) lightdb: After request (cbor sync)
```

### Monitor counter value

Device increments counter every 5s and updates `/counter` resource in
LightDB with its value. Current value can be fetched using following
command:

```console
goliothctl lightdb get <device-name> /counter
```
