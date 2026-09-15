"""
plot_tilt.py — Phase 3 results chart for the self-balancing robot.

Plots accel-only vs gyro-only vs complementary (and Kalman, if present) tilt angles
logged from the STM32 over serial.

CSV formats it understands (comma-separated, header text lines ignored automatically):
    [time_ms,] accel_angle, gyro_angle, complementary_angle [, kalman_angle]

If a leading millisecond-timestamp column is present (recommended — print HAL_GetTick()
as the first column in firmware), the x-axis uses REAL elapsed time. Without it, the
script falls back to assuming 100 Hz and warns you (that assumption is why an old log
looked like only ~4 s when it actually ran ~25 s).

Usage:
    pip install numpy matplotlib
    python plot_tilt.py                 # reads tilt_log.csv next to this file
    python plot_tilt.py mylog.csv
"""

import sys
import numpy as np
import matplotlib.pyplot as plt

filename = sys.argv[1] if len(sys.argv) > 1 else "tilt_log.csv"

data = np.genfromtxt(filename, delimiter=",", invalid_raise=False)
data = data[~np.isnan(data).any(axis=1)]        # drop any non-numeric / partial rows
if data.ndim == 1:
    data = data.reshape(1, -1)

n_cols = data.shape[1]
col0 = data[:, 0]

# A real time column (ms) is monotonic non-decreasing and spans a meaningful range.
# This also distinguishes [time,a,g,c] from [a,g,c,kalman] (accel is never monotonic).
has_time = (n_cols >= 4) and np.all(np.diff(col0) >= 0) and (col0.max() - col0.min() > 100)

if has_time:
    t = (col0 - col0[0]) / 1000.0               # ms -> s, start at 0
    angles = data[:, 1:]
    dur = t[-1] if t[-1] > 0 else 1.0
    print(f"Using logged timestamps: {t[-1]:.1f} s, {len(t)} samples "
          f"(~{len(t)/dur:.0f} Hz real loop rate)")
else:
    dt = 0.01
    t = np.arange(data.shape[0]) * dt
    angles = data
    print(f"WARNING: no timestamp column found -> assuming {1/dt:.0f} Hz, so the time "
          f"axis is only approximate. Add a millis column (HAL_GetTick) in firmware "
          f"for a true time axis.")

labels = ["Accel only (noisy, no drift)", "Gyro only (smooth, drifts)",
          "Complementary (fused)", "Kalman (fused)"]
styles = [dict(alpha=0.45, linewidth=1),
          dict(linewidth=1.5),
          dict(linewidth=2.2, color="black"),
          dict(linewidth=2.0, linestyle="--", color="crimson")]

plt.figure(figsize=(11, 6))
for i in range(angles.shape[1]):
    plt.plot(t, angles[:, i],
             label=labels[i] if i < len(labels) else f"col{i}",
             **(styles[i] if i < len(styles) else {}))

plt.xlabel("Time (s)")
plt.ylabel("Roll angle (deg)")
plt.title("Phase 3 — Tilt estimation: sensor fusion")
plt.grid(True, alpha=0.3)
plt.legend(loc="best")
plt.tight_layout()
plt.savefig("tilt_plot.png", dpi=150)
print("Saved tilt_plot.png")
plt.show()
