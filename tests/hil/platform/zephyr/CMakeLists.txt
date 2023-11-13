# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

target_sources(app PRIVATE src/main.c ../../tests/${GOLIOTH_HIL_TEST}/test.c)

if (CONFIG_BOARD STREQUAL "esp32_devkitc_wrover")

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/zephyr/merged.bin
    COMMAND ${PYTHON_EXECUTABLE} ${ESP_IDF_PATH}/components/esptool_py/esptool/esptool.py --chip ESP32 merge_bin --flash_size 8MB -o ${CMAKE_BINARY_DIR}/zephyr/merged.bin 0x1000 ${CMAKE_BINARY_DIR}/esp-idf/build/bootloader/bootloader.bin 0x8000 ${CMAKE_BINARY_DIR}/esp-idf/build/partitions_singleapp.bin 0x10000 ${CMAKE_BINARY_DIR}/zephyr/zephyr.bin
    DEPENDS ${logical_target_for_zephyr_elf}
)

add_custom_target(mergebin ALL DEPENDS ${CMAKE_BINARY_DIR}/zephyr/merged.bin)

endif()
