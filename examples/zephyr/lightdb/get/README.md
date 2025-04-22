# Golioth LightDB Get Sample

## Overview

This sample demonstrates how to connect to Golioth and get values from
LightDB.

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
this sample application (i.e., `examples/zephyr/lightdb/get`) and type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu examples/zephyr/lightdb/get
$ west flash
```

#### nRF52840 DK + ESP32-WROOM-32

See [Golioth ESP-AT WiFi
Shield](../../../../zephyr/boards/shields/golioth_esp_at/doc/index.md).

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/lightdb/get`) and type:

```console
$ west build -b nrf52840dk/nrf52840 --shield golioth_esp_at examples/zephyr/lightdb/get
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/lightdb/get`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/lightdb/get
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 examples/zephyr/lightdb/get
$ west flash
```

### Sample output

This is the output from the serial console:

```console
** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.020,000] <wrn> net_sock_tls: No entropy device on the system, TLS communication is insecure!
[00:00:00.020,000] <inf> net_config: Initializing network
[00:00:00.020,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.020,000] <dbg> lightdb_get: main: Start LightDB get sample
[00:00:00.020,000] <inf> golioth_samples: Waiting for interface to be up
[00:00:00.020,000] <inf> golioth_mbox: Mbox created, bufsize: 1100, num_items: 10, item_size: 100
[00:00:00.060,000] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:00.060,000] <inf> golioth_coap_client: Session PSK-ID: device-id@project-name
[00:00:00.070,000] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:01.270,000] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:01.270,000] <inf> lightdb_get: Before request (async)
[00:00:01.270,000] <inf> lightdb_get: After request (async)
[00:00:01.270,000] <inf> lightdb_get: Golioth client connected
[00:00:01.320,000] <inf> lightdb_get: Counter (async): 10
[00:00:06.280,000] <inf> lightdb_get: Before request (sync)
[00:00:06.330,000] <inf> lightdb_get: Counter (sync): 10
[00:00:06.330,000] <inf> lightdb_get: After request (sync)
[00:00:11.340,000] <inf> lightdb_get: Before JSON request (sync)
[00:00:11.390,000] <inf> lightdb_get: LightDB JSON (sync)
                                       22 63 6f 75 6e 74 65 72  22 3a 31 31             |"counter ":11
[00:00:11.390,000] <inf> lightdb_get: After JSON request (sync)
[00:00:16.400,000] <inf> lightdb_get: Before request (async)
[00:00:16.400,000] <inf> lightdb_get: After request (async)
[00:00:16.440,000] <inf> lightdb_get: Counter (async): 12
```

### Set counter value

The device retrieves the value stored at `/counter` in LightDB every 5
seconds. The value can be set with:

```console
goliothctl lightdb set <device-name> /counter -b 10
goliothctl lightdb set <device-name> /counter -b 11
goliothctl lightdb set <device-name> /counter -b 12
```
