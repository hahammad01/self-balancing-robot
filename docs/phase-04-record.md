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

**Re-run with logged intervals.** Pending. The firmware now prints
`time_ms,dir,cmd,count,delta` per sample, with `delta` computed on the board (16-bit wrap handled),
for logging with [`control/log_serial.py`](../control/log_serial.py) and summarising with
[`control/encoder_summary.py`](../control/encoder_summary.py) (mean, standard deviation, minimum and
maximum per direction, spin-up interval reported separately). The motor A pins are the same in the
Phase 5 wiring, so the test runs on the current bench setup; motor B is commanded too and must be
free to turn.

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
