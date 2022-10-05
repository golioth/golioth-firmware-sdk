# Golioth ESP-IDF Examples

This repo contains a set of examples intended to build
and run in the latest release of esp-idf
(currently [4.4.2](https://github.com/espressif/esp-idf/releases/tag/v4.4.2)).

### Install esp-idf

Install version 4.4.2 of esp-idf using the
[installation directions from Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation).
This is the version of esp-idf this SDK is tested against.

For Linux users, you can install esp-idf with these commands:

```sh
sudo apt-get install git wget flex bison gperf python3 python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v4.4.2
git submodule update --init --recursive
./install.sh all
```

### Using VSCode esp-idf extension

If you are using the VScode esp-idf extension, you should be able to load these examples
directly into your workspace and build/flash/monitor.

Otherwise, follow the command-line instructions below.

### Command-line

First, setup the environment. This step assumes you've installed esp-idf to `~/esp/esp-idf`

```sh
source ~/esp/esp-idf/export.sh
```

Next, `cd` to one of the examples, where you can build/flash/monitor:

```
cd examples/golioth_basics
idf.py build
idf.py flash
idf.py monitor
```
