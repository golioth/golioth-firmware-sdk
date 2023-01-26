# Golioth Zephyr Examples

This directory contains a set of examples intended to build and run with the latest release of
Zephyr.

### Install Zephyr SDK

Follow [Zephyr Geting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/).

### Setup west project

``` sh
cd ~/golioth-workspace
west init -m https://github.com/golioth/golioth-firmware-sdk.git --mf west-zephyr.yml
west update
(cd modules/lib/golioth-firmware-sdk && git submodule update --init --recursive)
```

### Configure PSK and PSK-ID credentials

Add credentials to ``qemu_x86
modules/lib/golioth-firmware-sdk/examples/zephyr/golioth_basics/prj.conf`` file:

``` cfg
CONFIG_GOLIOTH_PSK_ID="my-psk-id@my-project"
CONFIG_GOLIOTH_PSK="my-psk"
```

### Build and run sample project

``` sh
west build -b qemu_x86 modules/lib/golioth-firmware-sdk/examples/zephyr/golioth_basics
west build -t run
```
