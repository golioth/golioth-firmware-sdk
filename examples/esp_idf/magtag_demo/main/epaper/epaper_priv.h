#pragma once

#include <stdint.h>

#define UBYTE uint8_t
#define UWORD uint16_t
#define UDOUBLE uint32_t

#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define HIGH 1
#define LOW 0

void DEV_Digital_Write(uint8_t pin, uint8_t value);
uint8_t DEV_Digital_Read(uint8_t pin);
void DEV_SPI_WriteByte(UBYTE data);
