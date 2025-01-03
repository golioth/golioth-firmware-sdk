idf_component_register(
    INCLUDE_DIRS
        "../../common"
    SRCS
        "app_main.c"
        "../../common/shell.c"
        "../../common/wifi.c"
        "../../common/nvs.c"
    PRIV_REQUIRES
        "golioth_sdk"
        "console"
        "spi_flash"
        "nvs_flash"
        "json"
        "driver"
        "esp_hw_support"
        "esp_wifi"
    EMBED_TXTFILES
        certs/client.crt.pem
        certs/client.key.pem
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
