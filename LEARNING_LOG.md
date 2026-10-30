# Learning Log

One dated entry per working session: what I did, what I learned (the concept, not just
the action), what broke and how I fixed it, and what I don't understand yet.

> **Note (30 Oct 2026).** The Phase 1–4 entries below were reformatted: the original file had
> broken Markdown escaping that rendered as literal backslashes on GitHub. The wording is
> unchanged except where an entry was checked against the committed code and the logged data
> in `control/` and found to be wrong or incomplete. Every such change is marked
> **Correction (30 Oct 2026)** and says what the evidence is.

---

## 2026-09-12 — Phase 1: Toolchain + blink

**Did:** Installed STM32CubeMX + STM32CubeIDE 2.2.0. Created `phase01_blink` in CubeMX for the
STM32F407, set PD12 as GPIO_Output (label `LED_GREEN`), generated code, imported to CubeIDE.
Added `HAL_GPIO_TogglePin` + `HAL_Delay(500)` in the `while(1)` loop. Built and flashed over the
on-board ST-Link. Green LED blinks at 1 Hz. Added PA0 button input.

**Learned (the why):** Software controls hardware by writing bits to peripheral registers
(memory-mapped I/O) — HAL functions just wrap that. A GPIO output drives a pin to 3.3 V/0 V;
these LEDs are active-high. `HAL_Delay` is BLOCKING and runs off the 1 ms SysTick — it can't
drive a fixed-rate control loop, which is why Phase 5 needs a timer interrupt. Code must sit
between the `USER CODE` markers or CubeMX erases it on regeneration. The chip runs at 16 MHz
(internal HSI) by default, not its 168 MHz max.

**Broke / debugged:** ST-Link wouldn't connect: "No ST-LINK detected" → firmware upgrade
"GoToUsbLoader" error → "failed to connect to target". Fixed by switching the debugger to
**Connect under reset**. Lesson: flashing failures are almost always cable/port/connection
settings, not code.

**Open questions:** None recorded at the time.

**Commit:** No commit at the time. The project was first committed in `e86aef6` (2026-09-23),
together with the Phase 2 and Phase 4 firmware. The committed `main.c` is the final state of
this session: holding B1 (PA0) lights the green, orange and blue LEDs (PD12, PD13, PD15). The
1 Hz toggle loop was replaced by that button code and is not in the repo.

---

## 2026-09-13 — Phase 2: IMU over I2C → UART

**Did:** Wired the IMU module to the STM32 over I2C (SCL = PB8, SDA = PB7, AD0 → GND for
address 0x68) and an FTDI adapter for serial out (PA2 → FTDI RX, common GND). Configured
I2C1 + USART2 (115200) in CubeMX. Wrote code to check `WHO_AM_I`, wake the sensor
(`PWR_MGMT_1 = 0`), read 6 accel + 6 gyro bytes, reassemble them into signed 16-bit values,
and stream them over UART to Tera Term. Verified live: flat sensor reads ~1 g on Z, and
values respond correctly to tilt and rotation. Photo of the live stream:
[`media/phase02-imu-raw-stream.jpg`](media/phase02-imu-raw-stream.jpg).

**Learned (the why):**
- I2C is a 2-wire master/slave bus (SDA data, SCL clock); devices have addresses (0x68) and
  register-based comms — you read/write numbered registers.
- HAL wants the address shifted left by 1 (`0x68 << 1`) because the bottom bit is the R/W flag.
- Each axis is 16 bits sent as two bytes, high byte first (big-endian) → rebuild with
  `(int16_t)((hi << 8) | lo)`. The `int16_t` cast makes it signed so tilt direction gives +/−.
- Accel gives absolute tilt (feels gravity) but is noisy; gyro is smooth but has a constant
  bias (~−2.6 °/s at rest) that would make an integrated angle drift. This is the reason
  Phase 3 needs sensor fusion.
- The F407 Discovery has no built-in USB-serial, so an FTDI adapter is needed to see UART.

**Broke / debugged:**
1. Build error "`huart2` undeclared / implicit `HAL_UART_Transmit`" → I hadn't enabled USART2
   in CubeMX, so the handle didn't exist. Rule learned: an undeclared `hX` handle = peripheral
   not enabled in CubeMX.
