set(sdk_root ../../../..)
set(bsdiff_dir ${sdk_root}/external/bsdiff)

idf_component_register(
    INCLUDE_DIRS
        "${bsdiff_dir}"
    SRCS
        "${bsdiff_dir}/bspatch.c"
)
