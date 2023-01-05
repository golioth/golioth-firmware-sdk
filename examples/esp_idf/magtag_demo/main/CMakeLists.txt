if(${IDF_VERSION_MAJOR} EQUAL "5")
    set(esp_adc_lib "esp_adc")
else()
    set(esp_adc_lib "esp_adc_cal")
endif()

idf_component_register(
    INCLUDE_DIRS
        "."
        "epaper"
        "../../common"
    SRCS
        "app_main.c"
        "leds.c"
        "led_strip_rmt_ws2812.c"
        "buttons.c"
        "i2c.c"
        "lis3dh.c"
        "speaker.c"
        "light_sensor.c"
        "epaper/epaper.c"
        "epaper/EPD_2in9d.c"
        "epaper/ImageData.c"
        "../../common/shell.c"
        "../../common/wifi.c"
        "../../common/nvs.c"
    PRIV_REQUIRES
        "golioth_sdk"
        "json"
        "console"
        "nvs_flash"
        "${esp_adc_lib}"
        "spi_flash"
        "esp_wifi"
        "esp_timer"
        "driver"
)
list(APPEND EXTRA_C_FLAGS_LIST
    -Wno-error
)
component_compile_options(${EXTRA_C_FLAGS_LIST})
