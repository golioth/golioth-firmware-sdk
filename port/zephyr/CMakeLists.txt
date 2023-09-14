if(NOT CONFIG_GOLIOTH_FIRMWARE_SDK)
    return()
endif()

zephyr_include_directories(include)
zephyr_include_directories(../../src/include)

function (include_cert path_var name)
  set(path ${${path_var}})
  if(NOT IS_ABSOLUTE ${path})
    if(EXISTS ${APPLICATION_SOURCE_DIR}/${path})
      set(path ${APPLICATION_SOURCE_DIR}/${path})
    elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${path})
      set(path ${CMAKE_CURRENT_SOURCE_DIR}/${path})
    else()
      message(FATAL_ERROR "${path} set by ${path_var} does not exist")
    endif()
  endif()

  generate_inc_file_for_target(app ${path}
    ${ZEPHYR_BINARY_DIR}/include/generated/golioth-systemclient-${name}.inc)
endfunction()

add_subdirectory_ifdef(CONFIG_GOLIOTH_SAMPLE_COMMON
    ${ZEPHYR_GOLIOTH_FIRMWARE_SDK_MODULE_DIR}/examples/zephyr/common examples_zephyr_common)

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
    ../../external/libcoap/src/coap_address.c
    ../../external/libcoap/src/coap_asn1.c
    ../../external/libcoap/src/coap_async.c
    ../../external/libcoap/src/coap_block.c
    ../../external/libcoap/src/coap_cache.c
    ../../external/libcoap/src/coap_debug.c
    ../../external/libcoap/src/coap_dtls.c
    ../../external/libcoap/src/coap_encode.c
    ../../external/libcoap/src/coap_event.c
    ../../external/libcoap/src/coap_hashkey.c
    ../../external/libcoap/src/coap_layers.c
    ../../external/libcoap/src/coap_mem.c
    ../../external/libcoap/src/coap_net.c
    ../../external/libcoap/src/coap_netif.c
    ../../external/libcoap/src/coap_option.c
    ../../external/libcoap/src/coap_pdu.c
    ../../external/libcoap/src/coap_prng.c
    ../../external/libcoap/src/coap_resource.c
    ../../external/libcoap/src/coap_session.c
    ../../external/libcoap/src/coap_str.c
    ../../external/libcoap/src/coap_subscribe.c
    ../../external/libcoap/src/coap_tcp.c
    ../../external/libcoap/src/coap_uri.c
    ../../external/libcoap/src/coap_ws.c
    libcoap/coap_io_zephyr.c
    libcoap/coap_time.c
    libcoap/coap_zephyrtls.c
    libcoap/log.c

    # miniz
    ../../external/miniz/miniz_tinfl.c
)

if(CONFIG_GOLIOTH_AUTH_METHOD_CERT)
  include_cert(CONFIG_GOLIOTH_HARDCODED_CA_CRT_PATH ca_crt)
endif()

zephyr_library_sources_ifdef(CONFIG_LOG_BACKEND_GOLIOTH golioth_log_zephyr.c)

zephyr_library_compile_definitions(HEATSHRINK_DYNAMIC_ALLOC=0)

zephyr_library_link_libraries(mbedTLS posix_subsys)

if(CONFIG_GOLIOTH_CIPHERSUITES)
  string(REGEX MATCHALL "[^,; ]+" ciphersuites ${CONFIG_GOLIOTH_CIPHERSUITES})
  string(JOIN ", \\\n\t" ciphersuites_macro_value ${ciphersuites})

  set(ciphersuites_generated "#define GOLIOTH_CIPHERSUITES \\\n\t${ciphersuites_macro_value}\n")
else()
  set(ciphersuites_generated "#define GOLIOTH_CIPHERSUITES\n")
endif()

file(WRITE
  ${ZEPHYR_BINARY_DIR}/include/generated/golioth_ciphersuites.h
  "${ciphersuites_generated}")

if(CONFIG_ZCBOR)
  file(READ ${ZEPHYR_ZCBOR_MODULE_DIR}/zcbor/VERSION zcbor_version)
  string(STRIP ${zcbor_version} zcbor_version)
  set(zcbor_version_regex "^([0-9]+)\.([0-9]+)\.([0-9]+)$")
  string(REGEX REPLACE ${zcbor_version_regex} "\\1" zcbor_version_major "${zcbor_version}")
  string(REGEX REPLACE ${zcbor_version_regex} "\\2" zcbor_version_minor "${zcbor_version}")
  zephyr_library_compile_definitions(ZCBOR_VERSION_MAJOR=${zcbor_version_major})
  zephyr_library_compile_definitions(ZCBOR_VERSION_MINOR=${zcbor_version_minor})
  zephyr_library_compile_definitions(ZCBOR_VARIANCE)

  # Workaround for broken zcbor_any_skip()
  if(${zcbor_version} VERSION_LESS "0.7")
    zephyr_library_sources(../../src/zcbor_any_skip_fixed.c)
  endif()
endif()

# Workaround for LTE Connectivity module in NCS being incompatible with CONFIG_POSIX_API=y
target_compile_definitions_ifdef(CONFIG_LTE_CONNECTIVITY app PRIVATE -Dinet_pton=zsock_inet_pton)
