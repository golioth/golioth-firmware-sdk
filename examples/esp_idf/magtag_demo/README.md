## Environment Setup

```sh
source ~/esp/esp-idf/export.sh
idf.py set-target esp32s2
```

## Build

```sh
cd examples/magtag_demo
idf.py build
```

## Known issues

Newer versions of the ESP-IDF may fail at build time with the following message:

```sh
#warning "The legacy RMT driver is deprecated, please use driver/rmt_tx.h and/or driver/rmt_rx.h" [-Werror=cpp]
```

To successfully build the project, suppress the warning by adding this line to
`skdconfig.defaults` or using menuconfig:

```sh
CONFIG_RMT_SUPPRESS_DEPRECATE_WARN=y
```

After making this change, remove the `sdkconfig` file and rebuild the project.

## Flash

On the magtag board, put the ESP32 into bootloader mode so it can
be flashed:

1. press and hold the `Boot0` button
2. press the `Reset` button (while `Boot0` is held)

Now that the board is in bootloader mode, you can flash:

```sh
idf.py -p /dev/ttyACM0 flash
```

Finally, press the `Reset` button once more to reboot
the board into the new firmware.

## Serial monitoring

You can use the ESP-IDF monitor feature to see the serial output of the MagTag.

```sh
idp.py -p /dev/ttyACM0 monitor
```

Exit the monitor by typing `CTRL-]`.

## Configure

You will need to set the WiFi and Golioth credentials so that yor device can
connect. Access the serial shell using the serial montior mentioned above. If
you do not see the `esp32>` prompt after connecting, try pressing enter a few
times, or simply press the reset button on the board.

From the command prompt, use the `settings set <key> <value>` syntax to set the
following keys:

* `wifi/ssid`
* `wifi/psk`
* `golioth/psk-id`
* `golioth/psk`

Reset the board to use the newly saved credentials. For example:

```sh
I (420) epaper: ePaper Init and Clear
I (1220) epaper: Show Golioth logo

Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the console environment.
esp32> W (2440) magtag_demo: WiFi and golioth credentials are not set
W (8081) magtag_demo: Use the shell settings commands to set them, then restart
esp32>
esp32> settings set wifi/ssid your_wifi_AP
Setting wifi/ssid saved
esp32> settings set wifi/psk your_wifi_password
Setting wifi/psk saved
esp32> settings set golioth/psk-id demo-psk-id@demo-project-name
Setting golioth/psk-id saved
esp32> settings set golioth/psk strong-password
Setting golioth/psk saved
esp32> reset
```

