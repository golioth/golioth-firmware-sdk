cmake_minimum_required(VERSION 3.5)
set(projname "hil")
project(${projname} C)

set(CMAKE_BUILD_TYPE Debug)

set(CONFIG_GOLIOTH_COAP_HOST_URI "coaps://coap.golioth.io" CACHE STRING "Golioth coap gateway URI")

set(repo_root ../../../..)

set(srcs
    main.c
    ${repo_root}/tests/hil/tests/${GOLIOTH_HIL_TEST}/test.c
)

get_filename_component(user_config_file "golioth_user_config.h" ABSOLUTE)
add_definitions(-DCONFIG_GOLIOTH_USER_CONFIG_INCLUDE="${user_config_file}")
add_definitions(-DCONFIG_GOLIOTH_COAP_HOST_URI="${CONFIG_GOLIOTH_COAP_HOST_URI}")

add_subdirectory(${repo_root}/port/linux/golioth_sdk build)
add_executable(${projname} ${srcs})
target_link_libraries(${projname} golioth_sdk)
