# Golioth LightDB Delete Sample

## Overview

This sample demonstrates how to connect to Golioth and delete values
from LightDB.

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
this sample application (i.e., `examples/zephyr/lightdb/delete`) and
type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu examples/zephyr/lightdb/delete
$ west flash
```

#### nRF52840 DK + ESP32-WROOM-32

This subsection documents using nRF52840 DK running Zephyr with
offloaded ESP-AT WiFi driver and ESP32-WROOM-32 module based board (such
as ESP32 DevkitC rev. 4) running WiFi stack. See [AT Binary
Lists](https://docs.espressif.com/projects/esp-at/en/latest/AT_Binary_Lists/index.html)
for links to ESP-AT binaries and details on how to flash ESP-AT image on
ESP chip. Flash ESP chip with following command:

```console
esptool.py write_flash --verify 0x0 PATH_TO_ESP_AT/factory/factory_WROOM-32.bin
```

Connect nRF52840 DK and ESP32-DevKitC V4 (or other ESP32-WROOM-32 based
board) using wires:

| nRF52840 DK | ESP32-WROOM-32  | ESP32-WROVER-32 |
| ----------- | --------------- | ----------------|
| P1.01 (RX)  | IO17 (TX)       | IO22 (TX)       |
| P1.02 (TX)  | IO16 (RX)       | IO19 (RX)       |
| P1.03 (CTS) | IO14 (RTS)      | IO14 (RTS)      |
| P1.04 (RTS) | IO15 (CTS)      | IO15 (CTS)      |
| P1.05       | EN              | EN              |
| GND         | GND             | GND             |

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/lightdb/delete`) and
type:

```console
$ west build -b nrf52840dk/nrf52840 examples/zephyr/lightdb/delete
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/lightdb/delete`) and
type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/lightdb/delete
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 examples/zephyr/lightdb/delete
$ west flash
```

### Sample output

This is the output from the serial console:

```console
*** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.010,000] <wrn> net_sock_tls: No entropy device on the system, TLS communication is insecure!
[00:00:00.010,000] <inf> net_config: Initializing network
[00:00:00.010,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.010,000] <dbg> lightdb_delete: main: Start LightDB delete sample
[00:00:00.010,000] <inf> golioth_samples: Waiting for interface to be up
[00:00:00.010,000] <inf> golioth_mbox: Mbox created, bufsize: 1100, num_items: 10, item_size: 100
[00:00:00.060,000] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:00.060,000] <inf> golioth_coap_client: Session PSK-ID: device-id@project-name
[00:00:00.070,000] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:01.260,000] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:01.260,000] <dbg> lightdb_delete: main: Before request (async)
[00:00:01.260,000] <dbg> lightdb_delete: main: After request (async)
[00:00:01.260,000] <inf> lightdb_delete: Golioth client connected
[00:00:01.310,000] <dbg> lightdb_delete: counter_handler: Counter deleted successfully
[00:00:06.270,000] <dbg> lightdb_delete: main: Before request (sync)
[00:00:06.320,000] <dbg> lightdb_delete: counter_delete_sync: Counter deleted successfully
[00:00:06.320,000] <dbg> lightdb_delete: main: After request (sync)
[00:00:11.330,000] <dbg> lightdb_delete: main: Before request (async)
[00:00:11.330,000] <dbg> lightdb_delete: main: After request (async)
[00:00:11.370,000] <dbg> lightdb_delete: counter_handler: Counter deleted successfully
[00:00:16.340,000] <dbg> lightdb_delete: main: Before request (sync)
[00:00:16.380,000] <dbg> lightdb_delete: counter_delete_sync: Counter deleted successfully
[00:00:16.380,000] <dbg> lightdb_delete: main: After request (sync)
```

### Set counter value

The device deletes the `/counter` path in LightDB every 5 seconds. The
path (and value) can be created with:

```console
goliothctl lightdb set <device-name> /counter -b "{\"counter\":34}"
```
