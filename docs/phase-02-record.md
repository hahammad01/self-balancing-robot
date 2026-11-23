# Phase 2 record: IMU over I2C, raw data over UART

**Dates:** 2026-09-13. **Firmware:** [`firmware/phase02_imu`](../firmware/phase02_imu) (the committed
project already contains the Phase 3 filter code; the raw-stream-only version was not committed
separately). **Learning log:** Phase 2 entry.

## Objective
Read the accelerometer and gyroscope over I2C and stream the raw values to a PC.

## Configuration
| Item | Setting |
|------|---------|
| Clock | 16 MHz HSI (CubeMX default) |
| I2C1 | PB8 = SCL, PB7 = SDA, 100 kHz, IMU address 0x68 (AD0 to GND) |
| USART2 | PA2 = TX to an FTDI USB-UART adapter, 115200 baud |
| IMU | Module sold as MPU-6050; `WHO_AM_I` returns 0x70 (MPU-6500) |

PB6 and PB9 are avoided: on this board they connect to the audio DAC's I2C bus (UM1472).

## Implementation
- Read `WHO_AM_I` (0x75); accept 0x68 or 0x70.
- Write `PWR_MGMT_1` (0x6B) = 0x00 to clear the sleep bit.
- Read 6 bytes from 0x3B (accelerometer) and 6 bytes from 0x43 (gyroscope), rebuild each axis as
  a big-endian signed 16-bit value, print over UART.

## Validation
Bench photo, sensor flat and still: [`media/phase02-imu-raw-stream.jpg`](../media/phase02-imu-raw-stream.jpg).

| Quantity | Raw values in the photo | Converted |
|----------|-------------------------|-----------|
| Accel Z | 17 048 to 17 240 LSB | 1.04 to 1.05 g (16 384 LSB/g at +/-2 g) |
| Accel X | -456 to -604 LSB | -0.03 to -0.04 g (sensor slightly tilted) |
| Gyro X | -300 to -382 LSB | -2.3 to -2.9 deg/s (131 LSB per deg/s at +/-250 deg/s) |

Z reading about 1 g at rest and a constant non-zero gyro reading at rest are the expected behaviour
for a stationary IMU. The gyro offset is the bias that Phase 3 calibrates out.

## Problems found
- `huart2` undeclared: USART2 was not enabled in CubeMX.
- `WHO_AM_I` returned 0x70, not 0x68: the chip is an MPU-6500. The registers used in this phase are
  the same on both parts. (The accelerometer low-pass filter is not: on the MPU-6500 it is a separate
  register, 0x1D; this matters from Phase 5.)
- Loop code placed outside the `USER CODE` markers was deleted when CubeMX regenerated the project.

## Limitations
- The full-scale ranges are not written. The conversions above rely on the reset values (+/-2 g,
  +/-250 deg/s). Phase 5 writes them explicitly.
- No raw-data log file was saved for this phase; the evidence is the photo.
- `WHO_AM_I` being accepted does not check the I2C timing margins; the pull-up resistance on the
  IMU module has not been measured.
