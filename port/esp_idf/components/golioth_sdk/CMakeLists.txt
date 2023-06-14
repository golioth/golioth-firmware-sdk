set(sdk_root ../../../..)
set(sdk_src ${sdk_root}/src)
set(sdk_port ${sdk_root}/port)

if(${IDF_VERSION_MAJOR} EQUAL "5")
    set(freertos_include_dir "${idf_path}/components/freertos/FreeRTOS-Kernel/include/freertos")
else()
    set(freertos_include_dir "${idf_path}/components/freertos/include/freertos")
endif()

idf_component_register(
    INCLUDE_DIRS
        "${sdk_src}/include"
        "${sdk_port}/esp_idf"
    PRIV_INCLUDE_DIRS
        "${sdk_src}/priv_include"
        "${freertos_include_dir}"
    REQUIRES
        "json"
        "nvs_flash"
        "zcbor"
    PRIV_REQUIRES
        "coap"
        "heatshrink"
        "miniz"
        "bsdiff"
        "lwip"
        "mbedtls"
        "app_update"
        "esp_timer"
    SRCS
        "${sdk_port}/freertos//golioth_sys_freertos.c"
        "${sdk_port}/esp_idf/fw_update_esp_idf.c"
        "${sdk_port}/esp_idf/serial_tap_esp_idf.c"
        "${sdk_src}/golioth_status.c"
        "${sdk_src}/golioth_coap_client.c"
        "${sdk_src}/golioth_log.c"
        "${sdk_src}/golioth_lightdb.c"
        "${sdk_src}/golioth_rpc.c"
        "${sdk_src}/golioth_ota.c"
        "${sdk_src}/golioth_time.c"
        "${sdk_src}/golioth_fw_update.c"
        "${sdk_src}/golioth_statistics.c"
        "${sdk_src}/golioth_settings.c"
        "${sdk_src}/golioth_remote_shell.c"
        "${sdk_src}/golioth_debug.c"
        "${sdk_src}/golioth_ringbuf.c"
        "${sdk_src}/golioth_event_group.c"
        "${sdk_src}/golioth_mbox.c"
        "${sdk_src}/golioth_fw_block_processor.c"
        "${sdk_src}/zcbor_utils.c"
)

component_compile_options(${EXTRA_C_FLAGS_LIST})

target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=uart_hal_write_txfifo -u __wrap_uart_hal_write_txfifo")
