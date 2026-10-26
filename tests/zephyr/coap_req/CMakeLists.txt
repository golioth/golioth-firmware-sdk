# Copyright (c) 2026 Golioth, Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(golioth_coap_req_test)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ${ZEPHYR_GOLIOTH_FIRMWARE_SDK_MODULE_DIR}/src)

# Capture outgoing packets instead of writing them to the (unconnected) socket
zephyr_ld_options(-Wl,--wrap=golioth_send_coap)
