# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cert_provisioning)

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
