"""
encoder_summary.py - summarise a Phase 4 encoder log.

Input: a file written by log_serial.py while firmware/phase04_motors is running.
The firmware prints: time_ms,dir,cmd,count,delta  (10 samples per run, ~200 ms apart).

The first sample of each run includes the motor spinning up and is reported separately.
Only complete runs (10 samples) are used; lines from before a reset are ignored.
Rates use the board timestamps (time_ms), since the interval is ~200 ms plus the print time.

    python control/encoder_summary.py control/phase04-encoder-log.csv
"""

import sys
import statistics as st

if len(sys.argv) < 2:
    sys.exit("usage: python encoder_summary.py <log from log_serial.py>")

# A run ends when the direction changes, when the board time goes backwards (a reset), or when
# the gap to the previous sample is too long for one run (the 1 s stop between runs).
runs, current, last_dir, last_t = [], [], None, None
for line in open(sys.argv[1]):
    parts = line.strip().split(",")
    if len(parts) != 6 or parts[2] not in ("FWD", "REV"):
        continue                                    # header or partial line
    _, t_ms, direction, cmd, count, delta = parts
    t = int(t_ms)
    new_run = (direction != last_dir or last_t is None or t < last_t or t - last_t > 400)
    if new_run and current:
        runs.append((last_dir, current))
        current = []
    current.append((t, int(delta)))
    last_dir, last_t = direction, t
if current:
    runs.append((last_dir, current))

for direction in ("FWD", "REV"):
    deltas, rates, firsts = [], [], []
    for d, samples in runs:
        if d != direction or len(samples) != 10:
            continue                                # skip runs cut off by the log window or a reset
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
