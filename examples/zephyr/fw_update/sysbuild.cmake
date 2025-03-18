function(build_fw_update_next app_name)
    ExternalZephyrProject_Add(
        APPLICATION ${app_name}
        SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/next
        BUILD_ONLY True
    )

    # Set application name manually, so it is respected by included cmake snippet
    set(ZCMAKE_APPLICATION ${app_name})

    # Reuse MAIN application default confiugration, i.e. propagate sysbuild configuration to
    # bootloader config values (such as mcuboot key).
    include(${ZEPHYR_BASE}/share/sysbuild/image_configurations/MAIN_image_default.cmake)
endfunction()

if (SB_CONFIG_FW_UPDATE_NEXT)
    build_fw_update_next(fw_update_next)
endif()
