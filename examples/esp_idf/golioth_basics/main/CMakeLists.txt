set(srcs
    "app_main.c"
    "../../common/shell.c"
    "../../common/wifi.c"
    "../../common/nvs.c"
)

set(deps
    "golioth_sdk"
    "console"
    "spi_flash"
    "nvs_flash"
    "json"
    "driver"
    "esp_hw_support"
    "bt"
)

if(CONFIG_GOLIOTH_BLE_SERVICE_ENABLED)
    list(APPEND srcs "../../common/ble.c")
endif()

idf_component_register(
    INCLUDE_DIRS "../../common"
    SRCS "${srcs}"
    PRIV_REQUIRES "${deps}"
)
list(APPEND EXTRA_C_FLAGS_LIST
    -Werror
)
component_compile_options(${EXTRA_C_FLAGS_LIST})
