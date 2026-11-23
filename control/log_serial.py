"""
log_serial.py - record a serial stream with a PC timestamp on every line.

    pip install pyserial
    python control/log_serial.py COM4 control/phase05-loop-rate.csv 60

Writes "pc_time_s,<line as received>" for every line, for the given number of seconds
(default 60). The PC timestamp is the independent time reference used by
loop_rate_check.py: the board's own HAL_GetTick() comes from the same clock as the
timer being checked, so it cannot reveal a wrong clock.
"""

import sys
import time

import serial   # pyserial

if len(sys.argv) < 3:
    sys.exit("usage: python log_serial.py <port> <out.csv> [seconds]")

port, out_path = sys.argv[1], sys.argv[2]
duration = float(sys.argv[3]) if len(sys.argv) > 3 else 60.0

with serial.Serial(port, 115200, timeout=1) as ser, open(out_path, "w", newline="") as out:
    ser.reset_input_buffer()
    t0 = time.perf_counter()
    lines = 0
    while time.perf_counter() - t0 < duration:
        raw = ser.readline()
        if not raw:
            continue
        stamp = time.perf_counter() - t0
        text = raw.decode("ascii", errors="replace").strip()
        out.write(f"{stamp:.4f},{text}\n")
        lines += 1
print(f"{lines} lines in {duration:.0f} s -> {out_path}")
