# Integrating the SDK

This page has recommendations for integrating the Golioth Firmware SDK
into existing/new application on various platforms.

## Integrating with ESP-IDF Application

The recommended way to integrate this SDK into an external application is to add it as a
git submodule. For example:

```
cd your_project
git submodule add https://github.com/golioth/golioth-firmware-sdk.git third_party/golioth-firmware-sdk
git submodule update --init --recursive
```

You should not need to modify the files in the submodule at all.

The SDK is packaged as an esp-idf component, so you will need to add this line
to your project's top-level CMakeLists.txt, which will allow esp-idf to find the SDK:

```
list(APPEND EXTRA_COMPONENT_DIRS third_party/golioth-firmware-sdk/port/esp_idf/components)
```

In your `main/CMakeLists.txt`, you can add `golioth_sdk` to your `REQUIRES` or `PRIV_REQUIRES`
list, for example:

```
idf_component_register(
    INCLUDE_DIRS
        ...
    SRCS
        ...
    PRIV_REQUIRES
        "golioth_sdk"
        ...
)
```

Next, make sure your `sdkconfig.defaults` file has the following:

```
CONFIG_MBEDTLS_SSL_PROTO_DTLS=y
CONFIG_MBEDTLS_PSK_MODES=y
CONFIG_MBEDTLS_KEY_EXCHANGE_PSK=y
CONFIG_LWIP_NETBUF_RECVINFO=y
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y

# Default sdkconfig parameters to use the OTA
# partition table layout, with a 4MB flash size
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_TWO_OTA=y

# If the new app firmware does not cancel the rollback,
# then this will ensure that rollback happens automatically
# if/when the device is next rebooted.
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

Optionally, you can copy the files from `examples/esp_idf/common` into your project and modify
as needed. These files can be used to help with basic system initialization of
NVS, WiFi, and shell.

A typical project structure will look something like this:

```
your_project
├── CMakeLists.txt
├── main
│   ├── app_main.c
│   ├── CMakeLists.txt
├── sdkconfig.defaults
└── third_party
    └── golioth-firmware-sdk (submodule)
```

A complete example of using the Golioth SDK in an external project can be found here:

https://github.com/golioth/golioth-esp-idf-external-app.git

## Integrating with Linux Application

The recommended way to integrate this SDK into an external application is to add it as a
git submodule. For example:

```
cd your_project
git submodule add https://github.com/golioth/golioth-firmware-sdk.git third_party/golioth-firmware-sdk
git submodule update --init --recursive
```

You should not need to modify the files in the submodule at all.

The SDK is packaged as a cmake library, so you will need to add this line
to your project's CMakeLists.txt:

```
add_subdirectory(third_party/golioth-firmware-sdk/port/linux/golioth_sdk)
```

And you will need to link the `golioth_sdk` library into your executable:

```
target_link_libraries(my_exe_target golioth_sdk)
```

A typical project structure will look something like this:

```
your_project
├── CMakeLists.txt
├── main.c
└── third_party
    └── golioth-firmware-sdk (submodule)
```

## Integrating with Modus Toolbox Application

The following instructions will show the general process for integrating the Golioth Firmware SDK
into a ModusToolbox project on [a PSoC 6 development board](https://www.infineon.com/cms/en/product/evaluation-boards/cy8cproto-062-4343w/).

For reference, a fully working example is provided here: https://github.com/golioth/mtb-example-golioth.
It's recommended to start from this example, as it is easier than trying to add
Golioth to an existing project. The difficulty lies in needing to have a second bootloader project
running MCUboot which can perform firmware updates, which requires additional python scripts and
flash map configuration (which is provided in the Golioth example). But if you really want to add
Golioth to an existing application, you can follow the general steps in
this guide, and then "pattern match" against the `mtb-example-golioth` project linked above to
have a fully integrated project.

It is assumed you've already installed
[ModusToolbox](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)
release 3.0 from Infineon.

1. Add required dependencies as `.mtb` files in the `deps` folder

```
echo 'https://github.com/golioth/golioth-firmware-sdk#v0.5.0#$$ASSET_REPO$$/golioth-firmware-sdk/v0.5.0' > deps/golioth-firmware-sdk.mtb
echo 'https://github.com/obgm/libcoap#bbf098a72adeb8b5f9508e5a9f3a409f9a298d7a#$$ASSET_REPO$$/libcoap/bbf098a72adeb8b5f9508e5a9f3a409f9a298d7a' > deps/libcoap.mtb
echo 'https://github.com/DaveGamble/cJSON#v1.7.15#$$ASSET_REPO$$/cJSON/v1.7.15' > deps/cJSON.mtb
echo 'https://github.com/mcu-tools/mcuboot#v1.8.1-cypress#$$ASSET_REPO$$/mcuboot/v1.8.1-cypress' > deps/mcuboot.mtb
```

After doing this, update libraries:

```
make getlibs
```

2. Copy .cyignore and mbedtls_user_config.h from mtb-example-golioth into your project.

The mbedtls configuration is to enable support for DTLS.

```
wget https://raw.githubusercontent.com/golioth/mtb-example-golioth/main/golioth_app/mbedtls_user_config.h
wget https://raw.githubusercontent.com/golioth/mtb-example-golioth/main/golioth_app/.cyignore
```

3. Update your project's Makefile

You'll want to add lines similar to the following to your project:

```
MBEDTLSFLAGS = MBEDTLS_USER_CONFIG_FILE='"mbedtls_user_config.h"'
GOLIOTHSDK_PATH=$(SEARCH_golioth-firmware-sdk)
INCLUDES+=$(GOLIOTHSDK_PATH)/external/heatshrink
SOURCES+=$(GOLIOTHSDK_PATH)/external/heatshrink/heatshrink_decoder.c
```

4. Add Golioth startup code

To connect to Golioth servers using PSK authentication, you'll need code like the following
in your application, after connecting to WiFi:

```c
#include "golioth.h"

...

    const char* psk_id = "device@project";
    const char* psk = "supersecret";

    golioth_client_config_t config = {
            .credentials = {
                    .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
                    .psk = {
                            .psk_id = psk_id,
                            .psk_id_len = strlen(psk_id),
                            .psk = psk,
                            .psk_len = strlen(psk),
                    }}};
    golioth_client_t client = golioth_client_create(&config);

...
```

5. Create a bootloader project

To use the OTA firmware update feature, you will need two ModusToolbox projects: one for the
bootloader and another for the application. The bootloader project runs MCUboot and handles
updating of application firmware images in flash.

It's recommend to copy the
[bootloader_cm0p project from the Golioth example](https://github.com/golioth/mtb-example-golioth/tree/main/bootloader_cm0p).

6. Compiling and running

At this point, you've got all the major integration pieces needed, but it may not compile yet. If
you run into errors, pattern match against the [Golioth example](https://github.com/golioth/mtb-example-golioth),
and you should eventually get a working application.

If you encounter an error you don't know how to resolve, feel free to reach out on [Discord](https://discord.com/invite/UUqsDaG7kP)
or post on our [forum](https://forum.golioth.io/).
