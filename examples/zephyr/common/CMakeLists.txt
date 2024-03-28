# Guard against using CONFIG_GOLIOTH_SAMPLE_COMMON=y from applications other than SDK examples.
if(CONFIG_GOLIOTH_SAMPLE_COMMON)
    file(RELATIVE_PATH relative_path
        ${ZEPHYR_GOLIOTH_FIRMWARE_SDK_MODULE_DIR}
        ${APPLICATION_SOURCE_DIR})

    string(REGEX MATCH "^examples" matched ${relative_path})

    if(NOT matched)
        message(WARNING
            "WARNING: Using CONFIG_GOLIOTH_SAMPLE_COMMON=y with external \
             application code. This option is supposed to be used only \
             from projects in ${ZEPHYR_GOLIOTH_FIRMWARE_SDK_MODULE_DIR}/examples/
        ")
    endif()
endif()

zephyr_library_sources_ifndef(CONFIG_NET_CONFIG_NEED_IPV4 net_connect.c)
zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS hardcoded_credentials.c)
zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_NRF91_LTE_MONITOR nrf91_lte_monitor.c)
zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_PSK_SETTINGS settings_golioth.c)
zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_SETTINGS_AUTOLOAD settings_autoload.c)
zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_SETTINGS_SHELL settings_shell.c)
zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_WIFI wifi.c)

zephyr_include_directories(include)

if(CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS AND CONFIG_GOLIOTH_SAMPLE_AUTH_TYPE_PKI)
  include_cert(CONFIG_GOLIOTH_SAMPLE_HARDCODED_CRT_PATH crt)
  include_cert(CONFIG_GOLIOTH_SAMPLE_HARDCODED_KEY_PATH key)
endif()
