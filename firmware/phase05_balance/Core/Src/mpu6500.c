#include "mpu6500.h"

HAL_StatusTypeDef MPU_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t who = 0;

    if (HAL_I2C_Mem_Read(hi2c, MPU_ADDR, MPU_WHO_AM_I,
                         I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* 0x68 = MPU6050, 0x70 = the MPU6500 you actually have (Phase 2) */
    if (who != 0x68 && who != 0x70)
        return HAL_ERROR;

    uint8_t wake = 0x00;                /* clear the SLEEP bit */
    return HAL_I2C_Mem_Write(hi2c, MPU_ADDR, MPU_PWR_MGMT_1,
                             I2C_MEMADD_SIZE_8BIT, &wake, 1, 100);
}

HAL_StatusTypeDef MPU_ReadRaw(I2C_HandleTypeDef *hi2c, MPU_Raw *out)
{
    uint8_t b[14];

    /* One burst: 0x3B..0x48 = accel(6) + temp(2) + gyro(6) */
    if (HAL_I2C_Mem_Read(hi2c, MPU_ADDR, MPU_ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, b, 14, 10) != HAL_OK)
        return HAL_ERROR;

    out->ax = (int16_t)((b[0]  << 8) | b[1]);
    out->ay = (int16_t)((b[2]  << 8) | b[3]);
    out->az = (int16_t)((b[4]  << 8) | b[5]);
    /* b[6], b[7] = temperature — read but unused */
    out->gx = (int16_t)((b[8]  << 8) | b[9]);
    out->gy = (int16_t)((b[10] << 8) | b[11]);
    out->gz = (int16_t)((b[12] << 8) | b[13]);

    return HAL_OK;
}
