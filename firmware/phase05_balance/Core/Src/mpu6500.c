#include "mpu6500.h"

#define MPU_TIMEOUT_MS  5U   /* one register access takes well under 1 ms at 400 kHz */

static HAL_StatusTypeDef write_checked(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val)
{
    uint8_t back = 0;

    if (HAL_I2C_Mem_Write(hi2c, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                          &val, 1, MPU_TIMEOUT_MS) != HAL_OK)
        return HAL_ERROR;
    if (HAL_I2C_Mem_Read(hi2c, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                         &back, 1, MPU_TIMEOUT_MS) != HAL_OK)
        return HAL_ERROR;
    return (back == val) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef MPU_Init(I2C_HandleTypeDef *hi2c, uint8_t *who)
{
    uint8_t id = 0;

    if (HAL_I2C_Mem_Read(hi2c, MPU_ADDR, MPU_WHO_AM_I, I2C_MEMADD_SIZE_8BIT,
                         &id, 1, MPU_TIMEOUT_MS) != HAL_OK)
        return HAL_ERROR;
    if (who) *who = id;

    /* The module was sold as an MPU-6050 but reports 0x70 (MPU-6500). Both are accepted. */
    if (id != MPU_WHOAMI_6050 && id != MPU_WHOAMI_6500)
        return HAL_ERROR;

    if (write_checked(hi2c, MPU_PWR_MGMT_1, 0x00) != HAL_OK)    /* clear SLEEP */
        return HAL_ERROR;
    HAL_Delay(10);

    /* Gyro low-pass: DLPF_CFG = 3 -> 41 Hz bandwidth, 5.9 ms delay (MPU-6500 register map).
       On the MPU-6050 the same code filters both sensors at about 42-44 Hz. */
    if (write_checked(hi2c, MPU_CONFIG, 0x03) != HAL_OK)
        return HAL_ERROR;

    /* Full scale set explicitly instead of relying on the reset value (also 0x00). */
    if (write_checked(hi2c, MPU_GYRO_CONFIG, 0x00) != HAL_OK)   /* +/-250 dps */
        return HAL_ERROR;
    if (write_checked(hi2c, MPU_ACCEL_CONFIG, 0x00) != HAL_OK)  /* +/-2 g */
        return HAL_ERROR;

    /* Accel low-pass is a separate register on the MPU-6500:
       A_DLPF_CFG = 3 -> 44.8 Hz bandwidth, 4.88 ms delay. Not present on the MPU-6050. */
    if (id == MPU_WHOAMI_6500 &&
        write_checked(hi2c, MPU_ACCEL_CONFIG2, 0x03) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

HAL_StatusTypeDef MPU_ReadRaw(I2C_HandleTypeDef *hi2c, MPU_Raw *out)
{
    uint8_t b[14];

    /* One burst 0x3B..0x48: accel (6 bytes), temperature (2), gyro (6). */
    if (HAL_I2C_Mem_Read(hi2c, MPU_ADDR, MPU_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT,
                         b, 14, MPU_TIMEOUT_MS) != HAL_OK)
        return HAL_ERROR;

    out->ax = (int16_t)((b[0]  << 8) | b[1]);
    out->ay = (int16_t)((b[2]  << 8) | b[3]);
    out->az = (int16_t)((b[4]  << 8) | b[5]);
    /* b[6], b[7]: temperature, not used */
    out->gx = (int16_t)((b[8]  << 8) | b[9]);
    out->gy = (int16_t)((b[10] << 8) | b[11]);
    out->gz = (int16_t)((b[12] << 8) | b[13]);

    return HAL_OK;
}
