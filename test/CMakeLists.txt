cmake_minimum_required(VERSION 3.5)
project(host_test C)

set(CMAKE_BUILD_TYPE Debug)

# Flags needed for coverage reports
set(CMAKE_C_FLAGS "-fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage -coverage")

set(repo_root ..)
add_library(unity STATIC ${repo_root}/external/unity/src/unity.c)
target_compile_definitions(unity PUBLIC -DUNITY_INCLUDE_CONFIG_H)
target_include_directories(unity PUBLIC ${repo_root}/external/unity/src .)

add_library(fff INTERFACE)
target_include_directories(fff INTERFACE ${repo_root}/external/fff)

enable_testing()

set(root_dir ..)

#
# Test executables
#
set(test_name "test_ringbuf")
add_executable(${test_name}
    ${root_dir}/src/ringbuf.c
    test_ringbuf.c
)
target_include_directories(${test_name} PRIVATE
    ${root_dir}/src/priv_include
)
target_link_libraries(${test_name} unity fff)
add_test(NAME ${test_name} COMMAND "./${test_name}")
