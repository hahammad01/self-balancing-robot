# Phase 3 record: tilt estimation (complementary and Kalman filters)

**Dates:** 2026-09-14 to 2026-09-15. **Firmware:** [`firmware/phase02_imu`](../firmware/phase02_imu)
(Phase 3 was built inside the Phase 2 project). **Data:** [`control/`](../control).
**Learning log:** Phase 3 entry.

## Objective
Estimate the roll angle from the accelerometer and gyroscope, and compare a complementary filter
with a 2-state Kalman filter on the same samples.

## Implementation
Each loop: read accel and gyro (two blocking I2C reads), compute four angles, print one CSV line
`time_ms, accel, gyro_only, complementary, kalman`, then `HAL_Delay(10)`.

| Estimate | Computation |
|----------|-------------|
| Accel angle | `atan2(ay, az)`, degrees. Valid for roll only while the sensor is not tilted far about its other axis. |
| Gyro rate | `(gx - bias) / 131` deg/s; `bias` = mean of 2000 samples at rest at startup. Not negated (see the sign check below). |
| Gyro-only angle | integral of the gyro rate, for comparison |
| Complementary | `angle = 0.98 (angle + rate dt) + 0.02 accel`. At the logged ~15 ms period this is a time constant of `tau = -dt / ln(0.98) = 0.74 s`. |
| Kalman | 2 states (angle, gyro bias), `Q_angle = 0.001`, `Q_bias = 0.003`, `R_measure = 0.03` in the recorded runs. Structure taken from K. Lauszus, "A practical approach to Kalman filter and how to implement it", TKJ Electronics blog, 2012. |

`dt` is the difference of successive `HAL_GetTick()` values (1 ms resolution).

## Recorded runs
Rates are computed from the logged timestamps.

| Log file | Firmware state | Samples | Duration | Loop rate | Accel range |
|----------|----------------|--------:|---------:|----------:|------------:|
| `(-ve gyrorate)tilt_log.csv` | gyro rate negated; complementary only | 3496 | 52.4 s | 66.7 Hz (15 ms) | -66 to +69 deg |
| `Complementary Filter tilt_log.csv` | committed sign; complementary only | 3429 | 51.4 s | 66.7 Hz (15 ms) | -70 to +63 deg |
| `R_measure = 0.03_tilt_log.csv` | + Kalman, **R = 0.03** | 2533 | 39.7 s | 63.7 Hz (15-16 ms) | -180 to +180 deg |
| `R_measure = 0.1 tilt_log.csv` | + Kalman, **R = 0.03** (see below) | 2859 | 44.9 s | 63.6 Hz (15-16 ms) | -179 to +179 deg |

The loop runs at 15-16 ms, not the 10 ms of `HAL_Delay(10)`: the two blocking I2C reads and the
UART print add 5-6 ms.

## Results

**Gyro sign.** In the run with the rate negated, the gyro-only angle moves opposite to the accel
angle; in the runs with the committed sign, the two move together. The committed (un-negated) sign is
the correct one for this sensor orientation.

**Drift.** At the end of the 51 s complementary-filter run the gyro-only angle differs from the
accel angle by about 4 deg; the complementary angle is within 0.1 deg of the accel angle at that
point.

**The log named "R_measure = 0.1" was recorded with R = 0.03.** The filter inputs can be rebuilt
exactly from the log (accel column, and the gyro increment from the gyro-only column), so the Kalman
filter can be re-run offline ([`control/kalman_replay.py`](../control/kalman_replay.py)):

| Log | Replay with R = 0.03: max difference from logged Kalman | Replay with R = 0.1 |
|-----|------:|------:|
| `R_measure = 0.03_tilt_log.csv` | 0.011 deg | 31.8 deg |
| `R_measure = 0.1 tilt_log.csv` | 0.010 deg | 12.0 deg |

Both runs were therefore recorded with R = 0.03; the most likely cause is that `R_measure` was
edited but the board was not re-flashed before the second run. The two runs compare two different
hand movements, not two values of R.

**R = 0.03 against R = 0.1 on the same input** (offline replay of each logged run):

| Input run | Median difference between the two traces | Largest difference | RMS change per sample, \|accel\| < 60 deg (R = 0.03 / R = 0.1) |
|-----------|------:|------:|------:|
| `R_measure = 0.03_tilt_log.csv` | 0.30 deg | 31.8 deg | 0.835 / 0.736 deg |
| `R_measure = 0.1 tilt_log.csv` | 0.20 deg | 12.0 deg | 0.526 / 0.503 deg |

R = 0.1 is slightly smoother and lags more; the two settings differ mainly during the large
excursions. Figures: `control/kalman-replay-*.png`.

**The 198.5 deg Kalman value.** In the first Kalman run the accel angle stays above 139 deg for
0.75 s (about 19 s into the run) and reaches +/-180 deg during a 2.5 s stretch (about 34-36 s in).
Over the same stretches the gyro-only angle reads about 5 deg and 23-36 deg, so the gyro saw no
matching roll. The most likely cause is that the sensor tipped past 90 deg about its other axis: the
Z component of gravity changed sign and `atan2(ay, az)` moved toward +/-180 deg. The Kalman estimate
followed the accel angle and overshot to 198.5 deg because its angle state is not limited to
+/-180 deg. Wrapping the innovation into [-180, 180) deg in the replay does not remove the excursion,
because the accel angle itself is invalid there. For the robot, the protection is the operating
range: the balance controller (Phase 5) is to cut out at +/-45 deg, well inside the range where the
roll estimate is valid.

## Known issues
- `kalAngle` is initialised before the angle is seeded from the accelerometer, so the Kalman angle
  starts at 0 deg and converges from there.
- The Kalman angle state is not wrapped to +/-180 deg.
- With a fixed `R_measure` the Kalman filter does not reduce its trust in the accelerometer during a
  shake; that would need, for example, `R` raised when the acceleration magnitude is far from 1 g.

## Limitations
- There is no independent angle reference. All four estimates come from the same sensor, so their
  agreement shows the filters behave as intended (less noise, no drift), not that the angle is
  accurate.
- The tilt sweeps were by hand, about +/-60 deg, with no fixture.
