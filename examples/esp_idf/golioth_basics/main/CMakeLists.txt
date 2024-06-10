set(esp_idf_common "../../common")
set(examples_common "../../../common")

set(includes
    "${esp_idf_common}"
    "${examples_common}"
)

set(srcs
    "app_main.c"
    "${esp_idf_common}/shell.c"
    "${esp_idf_common}/wifi.c"
    "${esp_idf_common}/nvs.c"
    "${examples_common}/golioth_basics.c"
    "${esp_idf_common}/sample_credentials.c"
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

idf_component_register(
    INCLUDE_DIRS "${includes}"
    SRCS "${srcs}"
    PRIV_REQUIRES "${deps}"
)
list(APPEND EXTRA_C_FLAGS_LIST
    -Werror
)
component_compile_options(${EXTRA_C_FLAGS_LIST})
