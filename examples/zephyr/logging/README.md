# Golioth Logging sample

## Overview

This sample application demonstrates how to connect with Golioth and configure the logging backend
to send system logs to Golioth.

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
this sample application (i.e., `examples/zephyr/logging`) and type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu examples/zephyr/logging
$ west flash
```

#### nRF52840 DK + ESP32-WROOM-32

See [Golioth ESP-AT WiFi
Shield](../../../zephyr/boards/shields/golioth_esp_at/doc/index.md).

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/logging`) and type:

```console
$ west build -b nrf52840dk/nrf52840 examples/zephyr/logging
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/logging`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/logging
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 examples/zephyr/logging
$ west flash
```

### Sample output

This is the output from the serial console:

```console
*** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.010,000] <wrn> net_sock_tls: No entropy device on the system, TLS communication is insecure!
[00:00:00.010,000] <inf> net_config: Initializing network
[00:00:00.010,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.010,000] <dbg> golioth_logging: main: start logging sample
[00:00:00.010,000] <inf> golioth_samples: Waiting for interface to be up
[00:00:00.010,000] <inf> golioth_mbox: Mbox created, bufsize: 1100, num_items: 10, item_size: 100
[00:00:00.040,000] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:00.040,000] <inf> golioth_coap_client: Session PSK-ID: 20230816174513-qemu@ttgo-demo
[00:00:00.040,000] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:01.630,000] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:01.630,000] <dbg> golioth_logging: main: Debug info! 0
[00:00:01.630,000] <dbg> golioth_logging: func_1: Log 1: 0
[00:00:01.630,000] <dbg> golioth_logging: func_2: Log 2: 0
[00:00:01.630,000] <wrn> golioth_logging: Warn: 0
[00:00:01.630,000] <err> golioth_logging: Err: 0
[00:00:01.630,000] <inf> golioth_logging: Counter hexdump
                                          00 00 00 00                                      |....
[00:00:01.630,000] <inf> golioth_logging: Golioth client connected
[00:00:06.640,000] <dbg> golioth_logging: main: Debug info! 1
[00:00:06.640,000] <dbg> golioth_logging: func_1: Log 1: 1
[00:00:06.640,000] <dbg> golioth_logging: func_2: Log 2: 1
[00:00:06.640,000] <wrn> golioth_logging: Warn: 1
[00:00:06.640,000] <err> golioth_logging: Err: 1
[00:00:06.640,000] <inf> golioth_logging: Counter hexdump
                                          00 00 00 01                                      |....
```

### Access logs with goliothctl

Logs are viewable on the [Golioth Web Console](https://console.golioth.io) or you may use the
`goliothctl` command line tool:

```console
$ goliothctl logs
[2023-08-18T14:27:01Z] level:INFO  module:"golioth_coap_client"  message:"Golioth CoAP client connected"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:01Z] level:DEBUG  module:"golioth_logging"  message:"main: Debug info! 0"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:01Z] level:DEBUG  module:"golioth_logging"  message:"func_1: Log 1: 0"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:02Z] level:DEBUG  module:"golioth_logging"  message:"func_2: Log 2: 0"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:02Z] level:WARN  module:"golioth_logging"  message:"Warn: 0"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:02Z] level:ERROR  module:"golioth_logging"  message:"Err: 0"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:02Z] level:INFO  module:"golioth_logging"  message:"Counter hexdump"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:02Z] level:INFO  module:"golioth_logging"  message:"Golioth client connected"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:06Z] level:DEBUG  module:"golioth_logging"  message:"main: Debug info! 1"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:06Z] level:DEBUG  module:"golioth_logging"  message:"func_1: Log 1: 1"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:06Z] level:DEBUG  module:"golioth_logging"  message:"func_2: Log 2: 1"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:06Z] level:WARN  module:"golioth_logging"  message:"Warn: 1"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"

[2023-08-18T14:27:06Z] level:ERROR  module:"golioth_logging"  message:"Err: 1"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
[2023-08-18T14:27:07Z] level:INFO  module:"golioth_logging"  message:"Counter hexdump"  device_id:"64dd0b29da01bd555b64d5a4" metadata:"{}"
```
