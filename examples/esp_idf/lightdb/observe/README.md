# Golioth LightDB Observe Sample

## Overview

This sample demonstrates how to connect to Golioth and observe a LightDB
path for changes.

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

Next, `cd` to the lightdb/observe example where you can build/flash/monitor:
```
cd examples/esp_idf/lightdb/observe
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
I (398) lightdb: Start LightDB observe sample
I (4638) esp_netif_handlers: sta ip: 10.0.0.202, mask: 255.255.255.0, gw: 10.0.0.1
I (4638) example_wifi: WiFi Connected. Got IP:10.0.0.202
W (4658) lightdb: Waiting for connection to Golioth...
I (5988) golioth_coap_client_libcoap: Session PSK-ID: 2device-id@project-id
I (5998) libcoap: Setting PSK key
I (6008) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (6418) golioth_coap_client_libcoap: Golioth CoAP client connected
I (6418) lightdb: Golioth client connected
I (6508) lightdb: 0x3fcb5599   35                                                |5|
I (14228) lightdb: 0x3fcb5ca9   31 30                                             |10|
I (18338) lightdb: 0x3fcb5ca9   31 31                                             |11|
I (30458) lightdb: 0x3fcb5ca9   31 32                                             |12|
```

### Set the observed value

The device retrieves the value stored at `/counter` in LightDB and then
retrieves it every time that it's updated. The value can be updates as
such:

```console
goliothctl lightdb set <device-name> /counter -b 5
goliothctl lightdb set <device-name> /counter -b 10
goliothctl lightdb set <device-name> /counter -b 11
goliothctl lightdb set <device-name> /counter -b 12
```
