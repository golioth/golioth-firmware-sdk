if(NOT CONFIG_GOLIOTH_FIRMWARE_SDK)
    return()
endif()

zephyr_include_directories(include)
zephyr_include_directories(../../src/include)

zephyr_library()
zephyr_library_include_directories(../../src/priv_include)

zephyr_library_include_directories(
    # bsdiff
    ../../external/bsdiff

    # heatshrink
    ../../external/heatshrink/src
    ../../external/heatshrink/include

    # libcoap
    ../../external/libcoap/include
    libcoap/include

    # miniz
    ../../external/miniz
    ../common/miniz
)

zephyr_library_sources(
    # SDK
    ../../src/golioth_coap_client.c
    ../../src/golioth_debug.c
    ../../src/golioth_event_group.c
    ../../src/golioth_fw_block_processor.c
    ../../src/golioth_fw_update.c
    ../../src/golioth_lightdb.c
    ../../src/golioth_log.c
    ../../src/golioth_mbox.c
    ../../src/golioth_ota.c
    ../../src/golioth_remote_shell.c
    ../../src/golioth_ringbuf.c
    ../../src/golioth_rpc.c
    ../../src/golioth_settings.c
    ../../src/golioth_statistics.c
    ../../src/golioth_status.c
    ../../src/golioth_time.c
    ../../src/zcbor_utils.c
    golioth_fw_zephyr.c
    golioth_sys_zephyr.c

    # bsdiff
    ../../external/bsdiff/bspatch.c

    # heatshrink
    ../../external/heatshrink/src/heatshrink_decoder.c

    # libcoap
    ../../external/libcoap/src/block.c
    ../../external/libcoap/src/coap_address.c
    ../../external/libcoap/src/coap_asn1.c
    ../../external/libcoap/src/coap_async.c
    ../../external/libcoap/src/coap_cache.c
    ../../external/libcoap/src/coap_debug.c
    ../../external/libcoap/src/coap_encode.c
    ../../external/libcoap/src/coap_event.c
    ../../external/libcoap/src/coap_hashkey.c
    ../../external/libcoap/src/coap_mem.c
    ../../external/libcoap/src/coap_option.c
    ../../external/libcoap/src/coap_prng.c
    ../../external/libcoap/src/coap_session.c
    ../../external/libcoap/src/coap_str.c
    ../../external/libcoap/src/coap_subscribe.c
    ../../external/libcoap/src/coap_tcp.c
    ../../external/libcoap/src/coap_uri.c
    ../../external/libcoap/src/net.c
    ../../external/libcoap/src/pdu.c
    ../../external/libcoap/src/resource.c
    libcoap/io.c
    libcoap/log.c
    libcoap/time.c
    libcoap/tls.c

    # miniz
    ../../external/miniz/miniz_tinfl.c
)

zephyr_library_compile_definitions(HEATSHRINK_DYNAMIC_ALLOC=0)

zephyr_library_link_libraries(mbedTLS posix_subsys)
