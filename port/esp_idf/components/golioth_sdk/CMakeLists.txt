set(sdk_root ../../../..)
set(sdk_src ${sdk_root}/src)
set(sdk_port ${sdk_root}/port)
set(libcoap_dir ${sdk_root}/external/libcoap)

set(libcoap_srcs
    "${libcoap_dir}/src/coap_address.c"
    "${libcoap_dir}/src/coap_async.c"
    "${libcoap_dir}/src/block.c"
    "${libcoap_dir}/src/coap_asn1.c"
    "${libcoap_dir}/src/coap_cache.c"
    "${libcoap_dir}/src/coap_debug.c"
    "${libcoap_dir}/src/coap_event.c"
    "${libcoap_dir}/src/coap_hashkey.c"
    "${libcoap_dir}/src/coap_io.c"
    "${libcoap_dir}/src/coap_mbedtls.c"
    "${libcoap_dir}/src/coap_notls.c"
    "${libcoap_dir}/src/coap_prng.c"
    "${libcoap_dir}/src/coap_session.c"
    "${libcoap_dir}/src/coap_tcp.c"
    "${libcoap_dir}/src/coap_time.c"
    "${libcoap_dir}/src/encode.c"
    "${libcoap_dir}/src/mem.c"
    "${libcoap_dir}/src/net.c"
    "${libcoap_dir}/src/coap_option.c"
    "${libcoap_dir}/src/pdu.c"
    "${libcoap_dir}/src/resource.c"
    "${libcoap_dir}/src/str.c"
    "${libcoap_dir}/src/coap_subscribe.c"
    "${libcoap_dir}/src/uri.c")

idf_component_register(
    INCLUDE_DIRS
        "${sdk_src}/include"
        "${sdk_port}/esp_idf"
    PRIV_INCLUDE_DIRS
        "${sdk_src}/priv_include"
        "${sdk_port}/esp_idf/libcoap/include"
        "${libcoap_dir}/include"
        "${idf_path}/components/freertos/include/freertos"
    PRIV_REQUIRES
        "json"
        "lwip"
        "mbedtls"
        "app_update"
        "esp_timer"
        "nvs_flash"
    SRCS
        "${libcoap_srcs}"
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
)

component_compile_options(${EXTRA_C_FLAGS_LIST})

target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=uart_hal_write_txfifo -u __wrap_uart_hal_write_txfifo")
