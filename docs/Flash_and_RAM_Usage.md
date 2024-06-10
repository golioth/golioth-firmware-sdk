# Flash and RAM Usage

This page documents the flash, static RAM, and dynamic RAM usage of the Golioth SDK.

All measurements were taken by building the `golioth_basics` example in ESP-IDF v4.4.2, targeting
[ESP32-DevkitC](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview).

In general, the flash and RAM usage of the Golioth SDK will be similar on other platforms.

## Flash

The flash usage can be obtained with the `idf.py size-components` command.

For the `golioth_basics` example, this is the output for `libgolioth_sdk.a` (in bytes):

```
Flash .text & .rodata & .rodata_noload & .appdesc flash_total
      19394     13073                0          0       32471
```

## Static RAM

The static DRAM usage can be obtained with the `idf.py size-components` command.

For the `golioth_basics` example, this is the output for `libgolioth_sdk.a` (in bytes):

```
DRAM .data .rtc.data DRAM .bss IRAM0 .text & 0.vectors ram_st_total
         4         0      1637           0           0         1641
```

## Dynamic RAM

Measuring dynamic memory usage can be difficult, because it's not easy to track how much code is
allocated on a per-module basis, and because there are direct allocations (e.g. calling `malloc`)
and indirect allocations (calling another library's API which allocates memory).

In order to work around these difficulties, we measure the dynamic memory usage across four
different examples using the function `heap_caps_print_heap_info()` function:

1. [ESP-IDF hello_world](https://github.com/espressif/esp-idf/tree/master/examples/get-started/hello_world).
   Includes only the base platform, but does not include WiFi, CoAP, or Golioth.
2. [ESP-IDF wifi station](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station).
   Includes the base platform and WiFi, but does not include CoAP or Golioth.
3. [ESP-IDF coap_client](https://github.com/espressif/esp-idf/tree/master/examples/protocols/coap_client).
   Includes the base platform, WiFi, DTLS, and CoAP, but does not include Golioth.
4. [Golioth golioth_basics](https://github.com/golioth/golioth-firmware-sdk/tree/main/examples/esp_idf/golioth_basics).
   Includes the base platform, WiFi, DTLS, CoAP, and Golioth.

By measuring each example, you can get a rough idea of how much dynamic memory usage is happening in
each of the four layers (base platform, WiFi, DTLS+CoAP, Golioth).

| Example     | Example Heap Usage (bytes) |
| ---         | --- |
| 1 (B)       | 18788 |
| 2 (B+W)     | 62808 |
| 3 (B+W+C)   | 97880 |
| 4 (B+W+C+G) | 116640 |

Where B is "base platform", W is "WiFi", C is "DTLS+CoAP", and G is "Golioth".

To compute estimated heap usage of each layer, compute the difference between each example and the
one before it:

| Layer | Estimated Heap Usage |
| --- | --- |
| Base | 18788 |
| WiFi | 44020 |
| DTLS+CoAP | 35072 |
| Golioth | 18760 |
