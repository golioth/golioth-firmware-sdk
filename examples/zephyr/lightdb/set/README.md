# Golioth LightDB Set Sample

## Overview

This sample demonstrates how to connect to Golioth and set values inside
of LightDB.

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
this sample application (i.e., `examples/zephyr/lightdb/set`) and type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu examples/zephyr/lightdb/set
$ west flash
```

#### nRF52840 DK + ESP32-WROOM-32

See [Golioth ESP-AT WiFi
Shield](../../../../zephyr/boards/shields/golioth_esp_at/doc/index.md).

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/lightdb/set`) and type:

```console
$ west build -b nrf52840dk/nrf52840 --shield golioth_esp_at examples/zephyr/lightdb/set
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/lightdb/set`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/lightdb/set
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 examples/zephyr/lightdb/set
$ west flash
```

### Sample output

This is the output from the serial console:

```console
[00:00:00.000,000] <inf> golioth_system: Initializing
[00:00:00.000,000] <inf> net_config: Initializing network
[00:00:00.000,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.000,000] <dbg> golioth_lightdb: main: Start LightDB set sample
[00:00:00.010,000] <inf> golioth_system: Starting connect
[00:00:00.030,000] <dbg> golioth_lightdb: main: Setting counter to 0
[00:00:00.030,000] <dbg> golioth_lightdb: main: Before request (async)
[00:00:00.030,000] <dbg> golioth_lightdb: main: After request (async)
[00:00:00.030,000] <inf> golioth_system: Client connected!
[00:00:00.030,000] <dbg> golioth_lightdb: counter_set_handler: Counter successfully set
[00:00:05.040,000] <dbg> golioth_lightdb: main: Setting counter to 1
[00:00:05.040,000] <dbg> golioth_lightdb: main: Before request (sync)
[00:00:05.040,000] <dbg> golioth_lightdb: counter_set_sync: Counter successfully set
[00:00:05.040,000] <dbg> golioth_lightdb: main: After request (sync)
[00:00:10.050,000] <dbg> golioth_lightdb: main: Setting counter to 2
[00:00:10.050,000] <dbg> golioth_lightdb: main: Before request (async)
[00:00:10.050,000] <dbg> golioth_lightdb: main: After request (async)
[00:00:10.050,000] <dbg> golioth_lightdb: counter_set_handler: Counter successfully set
[00:00:15.060,000] <dbg> golioth_lightdb: main: Setting counter to 3
[00:00:15.060,000] <dbg> golioth_lightdb: main: Before request (sync)
[00:00:15.060,000] <dbg> golioth_lightdb: counter_set_sync: Counter successfully set
[00:00:15.060,000] <dbg> golioth_lightdb: main: After request (sync)
```

### Monitor counter value

Device increments counter every 5s and updates `/counter` resource in
LightDB with its value. Current value can be fetched using following
command:

```console
goliothctl lightdb get <device-name> /counter
```
