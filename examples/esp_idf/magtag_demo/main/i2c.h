#pragma once

#include <stdint.h>
#include <esp_err.h>

#define I2C_PORT 0
#define I2C_MASTER_CLK_FREQ_HZ 100000
#define I2C_TIMEOUT_MS 1000

void i2c_master_init(int scl_io, int sda_io);
esp_err_t i2c_master_write_reg(uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_wr, size_t size);
esp_err_t i2c_master_read_reg(uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_rd, size_t size);
esp_err_t write_reg8(uint8_t i2c_addr, uint8_t i2c_reg, uint8_t value);
uint8_t read_reg8(uint8_t i2c_addr, uint8_t i2c_reg);
