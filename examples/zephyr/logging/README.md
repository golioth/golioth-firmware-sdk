# Golioth Logging sample

## Overview

This sample application demonstrates how to connect with Golioth and configure the logging backend
to send system logs to Golioth.

## Requirements

* Golioth credentials
* Network connectivity

## Building and Running

### Authentication specific configuration

#### PSK based auth - Hardcoded

Configure the following Kconfig options based on your Golioth
credentials:

* GOLIOTH_SAMPLE_PSK_ID - PSK ID of registered device
* GOLIOTH_SAMPLE_PSK - PSK of registered device

by adding these lines to configuration file (e.g. `prj.conf`):

```cfg
CONFIG_GOLIOTH_SAMPLE_PSK_ID="my-psk-id"
CONFIG_GOLIOTH_SAMPLE_PSK="my-psk"
```

#### PSK based auth - Runtime

We provide an option for setting Golioth credentials through the Zephyr
shell. This is based on the Zephyr Settings subsystem.

Enable the settings shell by including the following configuration overlay
file:

```sh
$ west build -- -DEXTRA_CONF_FILE=../common/runtime_settings.conf
```

Alternatively, you can add the following options to ``prj.conf``:

```cfg
CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS=n

CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_NVS=y

CONFIG_SETTINGS=y
CONFIG_SETTINGS_RUNTIME=y
CONFIG_GOLIOTH_SAMPLE_PSK_SETTINGS=y
CONFIG_GOLIOTH_SAMPLE_SETTINGS_AUTOLOAD=y
CONFIG_GOLIOTH_SAMPLE_SETTINGS_SHELL=y
```

At runtime, configure PSK-ID and PSK using the device shell based on your
Golioth credentials:

```sh
uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
uart:~$ settings set golioth/psk <my-psk>
uart:-$ kernel reboot cold
```

#### Certificate based auth

Configure the following Kconfig options based on your Golioth
credentials:

* CONFIG_GOLIOTH_AUTH_METHOD_CERT - use certificate-based
    authentication
* CONFIG_GOLIOTH_SAMPLE_HARDCODED_CRT_PATH - device certificate
* CONFIG_GOLIOTH_SAMPLE_HARDCODED_KEY_PATH - device private key

by adding these lines to configuration file (e.g. `prj.conf`):

```cfg
CONFIG_GOLIOTH_AUTH_METHOD_CERT=y
CONFIG_GOLIOTH_SAMPLE_HARDCODED_CRT_PATH="keys/device.crt.der"
CONFIG_GOLIOTH_SAMPLE_HARDCODED_KEY_PATH="keys/device.key.der"
```

### Platform specific configuration

#### QEMU

This application has been built and tested with QEMU x86 (qemu_x86).

On your Linux host computer, open a terminal window, locate the source
code of this sample application (i.e., `examples/zephyr/logging`) and
type:

```console
$ west build -b qemu_x86 examples/zephyr/logging
$ west build -t run
```

See [Networking with
QEMU](https://docs.zephyrproject.org/3.3.0/connectivity/networking/qemu_setup.html)
on how to setup networking on host and configure NAT/masquerading to
access Internet.

#### ESP32-DevKitC-WROVER

Configure the following Kconfig options based on your WiFi AP
credentials:

- GOLIOTH_SAMPLE_WIFI_SSID  - WiFi SSID
- GOLIOTH_SAMPLE_WIFI_PSK   - WiFi PSK

by adding these lines to configuration file (e.g. `prj.conf` or
`board/esp32_devkitc_wrover.conf`):

```cfg
CONFIG_GOLIOTH_SAMPLE_WIFI_SSID="my-wifi"
CONFIG_GOLIOTH_SAMPLE_WIFI_PSK="my-psk"
```

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/logging`) and type:

```console
$ west build -b esp32_devkitc_wrover examples/zephyr/logging
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

Configure the following Kconfig options based on your WiFi AP
credentials:

* GOLIOTH_SAMPLE_WIFI_SSID - WiFi SSID
* GOLIOTH_SAMPLE_WIFI_PSK - WiFi PSK

by adding these lines to configuration file (e.g. `prj.conf` or
`board/nrf52840dk_nrf52840.conf`):

```cfg
CONFIG_GOLIOTH_SAMPLE_WIFI_SSID="my-wifi"
CONFIG_GOLIOTH_SAMPLE_WIFI_PSK="my-psk"
```

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/logging`) and type:

```console
$ west build -b nrf52840dk_nrf52840 examples/zephyr/logging
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/logging`) and type:

```console
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/logging
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
