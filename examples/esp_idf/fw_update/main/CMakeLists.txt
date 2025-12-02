set(esp_idf_common "../../common")
set(examples_common "../../../common")

set(includes
    "${esp_idf_common}"
    "${examples_common}"
)

set(freertos_include "${idf_path}/components/freertos/FreeRTOS-Kernel/include/freertos")

set(srcs
    "app_main.c"
    "fw_update.c"
    "${esp_idf_common}/shell.c"
    "${esp_idf_common}/wifi.c"
    "${esp_idf_common}/nvs.c"
    "${esp_idf_common}/sample_credentials.c"
)

set(deps
    "golioth_sdk"
    "app_update"
    "console"
    "spi_flash"
    "nvs_flash"
    "json"
    "driver"
    "esp_hw_support"
    "esp_wifi"
)

idf_component_register(
    INCLUDE_DIRS "${includes}"
    PRIV_INCLUDE_DIRS "${freertos_include}"
    SRCS "${srcs}"
    PRIV_REQUIRES "${deps}"
)
list(APPEND EXTRA_C_FLAGS_LIST
    -Werror
)
component_compile_options(${EXTRA_C_FLAGS_LIST})

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/merged.bin
                   COMMAND esptool.py --chip ${IDF_TARGET} merge_bin -o ${CMAKE_BINARY_DIR}/merged.bin @flash_args
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                   DEPENDS app)

add_custom_target(mergebin ALL DEPENDS ${CMAKE_BINARY_DIR}/merged.bin)
