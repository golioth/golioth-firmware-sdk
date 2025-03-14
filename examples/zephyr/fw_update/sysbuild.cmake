if (SB_CONFIG_FW_UPDATE_NEXT)
    ExternalZephyrProject_Add(
        APPLICATION fw_update_next
        SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/next
        BUILD_ONLY True
    )
endif()
