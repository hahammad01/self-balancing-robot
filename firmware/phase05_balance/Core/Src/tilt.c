#include "tilt.h"
#include "mpu6500.h"
#include <math.h>

#define RAD_TO_DEG  57.29578f    /* 180 / pi */

HAL_StatusTypeDef Tilt_Calibrate(Tilt *t, I2C_HandleTypeDef *hi2c)
{
    MPU_Raw  r;
    float    sum   = 0.0f;
    uint32_t valid = 0;

    t->angle       = 0.0f;
    t->rate        = 0.0f;
    t->gyro_bias   = 0.0f;
    t->read_errors = 0;

    /* The robot must not move during this (about 2-4 s). */
    for (uint32_t i = 0; i < TILT_CAL_SAMPLES; i++) {
        if (MPU_ReadRaw(hi2c, &r) == HAL_OK) {
            sum += (float)r.gx;
            valid++;
        } else {
            t->read_errors++;
        }
        HAL_Delay(1);
    }
    t->cal_valid = valid;
    if (valid < TILT_CAL_MIN_VALID)
        return HAL_ERROR;
    t->gyro_bias = sum / (float)valid;

    /* Start from the accelerometer angle instead of 0 deg. */
    if (MPU_ReadRaw(hi2c, &r) != HAL_OK) {
        t->read_errors++;
        return HAL_ERROR;
    }
    t->angle = atan2f((float)r.ay, (float)r.az) * RAD_TO_DEG;
    return HAL_OK;
}

HAL_StatusTypeDef Tilt_Update(Tilt *t, I2C_HandleTypeDef *hi2c, float dt)
{
    MPU_Raw r;

    if (MPU_ReadRaw(hi2c, &r) != HAL_OK) {
        t->read_errors++;
        return HAL_ERROR;        /* angle and rate keep their last values */
    }

    float acc_angle = atan2f((float)r.ay, (float)r.az) * RAD_TO_DEG;

    /* Gyro sign: not negated. In the Phase 3 logs (control/) the gyro-only angle and the
       accelerometer angle move in the same direction with this sign; the run logged with
       the rate negated ("(-ve gyrorate)tilt_log.csv") shows them moving opposite ways.
       This holds for the Phase 3 sensor orientation and must be re-checked once the IMU
       is mounted on the chassis. */
    t->rate = ((float)r.gx - t->gyro_bias) / GYRO_LSB_PER_DPS;

    float alpha = expf(-dt / TILT_TAU_S);
    t->angle = alpha * (t->angle + t->rate * dt) + (1.0f - alpha) * acc_angle;

    return HAL_OK;
}
