#ifndef MOTORS_H
#define MOTORS_H

#include <stdint.h>

#define PWM_MAX 999

void Motors_Init(void);              /* start PWM, stop motors, enable driver */
void Motors_Enable(uint8_t on);      /* STBY high/low */
void Motors_Stop(void);
void Motor_A_Set(int16_t speed);     /* -PWM_MAX..+PWM_MAX, +ve = robot forward */
void Motor_B_Set(int16_t speed);     /* -PWM_MAX..+PWM_MAX, +ve = robot forward */

#endif
