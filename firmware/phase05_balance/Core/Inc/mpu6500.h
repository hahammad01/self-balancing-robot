#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>
#include "main.h"

#define MPU_ADDR          (0x68 << 1)   /* HAL wants the 8-bit address */
#define MPU_WHO_AM_I      0x75
#define MPU_PWR_MGMT_1    0x6B
#define MPU_ACCEL_XOUT_H  0x3B

typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
} MPU_Raw;

HAL_StatusTypeDef MPU_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MPU_ReadRaw(I2C_HandleTypeDef *hi2c, MPU_Raw *out);

#endif
