#pragma once

#include <esp_err.h>
#include <stdint.h>

// Ref: https://www.st.com/resource/en/datasheet/lis3dh.pdf

// Registers
#define LIS3DH_REG_WHOAMI 0x0F
#define LIS3DH_REG_TEMPCFG 0x1F
#define LIS3DH_REG_CTRL1 0x20
#define LIS3DH_REG_CTRL3 0x22
#define LIS3DH_REG_CTRL4 0x23
#define LIS3DH_REG_OUT_X_L 0x28

///< Scalar to convert from 16-bit lsb to 10-bit and divide by 1k to
///< convert from milli-gs to gs
#define LIS3DH_LSB16_TO_KILO_LSB10 64000

#define LIS3DH_GRAVITY_EARTH (9.80665F)

typedef struct {
    // m/s^2
    float x_mps2;
    float y_mps2;
    float z_mps2;
} lis3dh_accel_data_t;

void lis3dh_init(uint8_t i2c_addr);
esp_err_t lis3dh_accel_read(lis3dh_accel_data_t* data);
