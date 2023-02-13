set(sdk_root ../../../..)
set(heatshrink_dir ${sdk_root}/external/heatshrink)

idf_component_register(
    INCLUDE_DIRS
        "${heatshrink_dir}/include"
        "${heatshrink_dir}/src"
    SRCS
        "${heatshrink_dir}/src/heatshrink_decoder.c"
)
