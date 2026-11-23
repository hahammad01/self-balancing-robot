"""
kalman_replay.py - re-run the Phase 3 Kalman filter offline on a logged run.

The firmware (firmware/phase02_imu/Core/Src/main.c) logs, per loop:
    time_ms, accel_angle, gyro_only_angle, complementary_angle, kalman_angle

The filter inputs can be rebuilt exactly from that log:
    dt          = (time_ms[k] - time_ms[k-1]) / 1000      (the firmware uses the same HAL_GetTick values)
    gyroRate*dt = gyro_only_angle[k] - gyro_only_angle[k-1]
    newAngle    = accel_angle[k]

The script does two things:
  1. Check: replays the log with each candidate R_measure and reports how closely the replay
     reproduces the logged Kalman column. The R that reproduces it identifies the firmware setting
     the run was actually recorded with.
  2. Comparison: runs R_measure = 0.03 and 0.1 on the SAME input, so any difference between the two
     traces is caused by R alone (the two logged runs used different hand movements).

Same arithmetic as the firmware, in float32: Q_angle = 0.001, Q_bias = 0.003, P starts at zero,
bias starts at zero. "wrap" variants wrap the innovation into [-180, 180) deg.

Usage (from the repo root or from control/):
    python control/kalman_replay.py                       # both logged Kalman runs
    python control/kalman_replay.py "path/to/log.csv"     # one log
Writes kalman-replay-<log name>.png next to each log and prints the numbers.
"""

import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve().parent
DEFAULT_LOGS = ["R_measure = 0.03_tilt_log.csv", "R_measure = 0.1 tilt_log.csv"]
R_VALUES = [0.03, 0.1]
Q_ANGLE, Q_BIAS = 0.001, 0.003


def load(path):
    data = np.genfromtxt(path, delimiter=",", invalid_raise=False)
    data = data[~np.isnan(data).any(axis=1)]
    if data.shape[1] < 5:
        raise ValueError(f"{path}: expected 5 columns (time, accel, gyro-only, comp, kalman)")
    return data


def kalman_replay(accel, gyro_increment, dt, r_measure, k0, wrap=False):
    """Two-state Kalman filter, line for line as in the Phase 3 firmware, in float32."""
    f = np.float32
    angle, bias = f(k0), f(0.0)
    P = np.zeros((2, 2), dtype=np.float32)
    out = np.empty(len(accel), dtype=np.float32)
    out[0] = angle
    for k in range(1, len(accel)):
        d = f(dt[k])
        angle = f(angle + (f(gyro_increment[k]) - d * bias))      # predict
        P[0, 0] += d * (d * P[1, 1] - P[0, 1] - P[1, 0] + f(Q_ANGLE))
        P[0, 1] -= d * P[1, 1]
        P[1, 0] -= d * P[1, 1]
        P[1, 1] += f(Q_BIAS) * d
        S = P[0, 0] + f(r_measure)                                 # update
        K0, K1 = P[0, 0] / S, P[1, 0] / S
        y = f(accel[k] - angle)
        if wrap:
            y = f((y + 180.0) % 360.0 - 180.0)
        angle = f(angle + K0 * y)
        bias = f(bias + K1 * y)
        P00, P01 = P[0, 0], P[0, 1]
        P[0, 0] -= K0 * P00
        P[0, 1] -= K0 * P01
        P[1, 0] -= K1 * P00
        P[1, 1] -= K1 * P01
        out[k] = angle
    return out


def analyse(path):
    data = load(path)
    t_ms, accel, gyro_only, kal_logged = data[:, 0], data[:, 1], data[:, 2], data[:, 4]
    dt = np.r_[0.0, np.diff(t_ms)] / 1000.0
    gyro_inc = np.r_[0.0, np.diff(gyro_only)]
    t = (t_ms - t_ms[0]) / 1000.0

    print(f"\n=== {path.name}  ({len(data)} samples, {t[-1]:.1f} s)")

    print("1. Which R_measure reproduces the logged Kalman column?")
    for r in R_VALUES:
        err = np.abs(kalman_replay(accel, gyro_inc, dt, r, kal_logged[0]) - kal_logged)
        print(f"   R = {r:<5}  max |replay - logged| = {err.max():7.3f} deg   "
              f"(99th percentile {np.percentile(err, 99):.3f} deg)")

    print("2. R = 0.03 vs R = 0.1 on this same input:")
    traces = {}
    quiet = np.abs(accel) < 60.0          # sensor within the normal tilt range
    for r in R_VALUES:
        for wrap in (False, True):
            k = kalman_replay(accel, gyro_inc, dt, r, kal_logged[0], wrap)
            traces[(r, wrap)] = k
            step = np.diff(k)[quiet[1:]]
            print(f"   R = {r:<5} wrap = {str(wrap):<5}  peak |angle| = {np.abs(k).max():6.1f} deg   "
                  f"RMS change per sample while |accel| < 60 deg = {np.sqrt(np.mean(step**2)):.3f} deg")
    diff = np.abs(traces[(0.03, False)] - traces[(0.1, False)])
    print(f"   largest difference between the R = 0.03 and R = 0.1 traces: {diff.max():.1f} deg "
          f"(median {np.median(diff):.2f} deg)")

    plt.figure(figsize=(11, 5.5))
    plt.plot(t, accel, color="0.6", linewidth=0.8, label="Accel angle (input)")
    plt.plot(t, traces[(0.03, False)], linewidth=1.6, label="Kalman, R = 0.03")
    plt.plot(t, traces[(0.1, False)], linewidth=1.6, linestyle="--", label="Kalman, R = 0.1")
    plt.xlabel("Time (s)")
    plt.ylabel("Roll angle (deg)")
    plt.title(f"Kalman filter replayed offline on one logged run: {path.stem}")
    plt.grid(alpha=0.3)
    plt.legend()
    plt.tight_layout()
    out = path.with_name("kalman-replay-" + path.stem.replace(" ", "").replace("=", "") + ".png")
    plt.savefig(out, dpi=120)
    plt.close()
    print(f"   figure: {out.name}")


if __name__ == "__main__":
    paths = [Path(p) for p in sys.argv[1:]] or [HERE / name for name in DEFAULT_LOGS]
    for p in paths:
        analyse(p.resolve())
