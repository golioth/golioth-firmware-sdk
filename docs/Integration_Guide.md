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

## Integrating with Modus Toolbox Application

TODO

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
