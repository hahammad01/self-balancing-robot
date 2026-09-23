# Phase 3 — Tilt Estimation: turning raw IMU data into a clean angle (with the *why*)

**Goal:** convert the noisy accelerometer + drifting gyroscope readings from Phase 2 into **one clean,
stable tilt angle** — the single number your robot will balance on — using a **complementary filter**
(then, as an upgrade, a **Kalman filter**). Then **log it and plot it** to produce your first real
results chart.

**Why this is the crux:** a balancing robot lives or dies by how good its angle estimate is. Feed it
noise and the motors jitter; feed it a drifting angle and it slowly falls over. Everything in Phases
5–6 assumes you can hand the controller a trustworthy angle. This phase builds that.

**Format:** every step is **Do / Why / Under the hood**. The math is explained from zero — if a line
isn't clear, that's a bug in the guide, flag it.

---

# Part 0 — The concepts (this is 70% of the phase; read it slowly)

### 0.1 The problem, restated with what you saw

In Phase 2 you observed two things directly:
- The **accelerometer** reads gravity, so it always knows "which way is down" → an **absolute** angle.
  But it was **noisy** (jumping ±0.009 g), and worse, it can't tell gravity from real acceleration —
  shake or accelerate the robot and it lies.
- The **gyroscope** was **smooth**, but it had that **-2.6 °/s bias at rest**. A gyro only measures
  *rate* of rotation, so to get an angle you must **integrate** it over time — and integrating a
  constant bias means the angle **drifts** away from truth, forever.

So: accel = correct on average but noisy and fooled by motion. Gyro = smooth and motion-immune but
drifts. **Neither alone is usable.** Fusion combines their strengths.

### 0.2 Getting an angle from the accelerometer

When the sensor is still, the accelerometer measures the gravity vector split across its axes. The
**tilt angle is just the geometry of that vector.** For rotation about the X axis (call it **roll**),
gravity moves between the Y and Z axes, so:

```
roll_from_accel = atan2(ay, az)      (in radians; ×180/π for degrees)
```

**Why `atan2` and not plain division:** `atan2(y, x)` returns the correct angle in *all four
quadrants* (it knows the sign of both inputs), so it works through the full ±180°, unlike `atan(y/x)`
which breaks at 90°. **Under the hood — a neat bonus:** because it uses the *ratio* `ay/az`, the
sensor's scale factor (16384 counts/g) **cancels out** — you can feed it raw counts directly, no
conversion to g needed. Flat sensor: `ay≈0, az≈+`, so `atan2 ≈ 0°`. Tilt 90°: gravity moves fully
onto Y, `atan2 ≈ 90°`.

### 0.3 Getting an angle from the gyroscope

The gyro gives **angular rate** (°/s) about an axis. To get an angle, you **integrate** — add up
rate × time each loop:

```
angle_gyro += rate * dt
```

where `dt` is the time since the last loop, in seconds. **Under the hood — why it drifts:** if the
true rate is 0 but the gyro reads a bias of -2.6 °/s, then every second you add -2.6° of *fake*
rotation. Over 10 s that's -26° of pure error. **This is the drift**, and it's exactly why we first
**calibrate the bias** (measure it at rest, subtract it) and then **fuse with the accelerometer** to
cancel whatever bias remains.

### 0.4 The complementary filter (the core idea)

Here's the elegant trick. Each loop, blend the two estimates with a weighted average:

```
angle = alpha * (angle + gyro_rate * dt)  +  (1 - alpha) * angle_from_accel
        └─────── trust the gyro short-term ───────┘      └─ trust the accel long-term ─┘
```

