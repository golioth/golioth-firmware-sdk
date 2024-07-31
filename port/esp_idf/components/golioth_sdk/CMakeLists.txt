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
        "${sdk_root}/include"
        "${sdk_port}/esp_idf"
    PRIV_INCLUDE_DIRS
        "${freertos_include_dir}"
    REQUIRES
        "nvs_flash"
        "zcbor"
    PRIV_REQUIRES
        "coap"
        "bsdiff"
        "lwip"
        "mbedtls"
        "app_update"
        "esp_timer"
    SRCS
        "${sdk_port}/freertos/golioth_sys_freertos.c"
        "${sdk_port}/esp_idf/fw_update_esp_idf.c"
        "${sdk_src}/golioth_status.c"
        "${sdk_src}/coap_client.c"
        "${sdk_src}/coap_client_libcoap.c"
        "${sdk_src}/log.c"
        "${sdk_src}/lightdb_state.c"
        "${sdk_src}/stream.c"
        "${sdk_src}/rpc.c"
        "${sdk_src}/ota.c"
        "${sdk_src}/payload_utils.c"
        "${sdk_src}/fw_update.c"
        "${sdk_src}/settings.c"
        "${sdk_src}/golioth_debug.c"
        "${sdk_src}/ringbuf.c"
        "${sdk_src}/event_group.c"
        "${sdk_src}/mbox.c"
        "${sdk_src}/fw_block_processor.c"
        "${sdk_src}/coap_blockwise.c"
        "${sdk_src}/zcbor_utils.c"
    EMBED_TXTFILES
        "${sdk_src}/isrgrootx1_x2.pem"
)

# Enable errors and warnings that ESP-IDF disables by default
list(APPEND EXTRA_C_FLAGS_LIST
    -Werror=unused-function
    -Werror=unused-variable
    -Werror=unused-but-set-variable
    -Werror=deprecated-declarations
)

if(${IDF_VERSION_MAJOR} GREATER_EQUAL "5")
    list(APPEND EXTRA_C_FLAGS_LIST -Wenum-conversion -Werror=enum-conversion)
endif()

component_compile_options(${EXTRA_C_FLAGS_LIST})

target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
