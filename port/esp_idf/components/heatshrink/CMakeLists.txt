set(sdk_root ../../../..)
set(heatshrink_dir ${sdk_root}/external/heatshrink)

idf_component_register(
    INCLUDE_DIRS
        "${heatshrink_dir}"
    SRCS
        "${heatshrink_dir}/heatshrink_decoder.c"
)
