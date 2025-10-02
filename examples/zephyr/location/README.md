# Golioth location sample

## Overview

This sample demonstrates how to use cell tower and / or Wi-Fi access
point information to stream device position to any destination using
[Golioth
Location](https://docs.golioth.io/application-services/location/) and
[Pipelines](https://docs.golioth.io/data-routing).

> [!NOTE]
> To successfully deliver device position to a destination, you must
> have an appropriate Pipeline configured in your Golioth project. This
> example works with the Golioth Location Pipelines examples in the
> [documentation](https://docs.golioth.io/data-routing/examples/location).

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

Devices that use WiFi use Zephyr's [WiFi Credentials](https://docs.zephyrproject.org/latest/connectivity/networking/api/wifi_credentials.html)
library. The `wifi cred add` shell command accepts network SSID and security
information and is compatible with a variety of WiFi security mechanisms.
For example, to add a network that uses WPA2-PSK:

```sh
uart:~$ wifi cred add -k 1 -s <my-ssid> -p <my-psk>
uart:-$ wifi cred auto_connect
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

#### ESP32-DevKitC

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/location`) and type:

```console
$ west build -b esp32_devkitc/esp32/procpu examples/zephyr/location
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
*** Booting nRF Connect SDK v3.0.1-9eb5615da66b ***
*** Using Zephyr OS v4.0.99-77f865b8f8d0 ***
*** Golioth Firmware SDK v0.18.1-25-g4939c82e47d7 ***
[00:00:00.000,000] <inf> net_config: Initializing network
[00:00:00.000,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.000,000] <dbg> location_main: main: Start location sample
[00:00:00.000,000] <inf> golioth_mbox: Mbox created, bufsize: 2024, num_items: 10, item_size: 184
[00:00:00.390,003] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:00:00.390,003] <inf> location_main: Golioth client connected
[00:00:00.390,003] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:00:15.010,000] <inf> location_main: block-idx: 0 bu_offset: 0 bytes_remaining: 378
[00:00:15.330,001] <inf> location_main: Successfully streamed network data
[00:00:20.010,000] <inf> location_main: block-idx: 0 bu_offset: 0 bytes_remaining: 1082
[00:00:20.070,001] <inf> location_main: block-idx: 1 bu_offset: 1024 bytes_remaining: 58
[00:00:20.220,002] <inf> location_main: Successfully streamed network data
[00:00:25.010,000] <inf> location_main: block-idx: 0 bu_offset: 0 bytes_remaining: 1428
[00:00:25.070,001] <inf> location_main: block-idx: 1 bu_offset: 1024 bytes_remaining: 404
[00:00:25.170,002] <inf> location_main: Successfully streamed network data
[00:00:35.010,000] <inf> location_main: block-idx: 0 bu_offset: 0 bytes_remaining: 1248
[00:00:35.070,001] <inf> location_main: block-idx: 1 bu_offset: 1024 bytes_remaining: 224
[00:00:35.170,002] <inf> location_main: Successfully streamed network data
[00:00:40.010,000] <inf> location_main: block-idx: 0 bu_offset: 0 bytes_remaining: 1112
[00:00:40.080,001] <inf> location_main: block-idx: 1 bu_offset: 1024 bytes_remaining: 88
[00:00:40.200,002] <inf> location_main: Successfully streamed network data
```
