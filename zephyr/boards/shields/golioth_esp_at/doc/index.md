# Golioth ESP-AT WiFi Shield

## Overview

This shield is a definition of how to connect ESP32-WROOM-32 module based board
(such as ESP32 DevkitC rev. 4) running WiFi stack to nRF52840 DK.

## Pins Assignment

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

## Requirements

This shield can only be used with `nrf52840dk/nrf52840` board.

## Flashing ESP-AT firmware

See [AT Binary
Lists](https://docs.espressif.com/projects/esp-at/en/latest/AT_Binary_Lists/index.html)
for links to ESP-AT binaries and details on how to flash ESP-AT image on
ESP chip. Flash ESP chip with following command:

```console
esptool.py write_flash --verify 0x0 PATH_TO_ESP_AT/factory/factory_WROOM-32.bin
```

## Programming

Set `--shield golioth_esp_at` when you invoke `west build`.
For example:

```console
$ west build -b nrf52840dk/nrf52840 --shield golioth_esp_at examples/zephyr/hello
```
