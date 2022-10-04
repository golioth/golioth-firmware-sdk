# Golioth Firmware SDK

A software development kit for connecting Espressif devices to the
[Golioth](https://golioth.io) IoT cloud.

SDK source: https://github.com/golioth/golioth-firmware-sdk

API documentation: https://firmware-sdk-docs.golioth.io/

## Getting Started

### Hello, Golioth!

Here is a minimal example that demonstrates how to connect to
[Golioth cloud](https://docs.golioth.io/cloud) and send the message "Hello, Golioth!":

```c
#include "wifi.h"
#include "golioth.h"

void app_main(void) {
    const char* wifi_ssid = "SSID";
    const char* wifi_password = "Password";
    const char* golioth_psk_id = "device@project";
    const char* golioth_psk = "supersecret";

    wifi_init(wifi_ssid, wifi_password);
    wifi_wait_for_connected();

    golioth_client_config_t config = {
        .credentials = {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
            .psk = {
                    .psk_id = golioth_psk_id,
                    .psk_id_len = strlen(golioth_psk_id),
                    .psk = golioth_psk,
                    .psk_len = strlen(golioth_psk),
            }
        }
    };

    golioth_client_t client = golioth_client_create(&config);
    golioth_log_info_sync(client, "app_main", "Hello, Golioth!", 5.0);
}
```

### Cloning this repo

This repo uses git submodules, so you will need to clone with the `--recursive` option:

```sh
git clone --recursive https://github.com/golioth/golioth-firmware-sdk.git
```

Or, if you've already cloned but forgot the `--recursive`, you can update and
initialize submodules with this command:

```sh
cd golioth-firmware-sdk
git submodule update --init --recursive
```

### Trying the SDK examples

The `examples` directory contains example apps which you can build and run.
It is organized by platform (e.g. ESP-IDF, ModusToolbox, etc),
so navigate to a specific platform directory and check out the README for further
instructions to build and run the examples.

The `golioth_basics` example (`examples/<platform>/golioth_basics`) is recommended
as a starting point, to learn how to connect to Golioth and use services like
[LightDB state](https://docs.golioth.io/cloud/services/lightdb),
[LightDB Stream](https://docs.golioth.io/cloud/services/lightdb-stream),
[Logging](https://docs.golioth.io/cloud/services/logging),
and [OTA](https://docs.golioth.io/cloud/services/ota).

### Additional Documentation

The `docs` folder contains additional documentation, such as a platform
integration guide and a platform porting guide.

## Verified Devices

The following table lists the different hardware configurations we test the SDK against,
and when it was last tested.

The test itself covers most major functionality of the SDK, including connecting
to the Golioth server, interacting with LightDB State and Stream, and performing
OTA firmware updates.

The test procedure is (e.g. for ESP-IDF):

```
cd examples/esp_idf/test
idf.py build
idf.py flash
python verify.py /dev/ttyUSB0
```

The `verify.py` script will return 0 on success (all tests pass), and non-zero otherwise.

| Board               | Platform             | Module           | Last Tested Commit    |
| ---                 | ---                  | ---              | ---                   |
| ESP32-S3-DevKitC-1  | ESP-IDF (v4.4.1)     | ESP32-S3-WROOM-1 | Every commit, CI/CD   |
| ESP32-DevKitC-32E   | ESP-IDF (v4.4.1)     | ESP32-WROOM-32E  | v0.3.0 (Oct 4, 2022)  |
| ESP32-C3-DevKitM-1  | ESP-IDF (v4.4.1)     | ESP32-C3-MINI-1  | v0.3.0 (Oct 4, 2022)  |
| CY8CPROTO-062-4343W | ModusToolbox (2.4.0) | PSoC® 6 CYW4343W | v0.3.0 (Oct 4, 2022)  |
