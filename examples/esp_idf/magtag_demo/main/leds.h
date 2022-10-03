#pragma once

// In R/G/B order
#define BLACK 0x000000
#define RED 0xFF0000
#define GREEN 0x00FF00
#define BLUE 0x0000FF
#define YELLOW 0xFFFF00

#define NUM_LEDS 4

void leds_init(void);
void leds_set_led(int led_index, uint32_t rgb);
void leds_set_led_immediate(int led_index, uint32_t rgb);
void leds_set_all(uint32_t rgb);
void leds_set_all_immediate(uint32_t rgb);
void leds_display(void);
