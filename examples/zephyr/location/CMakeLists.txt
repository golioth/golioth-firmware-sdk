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
project(location)

target_sources(app PRIVATE src/main.c)
target_sources_ifdef(CONFIG_GOLIOTH_CELLULAR_PLAYBACK app PRIVATE src/cellular_playback.c)
target_sources_ifdef(CONFIG_GOLIOTH_WIFI_PLAYBACK app PRIVATE src/wifi_playback.c)
target_sources_ifdef(CONFIG_SOC_SERIES_NRF91X app PRIVATE src/cellular_nrf91.c)
