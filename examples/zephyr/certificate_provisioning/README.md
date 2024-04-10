# Golioth Certificate Provisioning Sample

## Overview

This sample application demonstrates one method for provisioning
certificates onto a device for use in DTLS authentication. Certificates
are loaded into the device's filesystem using ``mcumgr``.

## Requirements

* Golioth credentials
* Network connectivity
* A filesystem
* ``mcumgr`` CLI tool

## Building and Running

### Platform specific configuration

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

| nRF52840 DK | ESP32-WROOM-32  | ESP32-WROVER-32 | ESP32-C3-MINI-1 |
| ----------- | --------------- | ----------------| ----------------|
| P1.01 (RX)  | IO17 (TX)       | IO22 (TX)       | IO7 (TX)        |
| P1.02 (TX)  | IO16 (RX)       | IO19 (RX)       | IO6 (RX)        |
| P1.03 (CTS) | IO14 (RTS)      | IO14 (RTS)      | IO4 (RTS)       |
| P1.04 (RTS) | IO15 (CTS)      | IO15 (CTS)      | IO5 (CTS)       |
| P1.05       | EN              | EN              | EN              |
| GND         | GND             | GND             | GND             |

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
this sample application (i.e., `examples/zephyr/certificate_provisioning`) and type:

```console
$ west build -b nrf52840dk_nrf52840 examples/zephyr/certificate_provisioning
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/certificate_provisioning`) and type:

```console
$ west build -b nrf9160dk_nrf9160_ns examples/zephyr/certificate_provisioning
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010_nrf52840 examples/zephyr/certificate_provisioning
$ west flash
```

## Installing `mcumgr`

For full instructions, see
[mcumgr](https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html).

1. Install go from https://go.dev/doc/install
2. Install the mcumgr tool:

```console
$ go install github.com/apache/mynewt-mcumgr-cli/mcumgr@latest
```

## Creating Certificates

This sample requires that you have:

* A root or intermediate certificate uploaded to the Golioth console
* A client certificate signed by the private key associated with the
  root or intermediate certificate
* The private key associated with the client certificate

For instructions on generating and positioning these certificates, see
[golioth cert
auth](https://docs.golioth.io/firmware/golioth-firmware-sdk/authentication/certificate-auth).

## Provisioning Certificates

This sample application uses certificates stored on the device's
filesystem at `/lfs1/credentials`. It enables the `mcumgr` device
management subsystem to enable file upload from a host computer to the
device over a serial connection.

Certificate authentication requires two files:

1. A Client Certificate, located at `/lfs1/credentials/client_cert.der`
2. A Private Key, located at `/lfs1/credentials/private_key.der`

### Loading Files:

First, open a serial connection to the device, and enter the following
commands:

```console
uart:~$ fs mkdir /lfs1/credentials
uart:~$ log halt
```

This will stop logs from being printed to the console to prevent them
from interfering with the file upload.

Next, exit the serial console, and from the host computer run the
following:

```console
$ mcumgr --conntype serial --connstring=dev=<path/to/your/device>,baud=115200 fs upload keys/client_certificate.der /lfs1/credentials/client_cert.der
$ mcumgr --conntype serial --connstring=dev=<path/to/your/device>,baud=115200 fs upload keys/private_key.der /lfs1/credentials/private_key.der
```

Be sure to replace `<path/to/your/device>` with the appropriate serial
device for your board, typically something like
`/dev/cu.usbmodem0009600837441`.

Finally, re-open a serial connection and reset your device:

```console
uart:-$ kernel reboot cold
```

### Sample output

The length and filesystem path of the credentials will be logged,
indicating a successful read operation.

```console
*** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.426,849] <dbg> hello_zephyr: main: Start certificate provisioning sample
[00:00:00.426,879] <inf> littlefs: LittleFS version 2.5, disk version 2.0
[00:00:00.427,093] <inf> littlefs: FS at flash-controller@4001e000:0xf8000 is 8 0x1000-byte blocks with 512 cycle
[00:00:00.427,124] <inf> littlefs: sizes: rd 16 ; pr 16 ; ca 64 ; la 32
[00:00:00.429,626] <inf> hello_zephyr: Read 352 bytes from /lfs1/credentials/client_cert.der
[00:00:00.431,243] <inf> hello_zephyr: Read 121 bytes from /lfs1/credentials/private_key.der
[00:00:00.431,274] <inf> golioth_samples: Waiting for interface to be up
[00:00:02.573,303] <inf> wifi_esp_at: AT version: 2.4.0.0(s-4c6eb5e - ESP32 - May 20 2022 03:12:58)
[00:00:02.576,477] <inf> wifi_esp_at: SDK version: qa-test-v4.3.3-20220423
[00:00:02.582,885] <inf> wifi_esp_at: Bin version: 2.4.0(WROVER-32)
[00:00:03.017,608] <inf> wifi_esp_at: ESP Wi-Fi ready
[00:00:03.017,700] <inf> golioth_samples: Connecting to WiFi
[00:00:03.119,995] <inf> golioth_wifi: Connected with status: 0
Connected
[00:00:03.120,056] <inf> golioth_wifi: Successfully connected to WiFi
[00:00:03.120,147] <inf> golioth_mbox: Mbox created, bufsize: 1144, num_items: 10, item_size: 104
[00:00:03.839,843] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:03.840,209] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:10.388,061] <err> net_pkt: Data buffer (1225) allocation failed.
[00:00:10.388,092] <err> wifi_esp_at: Failed to get net_pkt: len 1225
[00:00:14.189,300] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:14.189,514] <inf> hello_zephyr: Sending hello! 0
[00:00:14.189,575] <inf> hello_zephyr: Golioth client connected
[00:00:19.189,575] <inf> hello_zephyr: Sending hello! 1
[00:00:24.189,666] <inf> hello_zephyr: Sending hello! 2
```

In this example, the nRF52840 stops servicing the receive buffer while
calculating the handshake. This causes the buffer overflow errors shown
above. This is expected behavior and doesn't adversely affect the
connection process.
