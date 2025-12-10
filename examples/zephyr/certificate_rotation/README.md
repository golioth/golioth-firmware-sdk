# Golioth Certificate Rotation Sample

## Overview

This sample application demonstrates the certificate rotation API. The initial
credentials are loaded into the device's filesystem using ``mcumgr``. The
application uses the initial credentials to request a new certificate from
Golioth, then starts pushing log messages with the new credentials.

## Requirements

* Golioth credentials
* Network connectivity
* A filesystem
* ``mcumgr`` CLI tool
* A [configured PKI provider](https://docs.golioth.io/connectivity/credentials/pki)

## Building and Running

### Runtime Configuration

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

#### nRF52840 DK + ESP32-WROOM-32

See [Golioth ESP-AT WiFi
Shield](../../../zephyr/boards/shields/golioth_esp_at/doc/index.md).

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/certificate_rotation`) and type:

```console
$ west build -b nrf52840dk/nrf52840 --shield golioth_esp_at examples/zephyr/certificate_rotation
$ west flash
```

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/certificate_rotation`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/certificate_rotation
$ west flash
```

#### RAK5010 (v2 with BG95)

On your host computer open a terminal window. From the
`golioth-firmware-sdk` folder, type:

```console
$ west build -b rak5010/nrf52840 examples/zephyr/certificate_rotation
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

1. A Client Certificate, located at `/lfs1/credentials/crt.der`
2. A Private Key, located at `/lfs1/credentials/key.der`

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
$ mcumgr --conntype serial --connstring=dev=<path/to/your/device>,baud=115200 fs upload keys/client_certificate.der /lfs1/credentials/crt.der
$ mcumgr --conntype serial --connstring=dev=<path/to/your/device>,baud=115200 fs upload keys/private_key.der /lfs1/credentials/key.der
```

Be sure to replace `<path/to/your/device>` with the appropriate serial
device for your board, typically something like
`/dev/cu.usbmodem0009600837441`.

Finally, re-open a serial connection and restart the logs:

```console
uart:-$ log go
```

### Sample output

The length and filesystem path of the credentials will be logged,
indicating a successful read operation.

```console
*** Booting nRF Connect SDK v3.1.1-e2a97fe2578a ***
*** Using Zephyr OS v4.1.99-ff8f0c579eeb ***
*** Golioth Firmware SDK v0.21.1-30-g44106c036abc ***
[00:00:00.674,652] <dbg> cert_rotation: main: Start certificate rotation sample
[00:00:00.674,682] <inf> golioth_samples: Bringing up network interface
[00:00:00.674,682] <inf> golioth_samples: Waiting to obtain IP address
[00:00:02.007,324] <inf> lte_monitor: Network: Searching
[00:00:02.901,977] <inf> lte_monitor: Network: Registered (home)
[00:00:02.905,426] <inf> cert_rotation: Read 363 bytes from /lfs1/credentials/crt.der
[00:00:02.906,982] <inf> cert_rotation: Read 138 bytes from /lfs1/credentials/key.der
[00:00:02.907,409] <inf> golioth_mbox: Mbox created, bufsize: 1320, num_items: 10, item_size: 120
[00:00:04.498,138] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:00:04.538,757] <inf> cert_rotation: Golioth client connected
[00:00:04.538,787] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:00:04.747,070] <inf> cert_rotation: Received new certificate. Reconnecting...
[00:00:04.747,070] <inf> golioth_coap_client_zephyr: Attempting to stop client
[00:00:04.747,314] <inf> golioth_coap_client_zephyr: Stop request
[00:00:04.747,314] <inf> golioth_coap_client_zephyr: Ending session
[00:00:04.747,344] <inf> cert_rotation: Golioth client disconnected
[00:00:05.848,663] <inf> golioth_mbox: Mbox created, bufsize: 1320, num_items: 10, item_size: 120
[00:00:07.323,120] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:00:07.323,181] <inf> cert_rotation: Reconnected.
[00:00:07.323,181] <inf> cert_rotation: Sending with new certificate
[00:00:07.323,242] <inf> cert_rotation: Golioth client connected
[00:00:07.323,364] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:00:12.323,272] <inf> cert_rotation: Sending with new certificate
[00:00:17.323,364] <inf> cert_rotation: Sending with new certificate
```
