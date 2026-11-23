"""
loop_rate_check.py - check the Phase 5 control tick against the PC clock.

Input: a file written by log_serial.py while firmware/phase05_balance is running.
The firmware prints once per second:
    #S,ticks_total,ticks_missed,work_max_us,loop_max_us,imu_read_errors

Rate = (ticks_total[last] - ticks_total[first]) / (pc_time[last] - pc_time[first]).
Over 60 s, the PC timestamping jitter (a few ms) is below 0.01 % of the window.

    python control/loop_rate_check.py control/phase05-loop-rate.csv
"""

import sys

if len(sys.argv) < 2:
    sys.exit("usage: python loop_rate_check.py <log from log_serial.py>")

rows = []
for line in open(sys.argv[1]):
    pc, _, text = line.strip().partition(",")
    if text.startswith("#S,"):
        f = text.split(",")[1:]
        rows.append((float(pc), *(int(x) for x in f)))

if len(rows) < 2:
    sys.exit("fewer than two #S lines in the log")

pc0, ticks0, missed0 = rows[0][0], rows[0][1], rows[0][2]
pc1, ticks1, missed1 = rows[-1][0], rows[-1][1], rows[-1][2]
rate = (ticks1 - ticks0) / (pc1 - pc0)

print(f"window            : {pc1 - pc0:.1f} s ({len(rows)} stats lines)")
print(f"tick rate (PC)    : {rate:.2f} Hz  (target 200 Hz, error {100 * (rate - 200) / 200:+.3f} %)")
print(f"missed ticks      : {missed1 - missed0} in the window")
print(f"work max (I2C+filter) : {max(r[3] for r in rows)} us  (budget 5000 us)")
print(f"loop max (incl. UART) : {max(r[4] for r in rows)} us")
print(f"IMU read errors   : {rows[-1][5] - rows[0][5]} in the window")