2. "MPU6050 NOT found (got 0x70)" → NOT a wiring fault. Reading the actual value showed the bus
   works and a chip answered. The module was sold as an MPU6050, but 0x70 is the MPU6500's
   `WHO_AM_I` value, not the MPU6050's 0x68. The registers used here are compatible, so I
   accept either value. Lesson: read the actual value, don't just react to "not found".
3. "found" printed but no data → I'd put the loop code between `END WHILE` and `BEGIN 3`
   (outside the `USER CODE` markers), so regenerating for USART2 wiped it. Moved it inside
   `BEGIN 3`/`END 3`. Rule: all my code goes between `USER CODE BEGIN/END` or it gets deleted
   on regenerate.

**Debugging method that worked:** the "found/NOT found" print isolates UART from I2C — if it
prints at all, UART works, so the problem is I2C or logic. Cut every problem in half.

**Open questions:** None recorded at the time.

**Commits:** `cbfd7bf` and `0e023a1` (learning-log entries), `0c145f4` (README status).
Despite its message, `cbfd7bf` contains only `LEARNING_LOG.md`. The `phase02_imu` firmware was
first committed in `e86aef6` (2026-09-23), and by then it already held the Phase 3 filter code,
so the raw-stream-only version of this program is not preserved as separate code.

---

## 2026-09-14 → 2026-09-15 — Phase 3: Tilt estimation

**Did:** Fused the IMU accel + gyro into one clean roll angle. Built a complementary filter,
then added a 2-state Kalman filter (angle + gyro bias). Logged accel / gyro-only /
complementary / Kalman over UART with a millisecond timestamp, and plotted them in Python
(`control/plot_tilt.py`). Tested with ±60° tilt sweeps and two `R_measure` settings.
The filter code lives in `firmware/phase02_imu/Core/Src/main.c` — Phase 3 was built inside
the Phase 2 project, not in a separate one.

**Learned (the why):**
- Accel = absolute angle (`atan2` of gravity) but noisy and fooled by motion; gyro = smooth
  rate that must be integrated, so it drifts. Fusion combines their strengths.
- Complementary filter: `angle = alpha*(angle + gyroRate*dt) + (1-alpha)*accelAngle` — a
  high-pass on the gyro + low-pass on the accel, one fixed weight (`alpha = 0.98`).
- Kalman filter: predict with the gyro (uncertainty grows), correct with the accel
  (uncertainty shrinks); the Kalman gain K is computed each step from the ratio of
  prediction vs measurement uncertainty. It also tracks the gyro bias as a live 2nd state.
- `R_measure` = how much you trust the accel: low R → follows the accel more closely,
  spikes included; high R → leans on the gyro and smooths the accel out.
- `atan2` cancels the sensor scale factor; `dt` comes from `HAL_GetTick()`; gyro bias is
  calibrated by averaging 2000 still samples at startup.

> **Correction (30 Oct 2026) — the R_measure claim.** This entry originally said that raising
> R from 0.03 to 0.1 "made the Kalman ignore a violent shake (accel spiked to 170–200 deg)".
> The logs do not support that as stated:
> - The accel angle can never exceed ±180°, because `atan2` wraps there. In both runs it
>   reached about ±179° during the shake. The 198.5° value came from the **Kalman estimate**
>   in the R = 0.03 run, which followed the accel through the wrap.
> - In the R = 0.1 run the Kalman estimate stayed within −65° to +55°. But that was a
>   **separate shake**, and the complementary filter, which R does not affect, also stayed
>   within −65° to +55° in the same run. So the second shake may simply have been milder.
>
> What the data shows: at R = 0.03 the Kalman filter followed a wrapped accel spike to
> 198.5°. Whether R = 0.1 would have rejected **that same** spike is not yet tested. A fair
> test replays one logged run through both R values offline. The accel angle and the
> gyro rate can both be recovered from the CSV columns.

**Broke / debugged:**
1. Variable scope — declared loop variables inside the `if (who == ...)` block, so the while
   loop couldn't see them ("undeclared"). Fix: declare persistent vars at the top of
   `BEGIN 2`, before the `if`.
