# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if(BOARD STREQUAL "nrf52840dk/nrf52840")
    if(NOT SHIELD)
        set(SHIELD golioth_esp_at)
        message(WARNING
            "Implicitly selecting golioth_esp_at SHIELD. This behavior is\
             deprecated. Specify '--shield golioth_esp_at' explicitly with\
             'west build' invocation.")
    endif()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cert_rotation)

target_sources(app PRIVATE src/main.c)

add_custom_command(
    OUTPUT ${ZEPHYR_BINARY_DIR}/lfs.hex
    COMMAND ${PYTHON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/generate-lfs-image.py
      --build-dir ${APPLICATION_BINARY_DIR}
)

add_custom_target(
    lfs ALL
    DEPENDS ${ZEPHYR_BINARY_DIR}/lfs.hex
)
