# Golioth ESP-IDF Examples

This repo contains a set of examples intended to build
and run in the latest release of esp-idf
(currently [5.1.1](https://github.com/espressif/esp-idf/releases/tag/v5.1.1)).

### Install esp-idf

Install version 5.1.1 of esp-idf using the
[installation directions from Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation).
This is the version of esp-idf this SDK is tested against.

For Linux users, you can install esp-idf with these commands:

```sh
sudo apt-get install git wget flex bison gperf python3 python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git -b v5.1.1
cd esp-idf
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

### USB serial configuration for Adafruit ESP32-S2/S3 Feather Boards

If you're using an Adafruit Feather board with a USB-C connector,
you will need to set `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` to `y` via
`sdkconfig` or `idf.py menuconfig`. This is required in order to use
the command-line shell that is integrated with the ESP-IDF examples.
