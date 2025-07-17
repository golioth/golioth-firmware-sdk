if(NOT CONFIG_GOLIOTH_FIRMWARE_SDK)
    return()
endif()

zephyr_include_directories(include)
zephyr_include_directories(../../include)

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

zephyr_library_include_directories(
    # bsdiff
    ../../external/bsdiff
)

zephyr_library_sources(
    # SDK
    ../../src/zephyr_coap_req.c
    ../../src/zephyr_coap_utils.c
    ../../src/coap_client.c
    ../../src/coap_client_zephyr.c
    ../../src/golioth_debug.c
    ../../src/event_group.c
    ../../src/fw_update.c
    ../../src/coap_blockwise.c
    ../../src/gateway.c
    ../../src/lightdb_state.c
    ../../src/net_info.c
    ../../src/net_info_cellular.c
    ../../src/net_info_wifi.c
    ../../src/stream.c
    ../../src/log.c
    ../../src/mbox.c
    ../../src/ota.c
    ../../src/payload_utils.c
    ../../src/ringbuf.c
    ../../src/rpc.c
    ../../src/settings.c
    ../../src/golioth_status.c
    ../../src/zcbor_utils.c
    golioth_sys_zephyr.c
)

zephyr_library_sources_ifdef(CONFIG_GOLIOTH_FW_UPDATE golioth_fw_zephyr.c)
zephyr_library_sources_ifdef(CONFIG_NET_L2_OPENTHREAD golioth_openthread.c)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../VERSION.txt")
    include(${CMAKE_CURRENT_LIST_DIR}/golioth_version.cmake)
    zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SHOW_VERSION_BOOT_MSG golioth_version_boot_msg.c)
else()
    message(STATUS "Unable to find Golioth Firmware SDK VERSION.txt file")
endif()

if(CONFIG_GOLIOTH_AUTH_METHOD_CERT)
  include_cert(CONFIG_GOLIOTH_HARDCODED_CA_CRT_PATH ca_crt)
  if(CONFIG_GOLIOTH_SECONDARY_CA_CRT)
    include_cert(CONFIG_GOLIOTH_HARDCODED_SECONDARY_CA_CRT_PATH secondary_ca_crt)
  endif()
endif()

zephyr_library_sources_ifdef(CONFIG_LOG_BACKEND_GOLIOTH golioth_log_zephyr.c)

zephyr_library_compile_definitions(HEATSHRINK_DYNAMIC_ALLOC=0)

zephyr_library_link_libraries(mbedTLS)

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
