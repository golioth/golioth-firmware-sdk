/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-02-19
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include <driver/gpio.h>
#include <string.h>
#include <esp_log.h>
#include "epaper.h"
#include "epaper_priv.h"
#include "board.h"
#include "golioth_time.h"
#include "EPD_2in9d.h"
#include "ImageData.h"

#define TAG "epaper"

void DEV_Digital_Write(uint8_t pin, uint8_t value) {
    gpio_set_level(pin, value);
}

uint8_t DEV_Digital_Read(uint8_t pin) {
    return gpio_get_level(pin);
}

static void epaper_gpio_init(void) {
    gpio_config_t busy = {
            .pin_bit_mask = (1ULL << EPAPER_BUSY),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = 1,
    };
    ESP_ERROR_CHECK(gpio_config(&busy));

    gpio_config_t reset = {
            .pin_bit_mask = (1ULL << EPAPER_RESET),
            .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&reset));

    gpio_config_t dc = {
            .pin_bit_mask = (1ULL << EPAPER_DATA_COMMAND),
            .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&dc));

    gpio_config_t csel = {
            .pin_bit_mask = (1ULL << EPAPER_CSEL),
            .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&csel));

    gpio_config_t mosi = {
            .pin_bit_mask = (1ULL << EPAPER_MOSI),
            .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&mosi));

    gpio_config_t sclk = {
            .pin_bit_mask = (1ULL << EPAPER_SCLK),
            .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&sclk));

    gpio_set_level(EPAPER_CSEL, 1);
    gpio_set_level(EPAPER_SCLK, 0);
}

void DEV_SPI_WriteByte(UBYTE data) {
    for (int i = 0; i < 8; i++) {
        if ((data & 0x80) == 0) {
            gpio_set_level(EPAPER_MOSI, 0);
        } else {
            gpio_set_level(EPAPER_MOSI, 1);
        }

        data <<= 1;
        gpio_set_level(EPAPER_SCLK, 1);
        gpio_set_level(EPAPER_SCLK, 0);
    }
}

void epaper_init(void) {
    ESP_LOGI(TAG, "Setup ePaper pins");
    epaper_gpio_init();

    ESP_LOGI(TAG, "ePaper Init and Clear");
    EPD_2IN9D_Init();
    EPD_2IN9D_Clear();
    golioth_time_delay_ms(500);

    ESP_LOGI(TAG, "Show Golioth logo");
    EPD_2IN9D_Display(
            (void*)gImage_2in9); /* cast because function is not expecting a CONST array) */
    EPD_2IN9D_Sleep();           /* always sleep the ePaper display to avoid damaging it */
}

void epaper_autowrite_len(uint8_t* str, size_t len) {
    static uint8_t line = 0;
    if (line > 7) {
        EPD_2IN9D_Init();
        EPD_2IN9D_Clear();
        EPD_2IN9D_FullRefreshDoubleLine(str, len);
        line = 1;
    } else {
        epaper_WriteDoubleLine(str, len, line);
        ++line;
    }
}

void epaper_autowrite(uint8_t* str) {
    epaper_autowrite_len(str, strlen((const char*)str));
}
