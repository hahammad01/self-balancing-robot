# Phase 5 record: second motor, sensor and 200 Hz loop (in progress)

**Started:** 2026-09-26. **Firmware:** [`firmware/phase05_balance`](../firmware/phase05_balance).
**Learning log:** Phase 5 entries. **Status:** no controller yet; the robot is not assembled; motors are
held stopped by the firmware.

## Objective
Run the IMU and tilt filter at a fixed 200 Hz rate with both motors and encoders available, as the
base for the balance controller.

## Configuration (CubeMX)
| Item | Setting |
|------|---------|
| Clock | HSI 16 MHz, PLL M = 8, N = 168, P = 2: **168 MHz** SYSCLK; APB1 / 4 (timers 84 MHz), APB2 / 2 (timers 168 MHz) |
| PWM | TIM1 CH1/CH2 (PE9/PE11), PSC = 7, ARR = 999: 168 MHz / 8 / 1000 = **21 kHz** |
| Encoders | TIM3 (motor A, PB4/PB5) and TIM2 (motor B, PA15/PA1), mode TI12, ARR = 65535, input filter 10 |
| Control tick | TIM6, PSC = 8399, ARR = 49: 84 MHz / 8400 / 50 = **200 Hz**, update interrupt enabled |
| I2C1 | PB8/PB7, 400 kHz |
| USART2 | 115200 baud; `printf` float support enabled (newlib-nano) |
| IMU registers (set in `MPU_Init`) | `CONFIG` 0x1A = 3: gyro low-pass 41 Hz, 5.9 ms delay; `ACCEL_CONFIG2` 0x1D = 3: accel low-pass 44.8 Hz, 4.88 ms delay (MPU-6500 only); +/-250 deg/s, +/-2 g written explicitly. Each write is read back. |

## Implementation
- **Tick handling.** The TIM6 interrupt increments a counter; the main loop takes all pending ticks
  at once with interrupts briefly disabled. If more than one tick is pending, the extra ticks are
  counted as missed, and the filter integrates over the elapsed ticks (`dt = n x 5 ms`) rather than a
  fixed 5 ms.
- **Tilt filter.** Complementary filter with a time constant `tau = 0.75 s`, the same as the Phase 3
  filter (alpha = 0.98 at about 15 ms). `alpha = exp(-dt / tau)` is computed on every update, so the
  filter behaves the same at any loop period. Gyro sign not negated, as in Phase 3; to be re-checked
  once the IMU is mounted on the chassis.
- **Calibration.** 2000 gyro samples at rest; the bias is averaged over the valid reads only, and
  calibration fails if fewer than 1900 succeed.
- **Failure handling.** IMU not found, calibration failure, or 5 consecutive failed reads: motor
  driver to standby, reason printed, red LED blinks. `Error_Handler` and `HardFault_Handler` set the
  driver to standby first.
- **Motors.** `motors.c` has one drive helper with `MOTOR_A_SIGN = +1`, `MOTOR_B_SIGN = -1`. These
  signs are not yet verified (see below).
- **Output.** `time_ms,angle_deg,rate_dps` about every 100 ms; once per second a statistics line
  `#S,ticks_total,ticks_missed,work_max_us,loop_max_us,imu_read_errors`, timed with the DWT cycle
  counter. The two lines are scheduled half a telemetry period apart, so they are not normally
  sent in the same pass (together they would take about 4.5 ms of the 5 ms tick at 115200 baud).

## Verification
| Check | Method | Status |
|-------|--------|--------|
| Firmware builds | CubeIDE from the repo path | built and flashed 23 Nov 2026 |
| Tick rate | `#S` tick totals against the PC clock: [`control/log_serial.py`](../control/log_serial.py) then [`control/loop_rate_check.py`](../control/loop_rate_check.py). The board's own `HAL_GetTick()` cannot be used: it runs from the same clock. | **199.89 Hz** over 64 s (-0.056 %), 23 Nov 2026 |
| Missed deadlines, execution time | `ticks_missed`, `work_max_us`, `loop_max_us` from the same log, telemetry on | **0 missed**; IMU read + filter max **400 us**; whole pass incl. UART max **2432 us** of the 5000 us tick; 0 IMU read errors |
| Motor and encoder signs | Wheels off the ground: command a small positive value on each motor and watch the wheel; then set the encoder signs. An encoder count alone cannot verify a sign, because it shows the product of the drive and encoder polarities. | pending |

## Result: loop timing (23 Nov 2026)
Log: [`control/phase05-loop-rate.csv`](../control/phase05-loop-rate.csv) (USB power, IMU and FTDI connected,
no motors). The first 4.8 s of the file are from the previous boot; the board was reset at
4.85 s (PC time), and the line `# WHO_AM_I=0x70 gyro_bias=-324.8 LSB (2000 valid) tau=0.75 s`
follows.

| Quantity | Value |
|----------|-------|
| Tick rate against the PC clock | 199.89 Hz over 64 s (65 statistics lines): -0.056 % |
| Missed ticks | 0 |
| Longest IMU read + filter step | 400 us |
| Longest pass including UART output | 2432 us (budget 5000 us) |
| IMU read errors | 0 |
| Gyro X bias at rest | -324.8 LSB = -2.48 deg/s |
| Angle at rest, 28 s after calibration | mean 0.12 deg, SD 0.02 deg |
| Angle range during the hand tilts | -31.9 to +67.6 deg |

The -0.056 % rate error is the internal RC oscillator (HSI), which the board runs on; ST specifies
it to +/-1 % at 25 deg C. The loop uses less than half of each tick with telemetry on.

## Earlier result (September)
With `MOTOR_B_SIGN = -1`, both encoders counted up when both motors were commanded forward. For the
reason above, this does not verify the signs. No readings from this test were saved.
Photo: [`media/phase05-two-motor-bench-wiring.jpg`](../media/phase05-two-motor-bench-wiring.jpg).

## Not done yet
PID controller, arming, tilt cut-out, robot assembly, balance.
