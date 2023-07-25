set(repo_root ../../..)
set(sdk_src ${repo_root}/src)
set(sdk_port ${repo_root}/port)
set(heatshrink_dir ${repo_root}/external/heatshrink)
set(miniz_dir ${repo_root}/external/miniz)
set(bsdiff_dir ${repo_root}/external/bsdiff)
set(zcbor_dir ${repo_root}/external/zcbor)

option(ENABLE_DOCS "" OFF)
option(ENABLE_EXAMPLES "" OFF)
option(ENABLE_SERVER_MODE "" OFF)
option(ENABLE_TCP "" OFF)
add_subdirectory("${repo_root}/external/libcoap" build)

set(heatshrink_srcs
    "${heatshrink_dir}/src/heatshrink_decoder.c"
)

set(miniz_srcs
    "${miniz_dir}/miniz_tinfl.c"
)

set(bsdiff_srcs
    "${bsdiff_dir}/bspatch.c"
)

set(zcbor_srcs
    "${zcbor_dir}/src/zcbor_common.c"
    "${zcbor_dir}/src/zcbor_decode.c"
    "${zcbor_dir}/src/zcbor_encode.c"
)

# Build Golioth SDK
set(sdk_srcs
    "${sdk_port}/linux//golioth_sys_linux.c"
    "${sdk_port}/linux/fw_update_linux.c"
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
    "${sdk_src}/golioth_ringbuf.c"
    "${sdk_src}/golioth_event_group.c"
    "${sdk_src}/golioth_mbox.c"
    "${sdk_src}/golioth_debug.c"
    "${sdk_src}/golioth_fw_block_processor.c"
    "${sdk_src}/zcbor_utils.c"
)

add_library(golioth_sdk
    ${heatshrink_srcs}
    ${miniz_srcs}
    ${bsdiff_srcs}
    ${zcbor_srcs}
    ${sdk_srcs}
)

target_include_directories(golioth_sdk
    PUBLIC
        ${zcbor_dir}/include
        ${sdk_src}/include
        ${sdk_port}/linux
    PRIVATE
        ${bsdiff_dir}
        ${heatshrink_dir}/include
        ${heatshrink_dir}/src
        ${miniz_dir}
        ${zcbor_dir}
        ${sdk_port}/common/miniz
        ${sdk_src}/priv_include
)
target_link_libraries(golioth_sdk
    PRIVATE coap-3 pthread rt)
target_compile_definitions(golioth_sdk PRIVATE -DHEATSHRINK_DYNAMIC_ALLOC=0)