2. Plot showed only ~4 s for a 25 s run — the script assumed 100 Hz. Fix: log a real millis
   timestamp (`HAL_GetTick`) as CSV column 0 and use it for the time axis. The blocking UART
   print slows the loop.
   > **Correction (30 Oct 2026):** this line originally gave the real loop rate as
   > "~20–60 Hz". All four logged runs in `control/` show a 15–16 ms loop period, i.e.
   > **~62–67 Hz**.
3. Gyro sign.
   > **Correction (30 Oct 2026):** this item originally said the gyro sign was inverted
   > and was fixed by negating `gyroRate`. The committed code does **not** negate it:
   > `gyroRate = (gx - gxBias) / 131.0f`. The logs show what happened:
   > - In `control/(-ve gyrorate)tilt_log.csv` the gyro-only angle moves **opposite** to the
   >   accel angle (correlation of their changes: −0.92).
   > - In the three later runs, made with the committed code, they move **together**
   >   (+0.57 to +0.65).
   >
   > So the un-negated sign is the correct one for this mounting, and the negated version
   > was the run with the problem. Phase 5 must keep the sign as committed.

**Known issue (found 30 Oct 2026):** `kalAngle = roll;` runs before `roll` is seeded from the
accelerometer, so the Kalman angle starts at 0° rather than at the startup tilt and converges
from there. The comment in the code said otherwise; the comment has been corrected, and the
behaviour is unchanged so the code still matches the logged data.

**Open questions:** None recorded at the time.

**Result:** The fused estimates track ±60° tilt sweeps (accel range about ±65–70° in the
logs). The gyro-only angle visibly drifts while the fused angles stay with the accel. Plots
for each run are in `control/`.

**Commit:** `fd54aa5` — CSV logs, plots and `plot_tilt.py`. The filter firmware itself was first
committed in `e86aef6` (2026-09-23), as part of `firmware/phase02_imu`.

---

## 2026-09-19 → 2026-09-23 — Phase 4: Open-loop motor drive + encoder feedback

**Did:** Configured TIM1_CH1 as 25 kHz PWM (PSC = 0, ARR = 999) and PE7/PE8/PE15 as
direction + standby GPIO, wired a JGA25-370 through a TB6612FNG on a barrel-jack battery pack,
and set TIM3 to quadrature encoder mode (TI12, PB4/PB5) to read wheel motion. Wrote a
`motorA(int)` helper mapping a signed command to direction pins + duty, and a labelled FWD/REV
test that samples the encoder every 200 ms *during* motion and prints over USART2/FTDI at
115200. Photo: [`media/phase04-motor-a-encoder-test.jpg`](media/phase04-motor-a-encoder-test.jpg).

PWM frequency check (verified 30 Oct 2026 from `phase04_motors.ioc`): HSI 16 MHz → PLL
(M = 8, N = 50, P = 4) → 25 MHz SYSCLK; APB2 ÷2 = 12.5 MHz, doubled for timers = 25 MHz;
25 MHz / (0 + 1) / (999 + 1) = **25 kHz**.

**Result:** +245 counts / 200 ms forward, −244 reverse, steady across 10 samples and symmetric
both ways (~1225 counts/s at 30 % duty). Commanded direction matches measured direction.

**Learned (the why):**
- An MCU pin sources ~20 mA; a motor draws hundreds. The H-bridge is the power stage — the MCU
  only supplies *intent* (direction bits + a duty cycle), never current.
- PWM sets speed because the motor's inductance and rotor inertia average the fast on/off
  waveform into a mean voltage. `duty = CCR / (ARR + 1)`; 25 kHz is above audible so the motor
  doesn't whine.
- TB6612 truth table: IN1/IN2 = 10 forward, 01 reverse, 00 coast, 11 brake; STBY low disables
  everything regardless of the inputs. STBY not being pulled high is the classic "nothing
  happens".
- Quadrature: two Hall channels 90° out of phase. Position is the edge count; *direction* is
  which channel leads. In timer encoder mode the hardware counts every edge with zero CPU
  cost — the loop just reads a register.
- Reading a 16-bit counter as `int16_t` gives signed rollover for free: 65535 reads as −1, so
  backwards motion is negative without any wraparound logic.
- Two mirrored motors need the sign fixed in *software*, not by swapping wires, so the whole
  codebase can rely on one "positive = robot moves forward" convention.

