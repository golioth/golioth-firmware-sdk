#pragma once

// Small LED on back
#define D13_LED_GPIO_PIN 13

// Front-face buttons
#define BUTTON_A_GPIO_PIN 15
#define BUTTON_B_GPIO_PIN 14
#define BUTTON_C_GPIO_PIN 12
#define BUTTON_D_GPIO_PIN 11

// LED strip
//
// Refer to Adafruit schematic:
// https://cdn-learn.adafruit.com/assets/assets/000/096/946/original/adafruit_products_MagTag_sch.png?1605026160
#define LED_STRIP_DATA_PIN 1
#define LED_STRIP_POWER_PIN 21
#define LED_STRIP_NUM_RGB_LEDS 4

// Epaper
#define EPAPER_BUSY 5
#define EPAPER_RESET 6
#define EPAPER_DATA_COMMAND 7
#define EPAPER_CSEL 8
#define EPAPER_MOSI 35
#define EPAPER_SCLK 36
#define EPAPER_MISO 37

// Accelerometer, LIS3DH
#define I2C_SCL_PIN 34
#define I2C_SDA_PIN 33
#define LIS3DH_I2C_ADDR 0x19

// Speaker
#define SPEAKER_DAC1_PIN 17
#define SPEAKER_ENABLE_PIN 16
