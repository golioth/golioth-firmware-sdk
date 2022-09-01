# TODO - Remove esp_libcoap dependency if/when upstream esp-idf updates the
# libcoap component to a more recent commit
set(libcoap_dir third_party/esp_libcoap)
set(libcoap_include_dirs
    ${libcoap_dir}/port/include
    ${libcoap_dir}/libcoap/include)
set(libcoap_srcs
    "${libcoap_dir}/libcoap/src/address.c"
    "${libcoap_dir}/libcoap/src/async.c"
    "${libcoap_dir}/libcoap/src/block.c"
    "${libcoap_dir}/libcoap/src/coap_asn1.c"
    "${libcoap_dir}/libcoap/src/coap_cache.c"
    "${libcoap_dir}/libcoap/src/coap_debug.c"
    "${libcoap_dir}/libcoap/src/coap_event.c"
    "${libcoap_dir}/libcoap/src/coap_hashkey.c"
    "${libcoap_dir}/libcoap/src/coap_io.c"
    "${libcoap_dir}/libcoap/src/coap_mbedtls.c"
    "${libcoap_dir}/libcoap/src/coap_notls.c"
    "${libcoap_dir}/libcoap/src/coap_prng.c"
    "${libcoap_dir}/libcoap/src/coap_session.c"
    "${libcoap_dir}/libcoap/src/coap_tcp.c"
    "${libcoap_dir}/libcoap/src/coap_time.c"
    "${libcoap_dir}/libcoap/src/encode.c"
    "${libcoap_dir}/libcoap/src/mem.c"
    "${libcoap_dir}/libcoap/src/net.c"
    "${libcoap_dir}/libcoap/src/option.c"
    "${libcoap_dir}/libcoap/src/pdu.c"
    "${libcoap_dir}/libcoap/src/resource.c"
    "${libcoap_dir}/libcoap/src/str.c"
    "${libcoap_dir}/libcoap/src/subscribe.c"
    "${libcoap_dir}/libcoap/src/uri.c")

idf_component_register(
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "${libcoap_include_dirs}"
        "priv_include"
    PRIV_REQUIRES
        "json"
        "lwip"
        "mbedtls"
        "app_update"
        "esp_timer"
        "nvs_flash"
    SRCS
        "${libcoap_srcs}"
        "golioth_status.c"
        "golioth_coap_client.c"
        "golioth_log.c"
        "golioth_lightdb.c"
        "golioth_rpc.c"
        "golioth_ota.c"
        "golioth_time.c"
        "golioth_fw_update.c"
        "golioth_statistics.c"
        "golioth_settings.c")

list(APPEND EXTRA_C_FLAGS_LIST -Werror)
component_compile_options(${EXTRA_C_FLAGS_LIST})

target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
