#ifndef TILT_H
#define TILT_H

#include "main.h"

/* Complementary-filter time constant. alpha is computed from it each update as
   alpha = exp(-dt / tau), so the filter behaves the same at any loop period.
   0.75 s matches the Phase 3 filter (alpha = 0.98 at its ~15 ms loop -> tau = 0.74 s). */
#define TILT_TAU_S              0.75f

#define TILT_CAL_SAMPLES        2000U   /* gyro-bias samples, 1 ms apart        */
#define TILT_CAL_MIN_VALID      1900U   /* fewer valid reads -> calibration fails */

typedef struct {
    float    angle;          /* fused roll angle, deg                         */
    float    rate;           /* bias-corrected gyro X rate, deg/s             */
    float    gyro_bias;      /* gyro X offset at rest, raw LSB                */
    uint32_t cal_valid;      /* valid samples used for gyro_bias              */
    uint32_t read_errors;    /* failed IMU reads since start                  */
} Tilt;

/* Averages the gyro X offset with the robot at rest and seeds the angle from the
   accelerometer. Returns HAL_ERROR if fewer than TILT_CAL_MIN_VALID reads succeed
   or the seed read fails; the caller must not run the filter in that case. */
HAL_StatusTypeDef Tilt_Calibrate(Tilt *t, I2C_HandleTypeDef *hi2c);

/* One filter step over dt seconds. On a failed read the angle and rate are held,
   read_errors is incremented and HAL_ERROR is returned. */
HAL_StatusTypeDef Tilt_Update(Tilt *t, I2C_HandleTypeDef *hi2c, float dt);

#endif
