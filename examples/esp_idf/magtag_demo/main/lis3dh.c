#include <esp_log.h>
#include "i2c.h"
#include "lis3dh.h"

#define TAG "lis3dh"

#define LIS3DH_EXPECTED_WHOAMI 0x33

static uint8_t _i2c_addr;

void lis3dh_init(uint8_t i2c_addr) {
    _i2c_addr = i2c_addr;

    uint8_t whoami = read_reg8(_i2c_addr, LIS3DH_REG_WHOAMI);
    assert(whoami == LIS3DH_EXPECTED_WHOAMI);

    // Enable all axes, normal mode, 400 Hz rate
    ESP_ERROR_CHECK(write_reg8(_i2c_addr, LIS3DH_REG_CTRL1, 0x77));

    // High res & BDU enabled, +/- 2g range
    ESP_ERROR_CHECK(write_reg8(_i2c_addr, LIS3DH_REG_CTRL4, 0x88));

    // enable interrupt on ZYXDA
    ESP_ERROR_CHECK(write_reg8(_i2c_addr, LIS3DH_REG_CTRL3, 0x10));

    // Turn on orientation config, enable ADC
    ESP_ERROR_CHECK(write_reg8(_i2c_addr, LIS3DH_REG_TEMPCFG, 0x80));
}

esp_err_t lis3dh_accel_read(lis3dh_accel_data_t* data) {
    uint8_t reg_addr = LIS3DH_REG_OUT_X_L;
    reg_addr |= 0x80;  // set [7] for auto-increment

    uint8_t buffer[6] = {};
    esp_err_t status = i2c_master_read_reg(_i2c_addr, reg_addr, buffer, sizeof(buffer));
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read REG_OUT_X_L");
        return status;
    }

    int16_t x_raw = buffer[0];
    x_raw |= ((uint16_t)buffer[1]) << 8;
    int16_t y_raw = buffer[2];
    y_raw |= ((uint16_t)buffer[3]) << 8;
    int16_t z_raw = buffer[4];
    z_raw |= ((uint16_t)buffer[5]) << 8;

    // Note: Assuming 2g range and 12-bit mode (hi-res) configured in CTRL4
    float g_per_digit = .001;
    data->x_mps2 = g_per_digit * (x_raw >> 4) * LIS3DH_GRAVITY_EARTH;
    data->y_mps2 = g_per_digit * (y_raw >> 4) * LIS3DH_GRAVITY_EARTH;
    data->z_mps2 = g_per_digit * (z_raw >> 4) * LIS3DH_GRAVITY_EARTH;

    return ESP_OK;
}
