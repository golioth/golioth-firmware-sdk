# Golioth Settings sample

## Overview

This sample application demonstrates how to use the Golioth Device
Settings service to register for automatic updates when a setting is
changed on the Golioth Cloud.

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
this sample application (i.e., `examples/zephyr/settings`) and type:

```console
$ west build -b esp32_devkitc_wrover/esp32/procpu examples/zephyr/settings
$ west flash
```

#### nRF52840 DK + ESP32-WROOM-32

See [Golioth ESP-AT WiFi
Shield](../../../zephyr/boards/shields/golioth_esp_at/doc/index.md).

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/settings`) and type:

```console
$ west build -b nrf52840dk/nrf52840 --shield golioth_esp_at examples/zephyr/settings
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/settings`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/settings
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 examples/zephyr/settings
$ west flash
```

### Receiving remote settings updates

This sample registers to receive settings updates made to the
`LOOP_DELAY_S` key. To create this key, go to the [Golioth Web
Console](https://console.golioth.io) and select `Device Settings` in the
left sidebar. Click on the `Create` button in the upper right and enter
your settings values in the modal window:

* Key: `LOOP_DELAY_S`
* Data Type: `Integer`
* Value: your desired delay (in seconds) between each "hello" message
  * this sample tests received values for a range of 1 to 300

This creates a value that all devices in your fleet will receive
(project-wide). The setting may be changed at the
blueprint level, and per-device. The device will receive the settings
value each time it powers up, and whenever the value is updated on the
cloud.

### Sample output

```console
*** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.010,000] <wrn> net_sock_tls: No entropy device on the system, TLS communication is insecure!
[00:00:00.010,000] <inf> net_config: Initializing network
[00:00:00.010,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.010,000] <dbg> device_settings: main: Start Golioth Device Settings sample
[00:00:00.010,000] <inf> golioth_samples: Waiting for interface to be up
[00:00:00.010,000] <inf> golioth_mbox: Mbox created, bufsize: 1100, num_items: 10, item_size: 100
[00:00:00.230,000] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:00.230,000] <inf> golioth_coap_client: Session PSK-ID: 20230816174513-qemu@ttgo-demo
[00:00:00.230,000] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:02.630,000] <inf> device_settings: Setting loop delay to 30 s
[00:00:02.630,000] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:02.630,000] <inf> device_settings: Sending hello! 0
[00:00:02.630,000] <inf> device_settings: Golioth client connected
[00:00:20.830,000] <inf> device_settings: Setting loop delay to 5 s
[00:00:32.640,000] <inf> device_settings: Sending hello! 1
[00:00:37.650,000] <inf> device_settings: Sending hello! 2
[00:00:42.660,000] <inf> device_settings: Sending hello! 3
```

From the sample output we can see that when the device first connects to
Golioth, it receives a loop delay setting of 30 seconds. After the first
message is sent, the device receives an updated loop delay value of 5
seconds, and begins sending messages using that delay.
