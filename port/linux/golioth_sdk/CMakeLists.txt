set(repo_root ../../..)
set(sdk_src ${repo_root}/src)
set(sdk_port ${repo_root}/port)

find_package(OpenSSL MODULE REQUIRED)
find_package(libcoap CONFIG REQUIRED)

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
)

add_library(golioth_sdk ${sdk_srcs})
target_include_directories(golioth_sdk
    PUBLIC
        ${sdk_src}/include
        ${sdk_port}/linux
        /usr/include/cjson
    PRIVATE
        ${sdk_src}/priv_include
)
target_link_libraries(golioth_sdk
    PUBLIC cjson
    PRIVATE libcoap::coap-3)
