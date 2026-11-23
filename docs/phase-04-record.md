# Phase 4 record: open-loop motor drive and encoder feedback (motor A)

**Dates:** 2026-09-19 to 2026-09-23. **Firmware:** [`firmware/phase04_motors`](../firmware/phase04_motors).
**Learning log:** Phase 4 entry.

## Objective
Drive one geared DC motor in both directions under PWM and read its quadrature encoder.
Motor B was deferred to Phase 5, so only motor A was built and tested here.

## Configuration
| Item | Setting |
|------|---------|
| Clock | HSI 16 MHz, PLL M = 8, N = 50, P = 4: 25 MHz SYSCLK; APB2 / 2 = 12.5 MHz, timer clock 25 MHz |
| PWM | TIM1 CH1 (PE9), PSC = 0, ARR = 999: 25 MHz / 1 / 1000 = **25 kHz** |
| Direction, standby | AIN1 = PE7, AIN2 = PE8, STBY = PE15 (TB6612FNG) |
| Encoder | TIM3 encoder mode TI12, PB4/PB5, ARR = 65535, read as `int16_t` |
| Motor, driver | JGA25-370 geared motor with Hall encoder; TB6612FNG; battery through a barrel jack |
| Test command | +/-300 of 1000 = 30 % duty |

## Implementation
`motorA(int)` maps a signed command to the direction pins and the PWM compare value. The test loop
drives forward for 10 samples about 200 ms apart, stops for 1 s, then repeats in reverse.

## Results

**September run (original firmware).** The committed September code printed the running TIM3
count (`FWD ENC: <count>`), not the change per interval. The reported figures, about +245 counts per
200 ms forward and about -244 in reverse (about 1225 counts/s at 30 % duty), were read off the
terminal by differencing successive counts. The terminal output was not saved, so the per-interval
values and their spread are not available. The commanded direction matched the sign of the count
change in both directions.

**Re-run with logged intervals (23 Nov 2026).** The firmware prints `time_ms,dir,cmd,count,delta`
per sample, with `delta` computed on the board (16-bit wrap handled). Log:
[`control/phase04-encoder-log.csv`](../control/phase04-encoder-log.csv), recorded with
[`control/log_serial.py`](../control/log_serial.py), summarised with
[`control/encoder_summary.py`](../control/encoder_summary.py). Conditions: motor A only, no wheel or
load, 30 % duty, pack 11.6 V before and after, bulk electrolytic capacitor fitted across VM-GND. The first two
lines of the file are from before the reset and are not used.

| | Forward | Reverse |
|---|---:|---:|
| Complete runs | 7 | 7 |
| Steady intervals (first of each run excluded) | 63 | 63 |
| Counts per interval, mean (SD) | +243.5 (0.6) | -241.7 (0.5) |
| Counts per interval, min / max | 242 / 245 | -243 / -241 |
| Interval length | about 203 ms (200 ms delay + UART print) | same |
| Counts per second, mean (SD) | 1200 (3) | -1191 (2) |
| First interval (spin-up) | 215-216 | -214 to -216 |

The September figures (about +245 and -244 per 200 ms) agree with these within 1 %. Using the measured
interval, the rate is about 1200 counts/s forward and 1191 in reverse, not the 1225 counts/s quoted
before, which assumed exactly 200 ms. Reverse is 0.8 % slower than forward. The count direction
followed the command in every interval.

## Problems found
- `sprintf`/`strlen` implicitly declared: `<stdio.h>`/`<string.h>` missing.
- Code pasted between `USER CODE END WHILE` and `USER CODE BEGIN 3` would be lost on regeneration.
- A deleted closing brace in `main()` produced errors reported in a later function.
- The motor ran for a moment and stopped. UART output continued, so the MCU had not reset; the motor
  supply had cut out. What resolved it was not recorded.
- Sampling only at the end of each motion showed a net change of +3 to +6 counts, because forward
  and reverse cancelled; sampling during the motion fixed this.

## Limitations
- Counts per wheel revolution and the gear ratio are not yet measured, so the counts cannot be
  converted to wheel speed.
- The bench photo ([`media/phase04-motor-a-encoder-test.jpg`](../media/phase04-motor-a-encoder-test.jpg))
  shows an earlier hand-rotation check (`ENC:` lines on the terminal), not the forward/reverse run.
- Motor stall current has not been measured against the TB6612FNG limits (1.2 A average, 3.2 A
  peak per channel).
