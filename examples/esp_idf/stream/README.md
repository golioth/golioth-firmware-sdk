# Golioth Stream sample

## Overview

This Golioth data streaming application demonstrates how to connect with
Golioth and periodically send data to the stream service. In this sample
temperature measurements will be displayed on the `/temp` stream
path. For platforms that do not have a temperature sensor, a value is
generated from 20 up to 30.

> :information-source: To receive stream data, a Pipeline must be
> configured for the project on Golioth. This example is configured
> to work with the default Pipeline present for every project upon creation.

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

Next, `cd` to the stream example where you can build/flash/monitor:

```
cd examples/esp_idf/stream
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
I (398) stream: Start Stream sample
I (2214) esp_netif_handlers: sta ip: 10.0.0.202, mask: 255.255.255.0, gw: 10.0.0.1
I (2214) example_wifi: WiFi Connected. Got IP:10.0.0.202
W (2234) stream: Waiting for connection to Golioth...
I (2494) golioth_coap_client_libcoap: Session PSK-ID: device-id@project-id
I (2504) libcoap: Setting PSK key
I (2514) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (2994) golioth_coap_client_libcoap: Golioth CoAP client connected
I (3119) stream: Golioth client connected
I (3129) stream: Sending temperature 20.000000
I (3899) stream: Temperature successfully pushed
I (8899) stream: Sending temperature 20.500000
I (9289) stream: Temperature successfully pushed
I (13899) stream: Sending temperature 21.000000
I (14179) stream: Temperature successfully pushed
I (19189) stream: Sending temperature 21.500000
I (19719) stream: Temperature successfully pushed
I (24189) stream: Sending temperature 22.000000
I (24529) stream: Temperature successfully pushed
I (29529) stream: Sending temperature 22.500000
I (29749) stream: Temperature successfully pushed
I (34529) stream: Sending temperature 23.000000
I (34869) stream: Temperature successfully pushed
I (39869) stream: Sending temperature 23.500000
I (40189) stream: Temperature successfully pushed
I (44869) stream: Sending temperature 24.000000
I (45119) stream: Temperature successfully pushed
I (50119) stream: Sending temperature 24.500000
I (50429) stream: Temperature successfully pushed
I (55119) stream: Sending temperature 25.000000
I (55239) stream: Temperature successfully pushed
```

### Monitor temperature value over time

Device sends temperature measurements every 5s to the `/temp` stream path,
which the default Pipeline will route to LightDB Stream. The latest value can
be fetched using the following command:

```console
$ goliothctl stream get <device-name> /temp
25
```

Data can be be observed in realtime using following command:

```console
$ goliothctl stream listen
{"timestamp":"2024-03-13T18:07:34.022122682Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":20}}
{"timestamp":"2024-03-13T18:07:39.283424059Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":20.5}}
{"timestamp":"2024-03-13T18:07:44.179455106Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":21}}
{"timestamp":"2024-03-13T18:07:49.548594450Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":21.5}}
{"timestamp":"2024-03-13T18:07:54.603702175Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":22}}
{"timestamp":"2024-03-13T18:07:59.836510643Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":22.5}}
{"timestamp":"2024-03-13T18:08:04.880963423Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":23}}
{"timestamp":"2024-03-13T18:08:10.174147969Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":23.5}}
{"timestamp":"2024-03-13T18:08:15.210617990Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":24}}
{"timestamp":"2024-03-13T18:08:20.447119383Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":24.5}}
{"timestamp":"2024-03-13T18:08:25.359690767Z","deviceId":"65b90cc76669d5765b9e61b0","data":{"temp":25}}
```

Historical data can be queried using following command:

```console
$ goliothctl stream query --interval 5m --field time --field sensor | jq
[
  {
    "temp": 20,
    "time": "2024-03-13T18:07:34.022123+00:00"
  },
  {
    "temp": 20.5,
    "time": "2024-03-13T18:07:39.283424+00:00"
  },
  {
    "temp": 21,
    "time": "2024-03-13T18:07:44.179455+00:00"
  },
  {
    "temp": 21.5,
    "time": "2024-03-13T18:07:49.548594+00:00"
  },
  {
    "temp": 22,
    "time": "2024-03-13T18:07:54.603702+00:00"
  },
  {
    "temp": 22.5,
    "time": "2024-03-13T18:07:59.836511+00:00"
  },
  {
    "temp": 23,
    "time": "2024-03-13T18:08:04.880963+00:00"
  },
  {
    "temp": 23.5,
    "time": "2024-03-13T18:08:10.174148+00:00"
  },
  {
    "temp": 24,
    "time": "2024-03-13T18:08:15.210618+00:00"
  },
  {
    "temp": 24.5,
    "time": "2024-03-13T18:08:20.447119+00:00"
  },
  {
    "temp": 25,
    "time": "2024-03-13T18:08:25.359691+00:00"
  }
]
```