**Broke / debugged:**
- Missing `<stdio.h>`/`<string.h>` → implicit declaration of `sprintf`/`strlen`.
- Pasted loop code into the unprotected gap between `USER CODE END WHILE` and
  `USER CODE BEGIN 3`. It compiled, but CubeMX regeneration would silently delete it. All
  hand-written code lives between `BEGIN`/`END` markers — no exceptions.
- Deleted `main()`'s closing brace while pasting → "invalid storage class for function
  `MX_TIM1_Init`" and "expected declaration at end of input". A cascade of nonsense errors
  starting at a function *after* your edit usually means an unbalanced brace, not a problem
  in that function.
- Motor spun for a split second then stopped: inrush current tripping the supply. Isolated it
  by checking whether UART was still printing — it was, so the MCU hadn't browned out; the
  power path had cut out.
- Encoder appeared to creep by only +3..+6: blocking `HAL_Delay` made the loop print once per
  ~6.2 s, and forward and reverse cancelled almost exactly between prints. Fixed by sampling
  inside a for-loop during each motion phase. Lesson: if you only sample at the end of a
  motion, you measure the net result, not the motion.

**Open questions:** battery current rating; whether the bulk capacitor across VM/GND is fitted
and what exactly resolved the power dropout; counts per wheel revolution (to measure).

**Deferred to Phase 5 (by design):** TIM2 encoder mode (ARR must be 65535 — TIM2 is 32-bit on
the F407, unlike TIM3), motor B wiring, and the A/B sign convention. Adding a second motor and
closing a control loop in the same step means two failure sources at once.

**Commits:** `e86aef6` (firmware, docs; also the first commit of the Phase 1 and Phase 2
firmware) and `dc26258` (README).

---

## 2026-09-26 → 2026-09-28 — Phase 5: Second motor + loop skeleton (in progress)

*Entry written on 30 Oct 2026 from the project files, their timestamps and the bench photo.
Nothing below is reconstructed from memory except where marked.*

**Did:**
- Created `phase05_balance` as a fresh CubeMX project (MCU selector, STM32Cube FW_F4 V1.28.3)
  so all Phase 5 peripherals are configured in one pass:
  - Clock: HSI 16 MHz → PLL (M = 8, N = 168, P = 2) → **168 MHz** SYSCLK; APB1 ÷4 (timers
    84 MHz), APB2 ÷2 (timers 168 MHz).
  - TIM1 CH1/CH2 PWM, PSC = 7, ARR = 999 → 168 MHz / 8 / 1000 = **21 kHz**.
  - TIM3 (motor A) and TIM2 (motor B) in encoder mode TI12, ARR = 65535, input filter 10.
  - TIM6 PSC = 8399, ARR = 49 → 84 MHz / 8400 / 50 = **200 Hz**, interrupt enabled in NVIC.
  - I2C1 on PB8/PB7 at **400 kHz**; USART2 at 115200.
  - User labels AIN1/AIN2/BIN1/BIN2/STBY, BTN_ARM (PA0), LED_ARMED/LED_CAL/LED_FALL
    (PD12/PD13/PD14).
- Wrote `motors.c`: one `drive()` helper with `MOTOR_A_SIGN = +1` and `MOTOR_B_SIGN = −1`, the
  only place in the firmware that knows the motors are mirrored.
- `main.c` drives both motors at 300/999 and prints the encoder change over each 200 ms window
  for both wheels.
- Ran the test: with `MOTOR_B_SIGN = −1`, both encoders read positive when the motors are
  commanded forward. *(Result as I observed it on 2026-09-28; no readings from this test are saved in the repo.)*
  Photo of the two-motor bench wiring:
  [`media/phase05-two-motor-bench-wiring.jpg`](media/phase05-two-motor-bench-wiring.jpg).
- Created `mpu6500`, `tilt` and `pid` modules as empty stubs.

**Not done yet:** the IMU, tilt and PID modules have no code; TIM6 is configured but not
started; no closed loop; no balance.

**Commit:** this entry and the `firmware/phase05_balance` project are committed together on
2026-10-30.

<!--
Template for each entry:

## YYYY-MM-DD — Phase N: <title>
**Did:**
**Learned (the why):**
**Broke / debugged:**
**Open questions:**
**Commit:** <short hash / message>
-->
