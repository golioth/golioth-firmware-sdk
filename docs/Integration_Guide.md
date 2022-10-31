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
