set(repo_root ../../..)
set(sdk_src ${repo_root}/src)
set(sdk_port ${repo_root}/port)
set(zcbor_dir ${repo_root}/external/zcbor)

option(ENABLE_DOCS "" OFF)
option(ENABLE_EXAMPLES "" OFF)
option(ENABLE_SERVER_MODE "" OFF)
option(ENABLE_TCP "" OFF)
option(WITH_EPOLL "" OFF)
add_subdirectory("${repo_root}/external/libcoap" build)

set(zcbor_srcs
    "${zcbor_dir}/src/zcbor_common.c"
    "${zcbor_dir}/src/zcbor_decode.c"
    "${zcbor_dir}/src/zcbor_encode.c"
)

# Build Golioth SDK
set(sdk_srcs
    "${sdk_port}/linux//golioth_sys_linux.c"
    "${sdk_port}/linux/fw_update_linux.c"
    "${sdk_port}/utils/hex.c"
    "${sdk_src}/golioth_status.c"
    "${sdk_src}/coap_client.c"
    "${sdk_src}/coap_client_libcoap.c"
    "${sdk_src}/gateway.c"
    "${sdk_src}/log.c"
    "${sdk_src}/lightdb_state.c"
    "${sdk_src}/net_info.c"
    "${sdk_src}/net_info_cellular.c"
    "${sdk_src}/net_info_wifi.c"
    "${sdk_src}/stream.c"
    "${sdk_src}/rpc.c"
    "${sdk_src}/ota.c"
    "${sdk_src}/payload_utils.c"
    "${sdk_src}/fw_update.c"
    "${sdk_src}/settings.c"
    "${sdk_src}/ringbuf.c"
    "${sdk_src}/event_group.c"
    "${sdk_src}/mbox.c"
    "${sdk_src}/golioth_debug.c"
    "${sdk_src}/coap_blockwise.c"
    "${sdk_src}/zcbor_utils.c"
)

add_library(golioth_sdk
    ${zcbor_srcs}
    ${sdk_srcs}
)

target_include_directories(golioth_sdk
    PUBLIC
        ${zcbor_dir}/include
        ${repo_root}/include
        ${sdk_port}/linux
    PRIVATE
        ${zcbor_dir}
)
target_link_libraries(golioth_sdk
    PRIVATE coap-3 pthread rt crypto)
target_compile_definitions(golioth_sdk PRIVATE -DHEATSHRINK_DYNAMIC_ALLOC=0)
