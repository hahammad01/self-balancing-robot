"""
encoder_summary.py - summarise a Phase 4 encoder log.

Input: a file written by log_serial.py while firmware/phase04_motors is running.
The firmware prints: time_ms,dir,cmd,count,delta  (10 samples per run, ~200 ms apart).

The first sample of each run includes the motor spinning up and is reported separately.
Rates use the board timestamps (time_ms), since the interval is ~200 ms plus the print time.

    python control/encoder_summary.py control/phase04-encoder-log.csv
"""

import sys
import statistics as st

if len(sys.argv) < 2:
    sys.exit("usage: python encoder_summary.py <log from log_serial.py>")

runs, current, last_dir = [], [], None
for line in open(sys.argv[1]):
    parts = line.strip().split(",")
    if len(parts) != 6 or parts[2] not in ("FWD", "REV"):
        continue                                    # header or partial line
    _, t_ms, direction, cmd, count, delta = parts
    if direction != last_dir and current:
        runs.append((last_dir, current))
        current = []
    current.append((int(t_ms), int(delta)))
    last_dir = direction
if current:
    runs.append((last_dir, current))

for direction in ("FWD", "REV"):
    deltas, rates, firsts = [], [], []
    for d, samples in runs:
        if d != direction or len(samples) < 10:
            continue                                # skip a run cut off by the log window
        firsts.append(samples[0][1])
        for (t_prev, _), (t, delta) in zip(samples, samples[1:]):
            deltas.append(delta)
            rates.append(delta / ((t - t_prev) / 1000.0))
    if not deltas:
        print(f"{direction}: no complete runs")
        continue
    print(f"{direction}: {len(firsts)} runs, {len(deltas)} steady intervals")
    print(f"   counts per interval : mean {st.mean(deltas):.1f}, SD {st.stdev(deltas):.1f}, "
          f"min {min(deltas)}, max {max(deltas)}")
    print(f"   counts per second   : mean {st.mean(rates):.0f}, SD {st.stdev(rates):.0f}")
    print(f"   first interval (spin-up) : {firsts}")
