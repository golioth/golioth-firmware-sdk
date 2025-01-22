# Golioth Hello sample for nRF91 with offloaded DTLS sockets

## Overview

This sample application demonstrates how to connect with Golioth and
publish simple Hello messages, running on nRF91 platform with DTLS
offloaded sockets.

## Requirements

* Golioth credentials
* Network connectivity

## Building and Running

On your host computer open a terminal window, locate the source code of
this sample application (i.e., `examples/zephyr/hello_nrf91_offloaded`) and type:

```console
$ west build -b nrf9160dk/nrf9160/ns examples/zephyr/hello_nrf91_offloaded
$ west flash
```

### Authentication specific configuration

#### PSK based auth

PSK credentials can be provisioned using nRF91 specific AT commands.
Those are enabled only in specific application configurations.

##### Using [AT Client](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.6.0/nrf/samples/cellular/at_client/README.html)

Build and flash [AT
Client](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.6.0/nrf/samples/cellular/at_client/README.html)
sample:

```sh
$ west build nrf/samples/cellular/at_client
$ west flash
```

Execute following AT commands over serial to delete old credentials:

``` sh
AT%CMNG=3,515765868,4
AT%CMNG=3,515765868,3
```

Execute following AT commands over serial to provision new credentials:

``` sh
AT%CMNG=0,515765868,4,my-psk-id
AT%CMNG=0,515765868,3,my-psk-encoded-as-hexstring
```

Note: the PSK *must* be encoded as hexadecimal values. The Golioth
console displays PSKs in ASCII encoding. One way to convert your
ASCII-encoded PSK is to use the following Linux command:

```
$ echo "my-golioth-psk" | tr -d '\n' | xxd -ps -c 200
6d792d676f6c696f74682d70736b
```

##### Using `nrf_modem_at` application specific shell command

This application comes with `nrf_modem_at` Zephyr shell command. It can
be used to execute AT commands. First, put nRF91 cellular modem into
offline mode, so it is possible to provision credentials:

``` sh
uart:~$ nrf_modem_at AT+CFUN=4
```

Delete old credentials:

``` sh
uart:~$ nrf_modem_at AT%CMNG=3,515765868,4
uart:~$ nrf_modem_at AT%CMNG=3,515765868,3
```

Provision new credentials:

``` sh
uart:~$ nrf_modem_at AT%CMNG=0,515765868,4,my-psk-id
uart:~$ nrf_modem_at AT%CMNG=0,515765868,3,my-psk-encoded-as-hexstring
```

Now reboot application to use newly provisioned PSK-ID and PSK:

``` sh
uart:-$ kernel reboot cold
```

### Sample output

This is the output from the serial console:

```console
*** Booting Zephyr OS build zephyr-v3.4.0-553-g40d224022608 ***
[00:00:00.020,000] <inf> net_config: Initializing network
[00:00:00.020,000] <inf> net_config: IPv4 address: 192.0.2.1
[00:00:00.020,000] <dbg> hello_zephyr: main: start hello sample
[00:00:00.020,000] <inf> golioth_samples: Waiting for interface to be up
[00:00:00.020,000] <inf> golioth_mbox: Mbox created, bufsize: 1100, num_items: 10, item_size: 100
[00:00:00.070,000] <inf> golioth_coap_client: Start CoAP session with host: coaps://coap.golioth.io
[00:00:00.070,000] <inf> golioth_coap_client: Session PSK-ID: your-device-id@your-golioth-project
[00:00:00.070,000] <inf> golioth_coap_client: Entering CoAP I/O loop
[00:00:01.260,000] <inf> golioth_coap_client: Golioth CoAP client connected
[00:00:01.260,000] <inf> hello_zephyr: Sending hello! 0
[00:00:01.260,000] <inf> hello_zephyr: Golioth client connected
[00:00:06.270,000] <inf> hello_zephyr: Sending hello! 1
[00:00:11.280,000] <inf> hello_zephyr: Sending hello! 2
```
