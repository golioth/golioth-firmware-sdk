# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if(BOARD STREQUAL "nrf52840dk/nrf52840")
    if(NOT SHIELD)
        set(SHIELD golioth_esp_at)
        message(WARNING "Selecting golioth_esp_at SHIELD.")
    endif()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(logging)

target_sources(app PRIVATE src/main.c)
