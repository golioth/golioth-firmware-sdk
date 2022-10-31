cmake_minimum_required(VERSION 3.5)
set(projname "golioth_basics")
project(${projname} C)

set(CMAKE_BUILD_TYPE Debug)

set(repo_root ../../..)

set(srcs
    main.c
    ${repo_root}/examples/common/golioth_basics.c
)

add_subdirectory(${repo_root}/port/linux/golioth_sdk build)
add_executable(${projname} ${srcs})
target_include_directories(${projname} PRIVATE ${repo_root}/examples/common)
target_link_libraries(${projname} golioth_sdk)
