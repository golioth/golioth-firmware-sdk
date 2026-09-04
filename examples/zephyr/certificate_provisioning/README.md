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

#### nRF9160 DK

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/certificate_provisioning`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/certificate_provisioning
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
[00:21:37.239,879] <inf> littlefs: littlefs partition at /lfs1
[00:21:37.240,006] <inf> littlefs: LittleFS version 2.11, disk version 2.1
[00:21:37.242,840] <inf> littlefs: FS at w25q512jvfiq@0:0x620000 is 14816 0x1000-byte blocks with 512 cycle
[00:21:37.242,848] <inf> littlefs: partition sizes: rd 16 ; pr 16 ; ca 64 ; la 32
[00:21:37.243,248] <inf> littlefs: Automount /lfs1 succeeded
[00:21:37.243,729] <inf> eth_nxp_enet_mac: Link is down
*** Booting Zephyr OS build v4.3.0 ***
*** Golioth Firmware SDK v0.22.0-81-gd36c63bee2bf ***
[00:21:37.260,559] <dbg> cert_provisioning: main: Start certificate provisioning sample
[00:21:37.260,593] <inf> golioth_samples: Bringing up network interface
[00:21:37.260,612] <inf> golioth_samples: Waiting for link to be up
[00:21:37.739,918] <inf> phy_mc_ksz8081: PHY (2) is entering autonegotiation sequence
[00:21:40.442,887] <inf> eth_nxp_enet_mac: Link is up
[00:21:40.442,924] <inf> phy_mc_ksz8081: PHY 2 is up
[00:21:40.442,940] <inf> phy_mc_ksz8081: PHY (2) Link speed 100 Mb, full duplex

[00:21:40.443,142] <inf> golioth_samples: Starting DHCP to obtain IP address
[00:21:40.443,207] <inf> golioth_samples: Waiting to obtain IP address
[00:21:49.446,980] <inf> net_dhcpv4: Received: 192.168.1.233
[00:21:49.447,921] <inf> cert_provisioning: Read 380 bytes from /lfs1/credentials/crt.der
[00:21:49.448,342] <inf> cert_provisioning: Read 138 bytes from /lfs1/credentials/key.der
[00:21:49.448,592] <inf> golioth_mbox: Mbox created, bufsize: 1232, num_items: 10, item_size: 112
[00:21:51.031,024] <inf> golioth_coap_client_zephyr: Golioth CoAP client connected
[00:21:51.031,180] <inf> cert_provisioning: Sending hello! 0
[00:21:51.031,292] <inf> cert_provisioning: Golioth client connected
[00:21:51.031,472] <inf> golioth_coap_client_zephyr: Entering CoAP I/O loop
[00:21:56.031,406] <inf> cert_provisioning: Sending hello! 1
[00:22:01.031,599] <inf> cert_provisioning: Sending hello! 2
```
