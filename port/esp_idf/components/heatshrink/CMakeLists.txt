set(sdk_root ../../../..)
set(heatshrink_dir ${sdk_root}/external/heatshrink)

idf_component_register(
    INCLUDE_DIRS
        "${heatshrink_dir}/include"
        "${heatshrink_dir}/src"
    SRCS
        "${heatshrink_dir}/src/heatshrink_decoder.c"
)
target_compile_definitions(${COMPONENT_LIB} PUBLIC -DHEATSHRINK_DYNAMIC_ALLOC=0)
