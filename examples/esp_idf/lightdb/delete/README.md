# Golioth LightDB Delete Sample

## Overview

This sample demonstrates how to connect to Golioth and delete values
from LightDB State.

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

Next, `cd` to the lightdb/delete example where you can build/flash/monitor:
```
cd examples/esp_idf/lightdb/delete
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
I (398) lightdb: Start LightDB delete sample
I (4638) esp_netif_handlers: sta ip: 10.0.0.202, mask: 255.255.255.0, gw: 10.0.0.1
I (4638) example_wifi: WiFi Connected. Got IP:10.0.0.202
W (4658) lightdb: Waiting for connection to Golioth...
I (5988) golioth_coap_client_libcoap: Session PSK-ID: device-id@project-id
I (5998) libcoap: Setting PSK key
I (6008) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (6418) golioth_coap_client_libcoap: Golioth CoAP client connected
I (6418) lightdb: Golioth client connected
I (3323) lightdb: Before request (async)
I (3323) lightdb: After request (async)
I (4233) lightdb: Counter deleted successfully
I (8333) lightdb: Before request (sync)
I (8533) lightdb: Counter deleted successfully
I (8543) lightdb: After request (sync)
I (13543) lightdb: Before request (async)
I (13543) lightdb: After request (async)
I (13763) lightdb: Counter deleted successfully
```

### Set counter value

The device deletes the `/counter` path in LightDB every 5 seconds. The
path (and value) can be created with:

```console
goliothctl lightdb set <device-name> /counter -b "{\"counter\":34}"
```
