#pragma once

// Event bits
#define EVENT_BUTTON_A_PRESSED (1 << 0)
#define EVENT_BUTTON_B_PRESSED (1 << 1)
#define EVENT_BUTTON_C_PRESSED (1 << 2)
#define EVENT_BUTTON_D_PRESSED (1 << 3)
#define EVENT_BUTTON_ANY (0xF)
#define EVENT_TIMER_250MS (1 << 4)
#define EVENT_ANY (0xFFFF)

extern EventGroupHandle_t _event_group;
