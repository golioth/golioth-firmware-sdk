# Golioth location demo

> [!IMPORTANT]
> Using [Golioth
> Location](https://docs.golioth.io/application-services/location/)
> requires enrolling in the [private access
> program](https://blog.golioth.io/golioth-location-private-access/).

## Overview

This sample demonstrates how to connect to Golioth and get location
using cellular network cell towers information and/or scanned WiFi
networks.

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

#### Native Simulator

This application has been built and tested with Native Simulator
(native_sim).

On your Linux host computer, open a terminal window, locate the source
code of this sample application (i.e., `examples/zephyr/location`)
and type:

```console
$ west build -b native_sim examples/zephyr/location
$ west build -t run
```

#### ESP32-DevKitC-WROVER

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/location`) and type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu examples/zephyr/location
$ west flash
```

#### nRF52840 DK + ESP32-WROOM-32

See [Golioth ESP-AT WiFi
Shield](../../../zephyr/boards/shields/golioth_esp_at/doc/index.md).

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/location`) and type:

```console
$ west build -b nrf52840dk/nrf52840 --shield golioth_esp_at examples/zephyr/location
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/location`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/location
$ west flash
```

### Sample output

This is the output from the serial console:

```console
*** Booting Zephyr OS build v4.0.0-1-gb39d67f51986 ***
[00:00:00.000,000] <inf> net_config: Initializing network
[00:00:00.000,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.000,000] <dbg> location_main: main: Start location sample
[00:00:00.000,000] <inf> golioth_mbox: Mbox created, bufsize: 1848, num_items: 10, item_size: 168
[00:00:00.400,003] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:00:00.400,003] <inf> location_main: Golioth client connected
[00:00:00.400,003] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:00:00.580,002] <inf> location_main: 50.663974800 17.942322850 (32)
[00:00:00.760,001] <inf> location_main: 50.664181170 17.942337360 (23)
[00:00:01.170,001] <inf> location_main: 50.664464180 17.942332180 (22)
[00:00:01.700,001] <inf> location_main: 50.665216090 17.942386110 (26)
[00:00:02.190,001] <inf> location_main: 50.665924320 17.942342850 (17)
[00:00:02.690,001] <inf> location_main: 50.666588620 17.942297690 (21)
[00:00:03.180,001] <inf> location_main: 50.667436140 17.942253480 (25)
[00:00:03.680,001] <inf> location_main: 50.667930110 17.942142910 (27)
[00:00:04.170,001] <inf> location_main: 50.668083230 17.942170380 (33)
[00:00:04.670,001] <inf> location_main: 50.669921000 17.948761000 (569)
[00:00:05.170,001] <inf> location_main: 50.667979840 17.942204310 (36)
[00:00:05.660,001] <inf> location_main: 50.665045000 17.947862000 (604)
[00:00:06.170,001] <inf> location_main: 50.668312550 17.942551440 (31)
[00:00:06.670,001] <inf> location_main: 50.668242700 17.942328220 (52)
[00:00:07.170,001] <inf> location_main: 50.668386960 17.942680660 (46)
[00:00:07.670,001] <inf> location_main: 50.668344030 17.943467020 (37)
[00:00:08.180,001] <inf> location_main: 50.668361530 17.943982820 (50)
[00:00:08.680,001] <inf> location_main: 50.668426910 17.945390790 (58)
[00:00:09.170,001] <inf> location_main: 50.668490680 17.947672650 (43)
[00:00:09.780,001] <inf> location_main: 50.668505260 17.947435020 (34)
[00:00:10.160,001] <inf> location_main: 50.668612950 17.948811810 (71)
[00:00:10.670,001] <inf> location_main: 50.668666040 17.948747050 (107)
[00:00:11.270,001] <inf> location_main: 50.664927000 17.947822000 (593)
```
