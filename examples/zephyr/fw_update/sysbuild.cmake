if (BOARD STREQUAL "nrf9160dk")
    add_overlay_dts(mcuboot "${CMAKE_CURRENT_LIST_DIR}/sysbuild/mcuboot_nrf9160dk_nrf9160_ns.overlay")

    add_overlay_config(mcuboot "${CMAKE_CURRENT_LIST_DIR}/sysbuild/mcuboot_nrf9160dk_nrf9160_ns.conf")

    set(SB_CONFIG_PM_EXTERNAL_FLASH_MCUBOOT_SECONDARY "y" CACHE INTERNAL "")
endif()
