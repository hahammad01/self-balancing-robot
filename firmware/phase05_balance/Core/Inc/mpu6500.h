#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>
#include "main.h"

#define MPU_ADDR          (0x68 << 1)   /* 7-bit address 0x68, shifted for HAL */

#define MPU_CONFIG        0x1A          /* DLPF_CFG[2:0]: gyro low-pass filter      */
#define MPU_GYRO_CONFIG   0x1B          /* GYRO_FS_SEL[4:3]: gyro full scale        */
#define MPU_ACCEL_CONFIG  0x1C          /* ACCEL_FS_SEL[4:3]: accel full scale      */
#define MPU_ACCEL_CONFIG2 0x1D          /* A_DLPF_CFG[2:0]: accel low-pass (6500 only) */
#define MPU_ACCEL_XOUT_H  0x3B
#define MPU_PWR_MGMT_1    0x6B
#define MPU_WHO_AM_I      0x75

#define MPU_WHOAMI_6050   0x68
#define MPU_WHOAMI_6500   0x70

/* Scale factors for the full-scale ranges set in MPU_Init():
   gyro +/-250 dps -> 131 LSB per deg/s, accel +/-2 g -> 16384 LSB per g. */
#define GYRO_LSB_PER_DPS  131.0f
#define ACCEL_LSB_PER_G   16384.0f

typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
} MPU_Raw;

/* Checks WHO_AM_I, wakes the sensor and sets full scale and low-pass filters.
   Each configuration write is read back. *who receives the WHO_AM_I value. */
HAL_StatusTypeDef MPU_Init(I2C_HandleTypeDef *hi2c, uint8_t *who);
HAL_StatusTypeDef MPU_ReadRaw(I2C_HandleTypeDef *hi2c, MPU_Raw *out);

#endif
