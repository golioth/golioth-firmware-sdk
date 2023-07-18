zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS hardcoded_credentials.c)
zephyr_library_sources(net_connect.c)
zephyr_library_sources_ifdef(CONFIG_GOLIOTH_SAMPLE_WIFI wifi.c)

zephyr_include_directories(include)

if(CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS AND CONFIG_GOLIOTH_AUTH_METHOD_CERT)
  foreach(type IN ITEMS crt key)
    string(TOUPPER ${type} type_upper)
    set(path ${CONFIG_GOLIOTH_SAMPLE_HARDCODED_${type_upper}_PATH})

    if(NOT IS_ABSOLUTE ${path})
      if(EXISTS ${APPLICATION_SOURCE_DIR}/${path})
        set(path ${APPLICATION_SOURCE_DIR}/${path})
      elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${path})
        set(path ${CMAKE_CURRENT_SOURCE_DIR}/${path})
      else()
        message(FATAL_ERROR "${path} configured by CONFIG_GOLIOTH_SAMPLE_HARDCODED_${type_upper}_PATH does not exist")
      endif()
    endif()

    generate_inc_file_for_target(app ${path}
      ${ZEPHYR_BINARY_DIR}/include/generated/golioth-systemclient-${type}.inc)
  endforeach()
endif()
