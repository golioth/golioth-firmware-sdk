#pragma once

#include <stdint.h>
#include <stdlib.h>

// TODO - more efficient driver, use SPI peripheral instead of bit-banging IO

void epaper_init(void);
void epaper_autowrite(uint8_t* str);                  // str must be NULL-terminated
void epaper_autowrite_len(uint8_t* str, size_t len);  // str does not have to be NULL-terminated
