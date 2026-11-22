#ifndef TILT_H
#define TILT_H

#include "main.h"

typedef struct {
    float angle;        /* fused tilt, degrees */
    float gyro_bias;    /* raw LSB offset measured at rest */
    float alpha;        /* complementary weight */
} Tilt;

void  Tilt_Calibrate(Tilt *t, I2C_HandleTypeDef *hi2c);
float Tilt_Update(Tilt *t, I2C_HandleTypeDef *hi2c, float dt);

#endif
