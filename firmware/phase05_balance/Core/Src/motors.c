#include "motors.h"
#include "main.h"     /* pin macros: AIN1_Pin, AIN1_GPIO_Port, STBY_Pin, ... */
#include "tim.h"      /* declares htim1 */

/* ---- Mirror sign convention ----------------------------------------------
 * The two motors are mounted facing opposite directions, so identical
 * electrical commands drive the robot opposite ways. Exactly ONE motor gets
 * negated, so the rest of the firmware deals in one clean convention:
 * positive = robot moves forward.
 *
 * Part 2.3 tells you which motor. Set these to +1 / -1 accordingly.
 * -------------------------------------------------------------------------*/
#define MOTOR_A_SIGN  (+1)
#define MOTOR_B_SIGN  (-1)

static void drive(int16_t speed, int8_t sign,
                  GPIO_TypeDef *port1, uint16_t in1,
                  GPIO_TypeDef *port2, uint16_t in2,
                  uint32_t channel)
{
    int32_t s = (int32_t)speed * (int32_t)sign;

    if (s >= 0) {
        HAL_GPIO_WritePin(port1, in1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(port2, in2, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(port1, in1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(port2, in2, GPIO_PIN_SET);
        s = -s;
    }

    if (s > PWM_MAX) s = PWM_MAX;
    __HAL_TIM_SET_COMPARE(&htim1, channel, (uint32_t)s);
}

void Motor_A_Set(int16_t speed)
{
    drive(speed, MOTOR_A_SIGN,
          AIN1_GPIO_Port, AIN1_Pin,
          AIN2_GPIO_Port, AIN2_Pin,
          TIM_CHANNEL_1);
}

void Motor_B_Set(int16_t speed)
{
    drive(speed, MOTOR_B_SIGN,
          BIN1_GPIO_Port, BIN1_Pin,
          BIN2_GPIO_Port, BIN2_Pin,
          TIM_CHANNEL_2);
}

void Motors_Stop(void)
{
    Motor_A_Set(0);
    Motor_B_Set(0);
}

void Motors_Enable(uint8_t on)
{
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Motors_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   /* PWMA — PE9  */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);   /* PWMB — PE11 */
    Motors_Stop();
    Motors_Enable(1);
}
