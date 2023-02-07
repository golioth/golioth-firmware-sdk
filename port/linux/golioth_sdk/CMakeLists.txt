set(repo_root ../../..)
set(sdk_src ${repo_root}/src)
set(sdk_port ${repo_root}/port)
set(heatshrink_dir ${repo_root}/external/heatshrink)

option(ENABLE_DOCS "" OFF)
option(ENABLE_EXAMPLES "" OFF)
option(ENABLE_SERVER_MODE "" OFF)
option(ENABLE_TCP "" OFF)
add_subdirectory("${repo_root}/external/libcoap" build)

# Build cJSON from source
set(cjson_dir "${repo_root}/external/cJSON")
add_library(cjson
    "${cjson_dir}/cJSON.c"
    "${cjson_dir}/cJSON_Utils.c"
)
target_include_directories(cjson PUBLIC "${cjson_dir}")

set(heatshrink_srcs
    "${heatshrink_dir}/heatshrink_decoder.c"
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
    "${sdk_src}/golioth_decompressor.c"
)

add_library(golioth_sdk
    ${heatshrink_srcs}
    ${sdk_srcs}
)
target_include_directories(golioth_sdk
    PUBLIC
        ${sdk_src}/include
        ${sdk_port}/linux
    PRIVATE
        ${heatshrink_dir}
        ${sdk_src}/priv_include
)
target_link_libraries(golioth_sdk
    PUBLIC cjson
    PRIVATE coap-3 pthread rt)
