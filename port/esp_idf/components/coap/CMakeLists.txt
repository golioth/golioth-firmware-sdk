# Note: this overrides ESP-IDF's coap component

set(sdk_root ../../../..)
set(sdk_port ${sdk_root}/port)
set(libcoap_dir ${sdk_root}/external/libcoap)

set(libcoap_srcs
    "${libcoap_dir}/src/coap_address.c"
    "${libcoap_dir}/src/coap_asn1.c"
    "${libcoap_dir}/src/coap_async.c"
    "${libcoap_dir}/src/coap_block.c"
    "${libcoap_dir}/src/coap_cache.c"
    "${libcoap_dir}/src/coap_debug.c"
    "${libcoap_dir}/src/coap_dtls.c"
    "${libcoap_dir}/src/coap_encode.c"
    "${libcoap_dir}/src/coap_event.c"
    "${libcoap_dir}/src/coap_hashkey.c"
    "${libcoap_dir}/src/coap_io.c"
    "${libcoap_dir}/src/coap_layers.c"
    "${libcoap_dir}/src/coap_mbedtls.c"
    "${libcoap_dir}/src/coap_mem.c"
    "${libcoap_dir}/src/coap_net.c"
    "${libcoap_dir}/src/coap_netif.c"
    "${libcoap_dir}/src/coap_option.c"
    "${libcoap_dir}/src/coap_pdu.c"
    "${libcoap_dir}/src/coap_prng.c"
    "${libcoap_dir}/src/coap_resource.c"
    "${libcoap_dir}/src/coap_session.c"
    "${libcoap_dir}/src/coap_str.c"
    "${libcoap_dir}/src/coap_subscribe.c"
    "${libcoap_dir}/src/coap_tcp.c"
    "${libcoap_dir}/src/coap_time.c"
    "${libcoap_dir}/src/coap_uri.c"
    "${libcoap_dir}/src/coap_ws.c"
)

idf_component_register(
    INCLUDE_DIRS
        "${libcoap_dir}/include"
        "${sdk_port}/esp_idf/libcoap/include"
    PRIV_REQUIRES
        "lwip"
        "mbedtls"
    SRCS
        "${libcoap_srcs}"
)
