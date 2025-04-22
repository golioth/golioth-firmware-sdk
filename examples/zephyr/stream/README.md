# Golioth stream sample

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

### Runtime Configuration

#### PSK based auth

We provide an option for setting Golioth credentials through the Zephyr
shell. This is based on the Zephyr Settings subsystem.

At runtime, configure PSK-ID and PSK using the device shell based on your
Golioth credentials:

```sh
uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
uart:~$ settings set golioth/psk <my-psk>
uart:-$ kernel reboot cold
```

#### WiFi Configuration

Devices that use WiFi get their WiFi credentials from the settings subsystem.
You can set the credentials with the following shell commands:

```sh
uart:~$ settings set wifi/ssid <ssid>
uart:~$ settings set wifi/psk <wifi-password>
uart:-$ kernel reboot cold
```

### Platform specific configuration

#### ESP32-DevKitC-WROVER

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/stream`) and
type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu examples/zephyr/stream
$ west flash
```

#### nRF52840 DK + ESP32

See [Golioth ESP-AT WiFi
Shield](../../../zephyr/boards/shields/golioth_esp_at/doc/index.md).

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/stream`) and
type:

```console
$ west build -b nrf52840dk/nrf52840 examples/zephyr/stream
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/stream`) and
type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/stream
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 examples/zephyr/stream
$ west flash
```

### Sample output

This is the output from the serial console:

```console
*** Booting Zephyr OS build v3.6.0 ***
[00:00:02.719,000] <inf> fs_nvs: 6 Sectors of 4096 bytes
[00:00:02.719,000] <inf> fs_nvs: alloc wra: 0, f78
[00:00:02.719,000] <inf> fs_nvs: data wra: 0, bc
[00:00:02.721,000] <dbg> golioth_stream: main: Start Golioth stream sample
[00:00:02.721,000] <inf> golioth_samples: Bringing up network interface
[00:00:02.721,000] <inf> golioth_samples: Waiting to obtain IP address
[00:00:02.721,000] <inf> golioth_wifi: Connecting to 'golioth'
[00:00:06.575,000] <inf> net_dhcpv4: Received: 192.168.200.6
[00:00:06.575,000] <inf> golioth_mbox: Mbox created, bufsize: 1232, num_items: 10, item_size: 112
[00:00:06.922,000] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:00:06.922,000] <inf> golioth_stream: Sending temperature 20.000000 (sync)
[00:00:06.923,000] <inf> golioth_stream: Golioth client connected
[00:00:06.923,000] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:00:07.007,000] <dbg> golioth_stream: temperature_push_cbor: Temperature successfully pushed
[00:00:12.007,000] <inf> golioth_stream: Sending temperature 20.500000 (async)
[00:00:12.185,000] <dbg> golioth_stream: temperature_async_push_handler: Temperature successfully pushed
[00:00:17.008,000] <inf> golioth_stream: Sending temperature 21.000000 (sync)
[00:00:17.149,000] <dbg> golioth_stream: temperature_push_cbor: Temperature successfully pushed
[00:00:22.149,000] <inf> golioth_stream: Sending temperature 21.500000 (async)
[00:00:22.323,000] <dbg> golioth_stream: temperature_async_push_handler: Temperature successfully pushed
[00:00:27.150,000] <inf> golioth_stream: Sending temperature 22.000000 (sync)
[00:00:27.341,000] <dbg> golioth_stream: temperature_push_cbor: Temperature successfully pushed
[00:00:32.341,000] <inf> golioth_stream: Sending temperature 22.500000 (async)
[00:00:32.509,000] <dbg> golioth_stream: temperature_async_push_handler: Temperature successfully pushed
[00:00:37.341,000] <inf> golioth_stream: Sending temperature 23.000000 (sync)
[00:00:37.494,000] <dbg> golioth_stream: temperature_push_cbor: Temperature successfully pushed
[00:00:42.494,000] <inf> golioth_stream: Sending temperature 23.500000 (async)
[00:00:42.703,000] <dbg> golioth_stream: temperature_async_push_handler: Temperature successfully pushed
[00:00:47.495,000] <inf> golioth_stream: Sending temperature 24.000000 (sync)
[00:00:47.616,000] <dbg> golioth_stream: temperature_push_cbor: Temperature successfully pushed
```

### Monitor temperature value over time

Device sends temperature measurements every 5s and updates the `/temp`
path in Golioth stream. The current value can be fetched using following
command:

```console
$ goliothctl stream get <Device Name> /temp
25
```

Data can be be observed in realtime using following command:

```console
$ goliothctl stream listen
{"timestamp":"2022-09-09T12:46:22.294832197Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":20}}
{"timestamp":"2022-09-09T12:46:27.301030227Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":20.5}}
{"timestamp":"2022-09-09T12:46:32.314922477Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":21}}
{"timestamp":"2022-09-09T12:46:37.321291988Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":21.5}}
{"timestamp":"2022-09-09T12:46:42.334931934Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":22}}
{"timestamp":"2022-09-09T12:46:47.344960716Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":22.5}}
{"timestamp":"2022-09-09T12:46:52.354604450Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":23}}
{"timestamp":"2022-09-09T12:46:57.362001530Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":23.5}}
{"timestamp":"2022-09-09T12:47:02.374861331Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":24}}
{"timestamp":"2022-09-09T12:47:07.384704973Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":24.5}}
{"timestamp":"2022-09-09T12:47:12.394896354Z", "deviceId":"6033cc457016b281d671df53", "data":{"temp":25}}
```

Historical data can be queried using following command:

```console
$ goliothctl stream query --interval 5m --field time --field temp | jq ''
[
  {
    "temp": 23.5,
    "time": "2024-05-07T22:19:48.622665+00:00"
  },
  {
    "temp": 24,
    "time": "2024-05-07T22:19:53.622269+00:00"
  },
  {
    "temp": 24.5,
    "time": "2024-05-07T22:19:58.693769+00:00"
  },
  {
    "temp": 25,
    "time": "2024-05-07T22:20:03.695776+00:00"
  },
  {
    "temp": 25.5,
    "time": "2024-05-07T22:20:08.770356+00:00"
  },
  {
    "temp": 26,
    "time": "2024-05-07T22:20:13.769955+00:00"
  },
  {
    "temp": 26.5,
    "time": "2024-05-07T22:20:18.847877+00:00"
  },
  {
    "temp": 27,
    "time": "2024-05-07T22:20:23.871405+00:00"
  },
  {
    "temp": 27.5,
    "time": "2024-05-07T22:20:28.998908+00:00"
  }
]
```
```console
