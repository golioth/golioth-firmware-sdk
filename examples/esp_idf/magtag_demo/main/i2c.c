#include "i2c.h"
#include <driver/i2c.h>
#include <esp_log.h>

#define TAG "i2c"

#define I2C_WRITE 0
#define I2C_READ 1
#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

void i2c_master_init(int scl_io, int sda_io) {
    i2c_config_t i2c_config = {};
    i2c_config.mode = I2C_MODE_MASTER;
    i2c_config.sda_io_num = sda_io;
    i2c_config.sda_pullup_en = 1;
    i2c_config.scl_io_num = scl_io;
    i2c_config.scl_pullup_en = 1;
    i2c_config.master.clk_speed = I2C_MASTER_CLK_FREQ_HZ;
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));
}

esp_err_t i2c_master_read_reg(uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_rd, size_t size) {
    if (size == 0) {
        return ESP_OK;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Send address write and register to be read, with ack
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((i2c_addr << 1) | I2C_WRITE), true);
    i2c_master_write_byte(cmd, i2c_reg, true);
    // Send another start, then address (read), with ack
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((i2c_addr << 1) | I2C_READ), true);
    // Read data from slave until the last byte
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, I2C_MASTER_ACK);
    }
    // Read the last byte, with NACK
    i2c_master_read_byte(cmd, data_rd + size - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t status = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return status;
}

esp_err_t i2c_master_write_reg(uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_wr, size_t size) {
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((i2c_addr << 1) | I2C_WRITE), true);
    i2c_master_write_byte(cmd, i2c_reg, true);
    // Write size bytes
    i2c_master_write(cmd, data_wr, size, true);
    i2c_master_stop(cmd);
    esp_err_t status = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return status;
}

uint8_t read_reg8(uint8_t i2c_addr, uint8_t i2c_reg) {
    uint8_t val = 0;
    esp_err_t status = i2c_master_read_reg(i2c_addr, i2c_reg, &val, 1);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Timeout reading reg 0x%02X", i2c_reg);
        return 0;
    }
    return val;
}

esp_err_t write_reg8(uint8_t i2c_addr, uint8_t i2c_reg, uint8_t value) {
    esp_err_t status = i2c_master_write_reg(i2c_addr, i2c_reg, &value, 1);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Timeout writing reg 0x%02X", i2c_reg);
    }
    return status;
}
