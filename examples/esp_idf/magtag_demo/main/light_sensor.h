#pragma once

#include <stdint.h>

void light_sensor_init(void);
uint32_t light_sensor_read_mV(void);
