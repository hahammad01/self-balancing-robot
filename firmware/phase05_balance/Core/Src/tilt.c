#include "tilt.h"
#include "mpu6500.h"
#include <math.h>

#define GYRO_LSB_PER_DPS   131.0f
#define RAD_TO_DEG          57.29578f    /* 180 / pi */

void Tilt_Calibrate(Tilt *t, I2C_HandleTypeDef *hi2c)
{
    MPU_Raw r;
    float sum = 0.0f;
    const int N = 2000;

    /* Robot must be COMPLETELY STILL for this (~2 s). */
    for (int i = 0; i < N; i++) {
        if (MPU_ReadRaw(hi2c, &r) == HAL_OK) sum += (float)r.gx;
        HAL_Delay(1);
    }
    t->gyro_bias = sum / (float)N;

    /* Seed the filter from the accelerometer so it starts at the true
       angle instead of ramping up from zero over the first few seconds. */
    if (MPU_ReadRaw(hi2c, &r) == HAL_OK)
        t->angle = atan2f((float)r.ay, (float)r.az) * RAD_TO_DEG;
    else
        t->angle = 0.0f;

    t->alpha = 0.98f;
}

float Tilt_Update(Tilt *t, I2C_HandleTypeDef *hi2c, float dt)
{
    MPU_Raw r;

    if (MPU_ReadRaw(hi2c, &r) != HAL_OK)
        return t->angle;                 /* I2C hiccup: hold last good value */

    float acc_angle = atan2f((float)r.ay, (float)r.az) * RAD_TO_DEG;

    /* NO negation — verified 30 Oct 2026 against your committed Phase 3
       code and its logged CSVs. Earlier revisions of this guide negated
       this term; that was WRONG. Read the correction note below before
       you touch this line. */
    float gyro_rate = ((float)r.gx - t->gyro_bias) / GYRO_LSB_PER_DPS;

    t->angle = t->alpha * (t->angle + gyro_rate * dt)
             + (1.0f - t->alpha) * acc_angle;

    return t->angle;
}
