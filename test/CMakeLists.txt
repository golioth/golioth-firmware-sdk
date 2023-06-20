cmake_minimum_required(VERSION 3.5)
project(host_test C)

set(CMAKE_BUILD_TYPE Debug)

# Flags needed for coverage reports
set(CMAKE_C_FLAGS "-fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage -coverage")

set(repo_root ..)
enable_testing()

# Build Unity

add_library(unity STATIC ${repo_root}/external/unity/src/unity.c)
target_compile_definitions(unity PUBLIC -DUNITY_INCLUDE_CONFIG_H)
target_include_directories(unity PUBLIC ${repo_root}/external/unity/src .)

# Build FFF

add_library(fff INTERFACE)
target_include_directories(fff INTERFACE ${repo_root}/external/fff)

# Build zcbor

set(zcbor_dir "${repo_root}/external/zcbor")
add_library(zcbor
    "${zcbor_dir}/src/zcbor_common.c"
    "${zcbor_dir}/src/zcbor_decode.c"
    "${zcbor_dir}/src/zcbor_encode.c"
    "${repo_root}/src/zcbor_utils.c"
)
target_include_directories(zcbor PUBLIC "${zcbor_dir}/include")
target_include_directories(zcbor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(zcbor PRIVATE "-Wno-strict-aliasing" "-Wno-uninitialized")

# Build libcoap

set(ENABLE_DOCS OFF)
add_subdirectory("${repo_root}/external/libcoap" build)

# Function for declaring unit tests

function(golioth_unit_test name src)
    add_executable(${name} ${src} ${ARGN})
    target_include_directories(${name} PRIVATE
        ${repo_root}/src/priv_include
        ${repo_root}/src/include
    )
    target_link_libraries(${name} unity fff)
    add_test(NAME ${name} COMMAND "./${name}")
endfunction()

#
# Test executables
#

# Ringbuf unit tests

golioth_unit_test(test_ringbuf
    ${repo_root}/src/golioth_ringbuf.c
    test_ringbuf.c
)

# RPC unit tests

golioth_unit_test(test_golioth_rpc
    test_golioth_rpc.c
    fakes/golioth_coap_client_fake.c
    fakes/golioth_time_fake.c
)
find_package(coap-3)
target_include_directories(test_golioth_rpc PRIVATE
    ${repo_root}/external/libcoap/include
    ${repo_root}/port/linux
    $<TARGET_PROPERTY:coap-3,INCLUDE_DIRECTORIES>
)
target_link_libraries(test_golioth_rpc zcbor)