with `alpha` close to 1 (typically **0.98**). Read it in two halves:
- **`alpha * (angle + gyro_rate*dt)`** — take the *previous* fused angle, advance it with the gyro,
  and weight it 98%. This makes the output **smooth** and follows fast motion (the gyro's strength),
  while the 98% weight means accel noise barely leaks in.
- **`(1-alpha) * angle_from_accel`** — gently pull 2% toward the accelerometer's absolute angle every
  loop. Over many loops this **cancels the gyro's drift** (the accel has no drift), without letting
  the accel's noise dominate.

**Why it's called "complementary":** the two weights sum to 1, and mathematically this is a
**high-pass filter on the gyro** (keeps fast changes, discards slow drift) plus a **low-pass filter on
the accel** (keeps the slow true angle, discards fast noise). The two filters are frequency-
complementary — each covers the band the other is bad at. One line of code, and you get the best of
both sensors.

**What `alpha` controls:** higher alpha → smoother but slower to correct drift (trusts gyro more);
lower alpha → snappier correction but more accel noise gets through. 0.98 is a good start.

### 0.5 `dt` — why consistent timing matters, and how we get it

Integration (`rate * dt`) is only correct if `dt` is the *actual* elapsed time. We measure it with
**`HAL_GetTick()`**, a millisecond counter the HAL runs for you (the SysTick from Phase 1):

```
now = HAL_GetTick();  dt = (now - lastTick) / 1000.0;  lastTick = now;
```

**Under the hood:** `HAL_GetTick()` returns milliseconds since boot. Subtracting the previous reading
gives the true loop time, so even if a loop takes 10 or 12 ms, your integration stays correct. (In
Phase 5 we'll go further and run the loop from a hardware **timer interrupt** for a *rock-steady* dt —
essential for control — but `HAL_GetTick` is perfect for Phase 3.)

### 0.6 Gyro bias calibration

Before the loop, we measure the gyro's resting bias and subtract it forever after: hold the sensor
**still**, average a couple thousand samples, and that average *is* the bias (since the true rate is
0). **Why:** it removes most of the drift up front, so the complementary filter only has to mop up a
little. This directly uses the -2.6 °/s you measured in Phase 2.

---

# Part 1 — Project setup (reuse Phase 2 — here's why)

**Do:** open your **`phase02_imu`** project and work in it. **No CubeMX changes needed.**

**Why reuse instead of a new project (a real principle):** Phase 2 needed *new hardware peripherals*
(I2C, UART), so a fresh clean project made sense. **Phase 3 adds no new hardware** — it's pure math on
the same sensor data over the same I2C/UART you already configured. Making a new project would only
re-introduce config risk (wrong pins, forgetting float-printf) for zero benefit. **Rule of thumb: new
project when you add hardware/peripherals; reuse when you're only changing software.** Each phase is
still its own git commit, which is what actually documents your progress.

**Two prerequisites in that project:**
1. **`<math.h>`** for `atan2f`. Add it in `USER CODE BEGIN Includes` (alongside stdio/string).
2. **Float printf must be ON** (we print decimal angles). If you did the Phase 2 stretch it's already
   on. If not: **Project → Properties → C/C++ Build → Settings → Tool Settings → MCU Settings → tick
   "Use float with printf" → Apply → rebuild.** (Without it, every angle prints blank/garbage.)

---

# Part 2 — The code

We'll replace your Phase 2 loop with the fusion code. Keep the WHO_AM_I check and wake from Phase 2.

### 2a. Includes and a constant
`USER CODE BEGIN Includes`:
```c
#include <stdio.h>
#include <string.h>
#include <math.h>
```
`USER CODE BEGIN PD` (add near your MPU defines):
```c
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
```
**Why:** `atan2f` lives in `math.h`. `M_PI` converts radians→degrees; the guard defines it only if the
library didn't.

### 2b. Setup + bias calibration — in `USER CODE BEGIN 2`
**Declare the persistent variables at the TOP of `USER CODE BEGIN 2`, BEFORE the
`if (who == 0x68 || who == 0x70)` block — NOT inside it.** (C **scope** rule: a variable declared
inside `{ }` only exists inside those braces. If you declare `roll` / `buf` / `gxBias` / `lastTick` /
`gyroAngle` inside the `if`, the `while(1)` loop below can't see them → "undeclared" build errors.)
The **declarations** go before the `if`; the **calibration + seeding** go inside the `if` (they should
only run when the sensor is actually found). The full correct shape:
```c
  uint8_t buf[6];
  float roll = 0.0f;        // the fused angle we care about (degrees)
  float gyroAngle = 0.0f;   // gyro-ONLY angle, kept for comparison (it will drift)
  float gxBias = 0.0f;      // measured gyro-X resting bias (raw counts)
  uint32_t lastTick;

  // --- Calibrate gyro bias: KEEP THE SENSOR STILL for ~2 seconds now ---
  HAL_UART_Transmit(&huart2, (uint8_t*)"Calibrating - keep still...\r\n", 29, 100);
  int32_t sum = 0;
  for (int i = 0; i < 2000; i++) {
      HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_GYRO_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
      sum += (int16_t)((buf[0] << 8) | buf[1]);   // accumulate raw gyro-X
      HAL_Delay(1);
  }
  gxBias = sum / 2000.0f;                          // average = the bias

  // --- Seed the fused angle from the accelerometer so it doesn't start at 0 and ramp ---
  HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
  int16_t ay0 = (int16_t)((buf[2] << 8) | buf[3]);
  int16_t az0 = (int16_t)((buf[4] << 8) | buf[5]);
  roll = atan2f((float)ay0, (float)az0) * 180.0f / M_PI;
  gyroAngle = roll;
  lastTick = HAL_GetTick();
```
**Why:** the loop averages 2000 still samples to find the bias, seeds the fused angle with the
accel's current angle (so it starts correct, not at 0), and records the start time for `dt`.

### 2c. The fusion loop — in `USER CODE BEGIN 3`
```c
    // 1) Read accel (for the absolute angle) and gyro-X (for the rate)
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
    int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4] << 8) | buf[5]);
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_GYRO_XOUT_H,  I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
    int16_t gx = (int16_t)((buf[0] << 8) | buf[1]);

    // 2) Real elapsed time this loop, in seconds
    uint32_t now = HAL_GetTick();
    float dt = (now - lastTick) / 1000.0f;
    lastTick = now;

    // 3) Angle from accelerometer (degrees) — absolute, no drift, but noisy
    float rollAcc = atan2f((float)ay, (float)az) * 180.0f / M_PI;

    // 4) Angular rate from gyro (deg/s), bias removed
    float gyroRate = (gx - gxBias) / 131.0f;

    // 5) Gyro-ONLY integrated angle (comparison line — watch it drift)
    gyroAngle += gyroRate * dt;

    // 6) COMPLEMENTARY FILTER: blend gyro (short term) + accel (long term)
    float alpha = 0.98f;
    roll = alpha * (roll + gyroRate * dt) + (1.0f - alpha) * rollAcc;

    // 7) Print as CSV: accel-only, gyro-only, fused
    char line[64];
    sprintf(line, "%.2f,%.2f,%.2f\r\n", rollAcc, gyroAngle, roll);
    HAL_UART_Transmit(&huart2, (uint8_t*)line, strlen(line), 100);

    HAL_Delay(10);   // ~100 Hz loop
```
**Under the hood, line by line:** (3) turns the gravity split into an absolute angle; (4) converts the
gyro to °/s and removes the bias you measured; (5) integrates the gyro *alone* so you can SEE drift in
the plot; (6) is the whole magic — one line that fuses them; (7) prints three comparable angles as
comma-separated values so a plot script can read them.

**Build, flash, and open Tera Term (115200).** Hold the sensor still during "Calibrating…", then tilt
it: all three numbers should read ~0° flat, and ~±90° on its side. The third column (fused) should be
**both smooth and correct**.

---

# Part 3 — Log it and plot your first results chart

Numbers scrolling by don't prove anything. A plot does. 

### 3a. Capture the serial stream to a file
In **Tera Term: File → Log…**, choose a filename like **`tilt_log.csv`** in a folder you'll find
(e.g. your repo's `control/` folder), untick "Timestamp", click Save. Now everything printed is being
saved. **Do a scripted motion for ~30 s:** hold still, tilt one way ~45°, hold, tilt the other way,
give it a couple of quick shakes, return to flat. Then **File → Log → Close/Stop**.

### 3b. Plot it in Python
You'll need Python with numpy + matplotlib (`pip install numpy matplotlib`). Use the `plot_tilt.py`
script I've given you (in `control/`). Put `tilt_log.csv` next to it and run:
```
python plot_tilt.py
```
It saves `tilt_plot.png` and shows the chart.

**What you should see (and why it's the payoff):**
- **Accel-only** line: correct on average but **jagged/noisy**.
- **Gyro-only** line: smooth but **slowly sliding away** from the others — that's the drift, live.
- **Complementary** line: **smooth AND tracks the true angle** — noise gone, drift gone.

That single picture *is* the proof your filter works, and it's exactly the kind of quantitative figure
that belongs in your portfolio and report.

---

# Part 4 — (Upgrade) The Kalman filter

The complementary filter is great and, honestly, is what most balancing robots actually run. The
**Kalman filter** is the "optimal" version and is what makes this project master's-grade, so it's
worth adding and understanding — even if the full matrix derivation is a topic for later.

**The intuition (this is what matters):** a Kalman filter keeps not just an angle estimate but a
measure of *how uncertain* it is. Each loop it does two steps:
1. **Predict** — advance the angle using the gyro, and *grow* the uncertainty (gyro drifts, so we
   trust it less over time).
2. **Update** — compare against the accel measurement and correct, by an amount set by the **Kalman
   gain**, which *optimally* weighs "how noisy is the gyro model" vs "how noisy is the accel." When
   the accel is trustworthy it corrects more; when it's being shaken it corrects less.

Its key advantage over the complementary filter: it **estimates the gyro bias as a second state and
tracks it live**, so it adapts as the bias changes with temperature — the complementary filter can't.

**A proven implementation** (the widely-used Lauszus/TKJ filter — understand it, then use it). Add as
a function in `USER CODE BEGIN 0`:
```c
// 2-state Kalman (angle, bias). Tuning knobs: process/measurement noise.
float Q_angle = 0.001f, Q_bias = 0.003f, R_measure = 0.03f;
float kalAngle = 0.0f, kalBias = 0.0f;
float P[2][2] = {{0,0},{0,0}};

float kalman(float newAngle, float newRate, float dt) {
    // --- Predict: advance angle with gyro, grow uncertainty ---
    kalAngle += dt * (newRate - kalBias);
    P[0][0] += dt * (dt*P[1][1] - P[0][1] - P[1][0] + Q_angle);
    P[0][1] -= dt * P[1][1];
    P[1][0] -= dt * P[1][1];
    P[1][1] += Q_bias * dt;
    // --- Update: correct with accel measurement ---
    float S = P[0][0] + R_measure;      // estimate error
    float K0 = P[0][0] / S, K1 = P[1][0] / S;   // Kalman gains
    float y = newAngle - kalAngle;      // innovation (measurement - prediction)
    kalAngle += K0 * y;
    kalBias  += K1 * y;
    float P00 = P[0][0], P01 = P[0][1];
    P[0][0] -= K0 * P00;  P[0][1] -= K0 * P01;
    P[1][0] -= K1 * P00;  P[1][1] -= K1 * P01;
    return kalAngle;
}
```
Then in the loop, call it and add a 4th CSV column:
```c
    float rollKalman = kalman(rollAcc, gyroRate, dt);
    // change the print to 4 columns:
    sprintf(line, "%.2f,%.2f,%.2f,%.2f\r\n", rollAcc, gyroAngle, roll, rollKalman);
```
**The three tuning knobs, in plain terms:** `R_measure` = how noisy you think the accel is (bigger →
trust accel less). `Q_angle`/`Q_bias` = how much you let the model drift between corrections (bigger →
trust the gyro model less). The defaults work well for the MPU6050/6500; nudge them only if needed.

**Re-log and re-plot** (update `plot_tilt.py` to read the 4th column). You'll likely see the Kalman
and complementary lines nearly overlap — that's expected, and being able to **show and discuss that
comparison** is itself a strong portfolio point.

---

# Part 5 — Debugging (structured, per symptom)

**Angles print blank or as garbage/`?`** → float-printf isn't enabled (Part 1, step 2). Fix and rebuild.

**The fused angle slowly runs away / spirals instead of settling** → your gyro's sign is opposite to
your accel-angle's sign, so the two fight each other. **Fix:** negate the rate — use
`gyroRate = -(gx - gxBias) / 131.0f;`. (Verify by tilting slowly: the fused angle should move the same
direction as the accel angle, not opposite.)

**Angle jumps by ~360° or flips sign near vertical** → that's `atan2` wrapping at ±180°, which is
normal and harmless for a robot that stays near upright. Don't "fix" it.

**Fused angle is too noisy** → raise `alpha` (e.g. 0.99) to trust the gyro more.
**Fused angle lags / drifts** → lower `alpha` (e.g. 0.95) to pull toward the accel harder, and make
sure your bias calibration ran with the sensor truly still.

**First `dt` is huge / first angle spikes** → `lastTick` wasn't seeded before the loop. Confirm the
`lastTick = HAL_GetTick();` line runs in `BEGIN 2`.

**Python: `could not convert string to float`** → your CSV has the "Calibrating…"/"found" text lines
at the top. The provided script uses `genfromtxt(..., invalid_raise=False)` and drops non-numeric
rows, so it should handle it — but if you edited it, just delete those first text lines from the CSV.

**All angles stuck at one value** → the I2C read is failing mid-run (loose wire) or the sensor slept;
re-check Phase 2 basics (`WHO_AM_I`, wake).

> **Debug mindset:** you now have three signals printing at once (accel/gyro/fused). When something
> looks wrong, *look at which of the three misbehaves* — noisy = accel path, sliding = gyro/bias,
> both-wrong = read/scale. The comparison columns are your diagnostic.

---

# Part 6 — Commit + learning log

```bash
git add .
git commit -m "phase3: complementary + Kalman tilt estimation, logged and plotted"
git push
```
Add the `tilt_plot.png` to your repo (in `media/` or `control/`) — a results figure in the repo is
worth a lot. In `LEARNING_LOG.md`, capture: how the complementary filter fuses high-pass gyro with
low-pass accel, what `alpha`/`dt`/bias each do, what your plot showed (noise gone, drift gone), and —
if you did it — the Kalman intuition (predict/update, it tracks bias as a state).

---

## ✅ Phase 3 checkpoint — done when:

- [ ] Fused angle reads ~0° flat and ~±90° on its side, and is **smooth**
- [ ] You logged a run and produced `tilt_plot.png` showing accel(noisy) vs gyro(drift) vs fused(clean)
- [ ] You can explain, in your words: why accel needs low-pass and gyro needs high-pass, what `alpha`
      trades off, and why the gyro must be bias-calibrated
- [ ] (Bonus) Kalman added and compared
- [ ] Committed + pushed + LEARNING_LOG updated

Next: **Phase 4 — drive the motors** (PWM + TB6612, read the encoders), open-loop, so that in Phase 5
this clean angle can finally *command* the wheels and the robot balances.

---

## Mini-glossary (Phase 3)

- **Sensor fusion** — combining sensors so the result beats any one of them.
- **Complementary filter** — weighted blend: high-pass gyro + low-pass accel; one line, `alpha` sets
  the split.
- **Integration (of the gyro)** — summing rate×dt to get angle; the source of drift.
- **Drift** — slow error growth from integrating a biased/noisy rate.
- **Bias (zero-rate offset)** — the non-zero gyro reading at rest; measured and subtracted.
- **`atan2(y,x)`** — four-quadrant angle; scale factor cancels, so raw counts are fine.
- **`dt`** — real loop time in seconds, from `HAL_GetTick()`.
- **`alpha`** — complementary weight (~0.98); higher = smoother/slower, lower = snappier/noisier.
- **Kalman filter** — optimal predict/update estimator; also tracks the gyro bias as a state.
- **Kalman gain** — how much to trust the new measurement vs the prediction each step.

---

## Sources

- **Complementary filter** — Pieter-Jan's "Reading a IMU without Kalman: The Complementary Filter"
  (the classic clear explanation of the high-pass/low-pass blend).
- **Kalman for IMU** — Kristian Lauszus (TKJ Electronics), "A practical approach to Kalman filter for
  the balancing robot" — the origin of the 2-state implementation above.
- **MPU6050/6500 Register Map** — for `ACCEL_XOUT_H` (0x3B), `GYRO_XOUT_H` (0x43) and scale factors.
- **STM32 HAL** — `HAL_GetTick()` for millisecond timing (docs in the HAL reference).
- Optional video: Brian Douglas "Sensor Fusion" and MATLAB Tech Talks on complementary/Kalman filters
  (watch only if a concept won't click from the text above).
