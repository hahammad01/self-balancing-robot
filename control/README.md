# control/ — logged data and analysis scripts

## Logged runs (Phase 3, firmware `firmware/phase02_imu`)

CSV columns: `time_ms, accel_angle, gyro_only_angle, complementary_angle[, kalman_angle]` (degrees).
File names are as saved on the day and are kept unchanged.

| File | Firmware state when recorded | Plot |
|------|------------------------------|------|
| `(-ve gyrorate)tilt_log.csv` | gyro rate negated; complementary filter only | `(-ve gyrorate)tilt_plot.png` |
| `Complementary Filter tilt_log.csv` | committed gyro sign; complementary filter only | `Complementary Filter tilt_plot.png` |
| `R_measure = 0.03_tilt_log.csv` | + Kalman filter, `R_measure = 0.03` | `R_measure = 0.03_tilt_plot.png` |
| `R_measure = 0.1 tilt_log.csv` | + Kalman filter, **`R_measure = 0.03`**, despite the file name: the offline replay reproduces its Kalman column with R = 0.03 (max error 0.010 deg) and not with R = 0.1 (max error 12 deg) | `R_measure = 0.1 _tilt_plot.png` |

`kalman-replay-*.png`: both R values run offline on the same input; see `kalman_replay.py` and
[`docs/phase-03-record.md`](../docs/phase-03-record.md).

## Logged runs (Phases 4 and 5)

| File | Content |
|------|---------|
| `phase04-encoder-log.csv` | 23 Nov 2026, firmware `phase04_motors`, motor A only, no load, 30 % duty, 11.6 V, bulk capacitor fitted. `pc_time_s,time_ms,dir,cmd,count,delta`. First two lines are from before the reset. Result: `encoder_summary.py`. |
| `phase05-loop-rate.csv` | 23 Nov 2026, USB power, IMU + FTDI, no motors. `pc_time_s` then the board line: `time_ms,angle_deg,rate_dps` about every 100 ms, `#S` statistics once per second. First 4.8 s are from the previous boot. Result: 199.89 Hz, 0 missed ticks (`loop_rate_check.py`). |

## Scripts

| Script | Use |
|--------|-----|
| `plot_tilt.py` | Plot one Phase 3 log: `python control/plot_tilt.py "control/<log>.csv"` |
| `kalman_replay.py` | Re-run the Phase 3 Kalman filter offline on a logged run; identifies the R a run was recorded with, and compares R values on identical input |
| `log_serial.py` | Record a serial stream with a PC timestamp per line (`pip install pyserial`) |
| `loop_rate_check.py` | Phase 5: tick rate against the PC clock, missed ticks, execution time, from a `log_serial.py` file |
| `encoder_summary.py` | Phase 4: counts per interval and per second (mean, SD, min, max) from a `log_serial.py` file |

For new datasets, record alongside the file: date, firmware folder and commit, and any setting that
changed (gains, filter constants, supply voltage).
