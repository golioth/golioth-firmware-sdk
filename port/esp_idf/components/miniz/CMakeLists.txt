set(sdk_root ../../../..)
set(sdk_port ${sdk_root}/port)
set(miniz_dir ${sdk_root}/external/miniz)

idf_component_register(
    INCLUDE_DIRS
        "${miniz_dir}"
        "${sdk_port}/common/miniz"
    SRCS
        "${miniz_dir}/miniz_tinfl.c"
)
