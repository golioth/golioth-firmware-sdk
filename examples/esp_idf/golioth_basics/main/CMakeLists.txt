set(esp_idf_common "../../common")
set(examples_common "../../../common")

set(includes
    "${esp_idf_common}"
    "${examples_common}"
    "${idf_path}/components/freertos/include/freertos"
)

set(srcs
    "app_main.c"
    "${esp_idf_common}/shell.c"
    "${esp_idf_common}/wifi.c"
    "${esp_idf_common}/nvs.c"
    "${examples_common}/golioth_basics.c"
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
    list(APPEND srcs "${esp_idf_common}/ble.c")
endif()

idf_component_register(
    INCLUDE_DIRS "${includes}"
    SRCS "${srcs}"
    PRIV_REQUIRES "${deps}"
)
list(APPEND EXTRA_C_FLAGS_LIST
    -Werror
)
component_compile_options(${EXTRA_C_FLAGS_LIST})
