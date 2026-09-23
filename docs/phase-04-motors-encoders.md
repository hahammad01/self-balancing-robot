# Phase 4 — Drive both motors + read both encoders (JGA25-370, open-loop)

**Goal:** drive **both** JGA25-370 motors under software control — **speed** (PWM) and **direction** via
the TB6612 — and read **both** quadrature encoders so you know how far each wheel actually turned.
"Open-loop" = command and observe; no feedback correcting it yet (that's Phase 5).

**Why:** Phase 3 gave you a clean tilt angle (the *input*). Phase 4 gives you the *outputs* (wheels) and
*feedback* (encoders). Phase 5 joins them into a balance loop.

**Format:** Do / Why / Under the hood, plus a **safety-sequenced bring-up** — everything testable on USB
alone comes first, motor power last.

---

> ## 📌 Scope note — what was actually built in Phase 4
>
> Phase 4 was **closed with motor A only**: TIM1_CH1 PWM, AIN1/AIN2 direction, TB6612 AO1/AO2,
> and encoder A on TIM3 (PB4/PB5). That path is verified working — commanded direction matches
> measured direction, ~1225 counts/s at 30 % duty, steady and symmetric forward and reverse.
>
> Everything in this document about **motor B** — CubeMX step **2d** (TIM2 encoder), wiring
> section **3.3**, the B half of the code in **4c/4d**, and the B parts of **5a/5d** — was written
> but **deliberately deferred to Phase 5**, where both motors get integrated behind a single sign
> convention anyway. Treat those sections as the *ready-to-use Phase 5 starting instructions*,
> not as unfinished Phase 4 work.
>
> **Why split it this way:** adding TIM2 + a second motor *and* closing a PID loop in the same
> step means two new sources of failure at once. Phase 5 therefore begins by repeating this
> document's open-loop test with both motors running, and only then adds control. Debug one
> new thing at a time.

---

# Part 0 — Concepts

### 0.1 Why you can't drive a motor from an STM32 pin
A pin sources a few **mA** at 3.3 V; a motor pulls **hundreds of mA to amps**, and needs current to flow
**both ways** to reverse. A **motor driver with an H-bridge** takes your logic signals and switches the
high-power motor current, including reversing it.

### 0.2 The TB6612FNG
Two channels (A, B — one per motor). Per channel: **IN1/IN2** = direction, **PWM** = speed. Board pins:
**STBY** (must be **HIGH** or nothing moves — the #1 "why won't it spin"), **VM** (motor supply),
**VCC** (logic 3.3 V), **GND**, outputs **AO1/AO2** and **BO1/BO2**.

| IN1 | IN2 | Result |
|-----|-----|--------|
| H | L | spin one way |
| L | H | spin the other way |
| L | L | stop (coast) |
| H | H | short brake |

### 0.3 PWM → speed
PWM switches the pin on/off fast; **duty cycle** (% ON) sets the average voltage → speed. A timer counts
0→**ARR** and repeats; the compare value **CCR** flips the output mid-cycle, so `duty = CCR/(ARR+1)`.
We use **ARR = 999** → speed 0…999 = 0…100%.

### 0.4 Quadrature encoder + hardware timer mode
Each JGA25-370 encoder gives two Hall outputs **A and B, 90° out of phase**. Pulse count = position;
which channel leads = direction. STM32 timers have a hardware **encoder mode** that counts A/B
automatically and counts *down* on reverse. You just read the counter — zero CPU cost. **Two motors =
two encoders = two timers** (TIM3 for motor A, TIM2 for motor B).

### 0.5 Why two motors need a sign convention (important for Phase 5)
On the finished robot the two motors are mounted **facing opposite directions** (mirrored). So to make
the *robot* go forward, their shafts must turn in **opposite** physical directions — and their encoders
will naturally count in opposite senses. You fix this in **software**: pick "positive = robot moves
forward" and negate whichever motor command and/or encoder reading is backwards. In this phase we just
*observe* the relationship; Phase 5 applies the convention.

---

# Part 1 — Pin plan (all verified free on the F407-DISC1)

| Function | Signal | STM32 pin | Peripheral |
|----------|--------|-----------|------------|
| PWM speed, motor A | PWMA | **PE9** | TIM1_CH1 |
| PWM speed, motor B | PWMB | **PE11** | TIM1_CH2 |
| Direction A | AIN1 / AIN2 | **PE7 / PE8** | GPIO out |
| Direction B | BIN1 / BIN2 | **PE10 / PE12** | GPIO out |
| Driver enable | STBY | **PE15** | GPIO out |
| **Encoder A** (motor A) | A / B | **PB4 / PB5** | TIM3_CH1 / CH2 (encoder mode) |
| **Encoder B** (motor B) | A / B | **PA15 / PA1** | TIM2_CH1 / CH2 (encoder mode) |
| Debug serial | TX / RX | **PA2 / PA3** | USART2, 115200 |

> Chosen to avoid the board's on-board audio, accelerometer and USB pins, and your IMU's I2C pins from
> Phase 2/3.

---

# Part 2 — CubeMX project `phase04_motors`

New project (MCU `STM32F407VGTx` or the board; init all peripherals = **No**), named `phase04_motors`
in `firmware/`. No IMU this phase — Phase 5 merges them. Configure **five** things:

### 2a. TIM1 → PWM (both motors)
**Timers → TIM1**: Channel1 = **PWM Generation CH1**, Channel2 = **PWM Generation CH2**. Pins **PE9**,
**PE11**. Parameter Settings: **Prescaler = 0**, **Counter Period (ARR) = 999**.
*Why:* ARR=999 makes speed 0…999 = 0…100%; PWM lands in the tens of kHz — fine for the TB6612, above
audible.

### 2b. Direction + STBY → GPIO outputs
**PE7, PE8, PE10, PE12, PE15** → **GPIO_Output**, labelled **AIN1, AIN2, BIN1, BIN2, STBY**.

### 2c. TIM3 → Encoder mode (motor A)
**Timers → TIM3**: Combined Channels = **Encoder Mode**. Pins **PB4 (CH1)**, **PB5 (CH2)**.
**Counter Period (ARR) = 65535**, mode "TI1 and TI2".

### 2d. TIM2 → Encoder mode (motor B)
**Timers → TIM2**: Combined Channels = **Encoder Mode**. Pins **PA15 (CH1)**, **PA1 (CH2)**.
**Counter Period (ARR) = 65535**, mode "TI1 and TI2".

> ⚠️ **TIM2 is a 32-bit timer** on the F407 (TIM3 is 16-bit). Setting ARR = **65535** makes TIM2 wrap
> exactly like TIM3, so both encoders behave identically and both can be read as `int16_t`. Don't leave
> it at the 32-bit default or the two counters won't match in behaviour.

> ⚠️ **CubeMX will put CH1 on PA0, not PA15 — you must move it.** TIM2_CH1 can live on PA0, PA5 or
> PA15, and CubeMX picks the lowest-numbered free one. **PA0 is the blue USER button on the DISC1**
> (it has a pull-down resistor to ground on the board), so leaving CH1 there means the board's own
> button circuit is fighting your encoder's output — noisy counts, and pressing the button corrupts
> them. Remap it to PA15 (see 2d-bis below). PA1 for CH2 is correct and needs no change.

#### 2d-bis. Adding TIM2 to a project that already exists
If `phase04_motors` is already generated and running motor A, you are **editing** the `.ioc`, not
making a new project. Exact sequence:

1. **Close the project in CubeIDE** (or at least save everything). Two programs writing the same
   files at once is how you lose work.
2. **Back up `main.c`** — copy it somewhere outside the project folder. Regeneration preserves
   `USER CODE` sections, but a backup costs nothing and a lost evening costs a lot.
3. Open **STM32CubeMX** → *File → Open Project* → `firmware/phase04_motors/phase04_motors.ioc`.
4. **Check the debug pins first.** *System Core → SYS → Debug* must read **Serial Wire**. Serial Wire
   uses only PA13/PA14, leaving PA15 free. If it says *JTAG (5 pins)*, PA15 is reserved as JTDI and
   TIM2_CH1 cannot go there — set it back to Serial Wire.
   *(If your PB4 encoder already works, you are on Serial Wire — PB4 is NJTRST and JTAG would have
   taken it too.)*
5. *Timers → **TIM2*** → **Combined Channels** dropdown → **Encoder Mode**.
   Look at the chip picture: **PA0-WKUP** and **PA1** just turned green. PA1 is right; PA0 is not.
6. **Move CH1 to PA15.** On the chip picture, **left-click PA15** → a list pops up → choose
   **TIM2_CH1**. PA0 releases itself. Confirm the picture now shows **PA15 = TIM2_CH1** and
   **PA1 = TIM2_CH2**, and that PA0 is grey again.
   *(If PA0 stays green: left-click it and choose **Reset_State**.)*
7. *TIM2 → **Parameter Settings***:
   - Prescaler = **0**
   - **Counter Period (AutoReload Register) = 65535**
   - Encoder Mode = **Encoder Mode TI1 and TI2**
   - Both channel polarities = **Rising Edge** (leave as-is)
8. *Project Manager* → confirm Toolchain = **STM32CubeIDE** → **GENERATE CODE**.
9. Back in CubeIDE: select the project, press **F5** (Refresh), then **Project → Clean** and build.
10. **Verify the regeneration didn't eat your code.** Open `main.c` and check three things:
    - `MX_TIM2_Init();` appears in `main()` alongside `MX_TIM3_Init();`
    - `TIM_HandleTypeDef htim2;` exists near the top
    - your test loop is still between `/* USER CODE BEGIN 3 */` and main's closing `}`
    If the loop vanished, it was sitting in the unprotected gap — paste it back from your backup,
    **inside** the markers this time.
11. Add the start call in `/* USER CODE BEGIN 2 */`, next to the TIM3 one:
    ```c
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    ```
    *Why:* generating the peripheral only **configures** the timer. `..._Start` is what actually lets
    its counter run. No start call = a counter frozen at 0, which looks exactly like a wiring fault.

### 2e. USART2 → debug
**Connectivity → USART2 → Asynchronous**, **115200** (PA2/PA3).

**Generate Code → Open Project.**

---

# Part 3 — Wiring (board UNPLUGGED, battery DISCONNECTED)

Wire it all now, but **don't connect the battery to VM until Part 5c.**

## 3.1 — TB6612 ↔ STM32 (logic)

| TB6612 pin | Connect to |
|-----------|-----------|
| VCC | STM32 **3.3V** |
| GND | STM32 **GND** (common ground — mandatory) |
| STBY | **PE15** |
| AIN1 / AIN2 | **PE7 / PE8** |
| PWMA | **PE9** |
| BIN1 / BIN2 | **PE10 / PE12** |
| PWMB | **PE11** |

## 3.2 — JGA25-370 **motor A**: its 6 wires

2 thick wires = motor power; 4 thin wires = encoder.

| Wire (typical colour) | What it is | Connect to |
|---|---|---|
| **Red** (M1) | Motor power **+** | TB6612 **AO1** |
| **White** (M2) | Motor power **−** | TB6612 **AO2** |
| **Blue** (VCC) | Encoder power (3.3–5 V) | STM32 **3.3V** |
| **Black** (GND) | Encoder ground | STM32 **GND** |
| **Yellow** (C1) | Hall channel **A** | STM32 **PB4** (TIM3_CH1) |
| **Green** (C2) | Hall channel **B** | STM32 **PB5** (TIM3_CH2) |

## 3.3 — JGA25-370 **motor B**: its 6 wires

Same pattern, different destinations — **BO1/BO2** on the driver and **TIM2** pins on the STM32.

| Wire (typical colour) | What it is | Connect to |
|---|---|---|
| **Red** (M1) | Motor power **+** | TB6612 **BO1** |
| **White** (M2) | Motor power **−** | TB6612 **BO2** |
| **Blue** (VCC) | Encoder power (3.3–5 V) | STM32 **3.3V** |
| **Black** (GND) | Encoder ground | STM32 **GND** |
| **Yellow** (C1) | Hall channel **A** | STM32 **PA15** (TIM2_CH1) |
| **Green** (C2) | Hall channel **B** | STM32 **PA1** (TIM2_CH2) |

> ⚠️ **Colours vary by supplier — identify by function, not colour.** The **2 thick** wires are motor
> power; the **4 thin** wires are the encoder. Check the motor's label/datasheet.
>
> - **Motor +/− (Red/White):** direction is arbitrary — swap these two (or AIN1/AIN2, BIN1/BIN2) if a
>   wheel spins the "wrong" way. Cosmetic.
> - **Hall A/B (Yellow/Green):** swap them if that encoder counts down when its wheel goes forward.
>   Cosmetic.
> - **Power both encoders from 3.3 V** (they accept 3.3–5 V); 3.3 V keeps their outputs at safe STM32
>   logic levels.
> - Both encoders share the same **3.3 V** and **GND** rails as everything else.

## 3.4 — Battery (barrel-jack) → TB6612 VM — connect ONLY at Part 5c

Your battery ends in a **male barrel plug**. Use a **female DC barrel-jack → screw-terminal adapter** to
break it into + and − wires.

1. **DMM the plug first:** confirm which lead is **+** (centre pin is usual — **verify**) and that the
   voltage is **≤ 13.5 V** (the TB6612's VM limit). Reversed polarity destroys the TB6612.
2. Then:

| Battery (via adapter) | Connect to |
|---|---|
| **+** | TB6612 **VM** |
| **−** | TB6612 **GND** (already tied to STM32 GND) |

Battery **−**, TB6612 **GND** and STM32 **GND** must all be one common ground.

> **Bench power split:** the **STM32 stays on USB**; the battery powers **only the motors (VM)**. Don't
> power the STM32 from the battery at the same time as USB.

## 3.5 — FTDI USB-UART (your serial monitor)

You need this from the very first test to see the encoder counts.

| FTDI pin | Connect to |
|----------|-----------|
| GND | STM32 **GND** |
| RXD (RX) | STM32 **PA2** (USART2 TX) — **TX goes to RX** |
| VCC / 5V / 3V3 | *leave unconnected* |

Set the FTDI jumper to **3.3 V**. At the PC you'll have **two USB connections**: the board's **ST-LINK**
(flash/debug) and the **FTDI** (serial). Open Tera Term on the FTDI's COM port at **115200**.

---

# Part 3B — Power, protection & noise (recommended additions)

**1. Bulk capacitor across VM↔GND — fit before the powered spin.** 470–1000 µF electrolytic, **+ → VM**,
**− (stripe) → GND**, close to the TB6612. Motors yank current spikes that dip VM and can **reset the
STM32** (brownout) or trip your battery's protection. Polarised — mind the stripe; rating ≥ 1.5× battery
voltage (≥ 25 V for 12 V).

**2. Motor noise-suppression caps — recommended.** A **0.1 µF (100 nF) ceramic** across each motor's two
terminals, close to the motor. Brushed motors arc and spray noise that can corrupt **encoder pulses**
and **I2C**. Ceramic = non-polarised, ≥ 50 V. With two motors now, fit **two** (one per motor).

**3. Power switch — recommended.** A slide/rocker switch in series with the battery **+** lead, to cut
all motor power in one flip.

**4. Inline fuse + XT60 — recommended (Li-ion).** A ~3–5 A inline fuse in the battery + lead; an XT60
makes the pack impossible to connect backwards.

**5. Buck converter — only for the UNTETHERED robot (not on the bench).** Battery → buck IN; **set buck
OUT to exactly 5.0 V with a DMM first**, then OUT+ → STM32 **5V** pin, OUT− → GND. **Never USB + buck at
once** — bench = USB only, standalone = buck only. Raw battery into the 5 V pin destroys the board.

> **Priority:** the **bulk cap (1)** before spinning motors; **noise caps (2)** if encoder/IMU misbehave
> near running motors. Switch (3) and fuse (4) any time. Buck (5) at Phase 5 when the robot goes
> untethered.

---

# Part 4 — The code

### 4a. Includes — `USER CODE BEGIN Includes` (do this FIRST)
```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */
```
**Why:** `sprintf` is in `<stdio.h>`, `strlen` in `<string.h>`. Miss these and you get *"implicit
declaration of function 'sprintf' / 'strlen'"*.

### 4b. Motor helpers — `USER CODE BEGIN 0` (above main)
```c
/* USER CODE BEGIN 0 */
// speed: -999..+999  (sign = direction, magnitude = PWM duty)
void motorA(int speed) {
    if (speed >= 0) {
        HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET);
        speed = -speed;
    }
    if (speed > 999) speed = 999;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, speed);
}
void motorB(int speed) {
    if (speed >= 0) {
        HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_SET);
        speed = -speed;
    }
    if (speed > 999) speed = 999;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, speed);
}
/* USER CODE END 0 */
```

### 4c. Start everything — `USER CODE BEGIN 2`
```c
  HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_SET);   // enable driver (critical!)
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);                    // PWM A
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);                    // PWM B
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);              // encoder A (motor A)
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);              // encoder B (motor B)
  char line[64];
```

### 4d. The loop — the working FWD/REV test, both encoders

⚠️ **Placement:** the code goes **between `/* USER CODE BEGIN 3 */` and the closing `}`** — NOT in the
gap between `USER CODE END WHILE` and `USER CODE BEGIN 3` (CubeMX deletes anything there on the next
regeneration).

**Replace everything from `/* USER CODE BEGIN WHILE */` down to and including the `}` that closes
`main()`** with exactly this:

```c
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // ---- forward ~2 s, sampling BOTH encoders every 200 ms ----
    motorA(300); motorB(300);
    for (int i = 0; i < 10; i++) {
        int16_t a = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);   // motor A encoder
        int16_t b = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);   // motor B encoder
        sprintf(line, "FWD  A:%6d  B:%6d\r\n", a, b);
        HAL_UART_Transmit(&huart2, (uint8_t*)line, strlen(line), 100);
        HAL_Delay(200);
    }

    motorA(0); motorB(0); HAL_Delay(1000);   // stop

    // ---- reverse ~2 s ----
    motorA(-300); motorB(-300);
    for (int i = 0; i < 10; i++) {
        int16_t a = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
        int16_t b = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
        sprintf(line, "REV  A:%6d  B:%6d\r\n", a, b);
        HAL_UART_Transmit(&huart2, (uint8_t*)line, strlen(line), 100);
        HAL_Delay(200);
    }

    motorA(0); motorB(0); HAL_Delay(1000);   // stop
  }
  /* USER CODE END 3 */
}   // ← this brace closes main() — it must still be here after you paste
```

> **Brace order at the bottom of `main`:** `}` closes the `while`, then `/* USER CODE END 3 */`, then a
> second `}` closes `main()`. Delete that last one and you get confusing errors like *"invalid storage
> class for function 'MX_TIM1_Init'"* — that means `main`'s closing brace is missing.
>
> **Why sample inside the for-loops:** `HAL_Delay` is **blocking**. If you print once and then
> `HAL_Delay(2000)` through the motion, you're blind exactly while the motor moves, and forward/reverse
> cancel out so the count barely seems to change. Sampling inside the loop gives 10 readings *during*
> each phase.

---

# Part 5 — Bring-up, safety-sequenced

### 5a. Both encoders by hand — USB only, battery unplugged (zero risk)
Build, flash, open Tera Term (115200). Battery NOT on VM, so nothing spins. **Turn each wheel by hand**
and watch its column:
- Turning motor A's wheel changes **A**; turning motor B's wheel changes **B**.
- Each should count **up** one way and **down into negatives** the other.
- Held still, the number should sit **rock steady** (drift while stationary = noise problem).

This proves both encoders, both timers, and direction sensing — with no motor power at all.

### 5b. (Optional) DMM logic check — still no spin
Temporarily uncomment just `motorA(300);`, flash, check PE7/PE8 (one HIGH one LOW) and PWMA (PE9) at a
mid voltage. Re-comment it.

### 5c. Powered spin — GATED (battery last)
Fit the bulk cap, DMM-check the battery (polarity + ≤ 13.5 V), connect **+ → VM**, **− → common GND**,
keep the board on USB. Flash and watch. Expect per 200 ms sample, at duty 300:
- **FWD:** both counts move steadily in a consistent direction, a few hundred counts per sample.
- **REV:** both reverse.
- The **first sample of each phase is smaller** — that's real inertia (decelerate, stop, accelerate).
- A small **jump at each phase boundary** is a sampling artifact (the print happens at the start of the
  new phase, showing where the previous phase ended).

**Then note the A↔B sign relationship:** do A and B count the *same* way, or *opposite*? Either is fine
on the bench — write it down. Once the motors are mounted mirrored on the chassis, Phase 5 uses that to
set the "positive = robot forward" convention (by negating one motor's command and/or one encoder's
reading).

> **Motor voltage:** if your JGA25-370s are the 6 V variant and your battery is ~12 V, keep duty low or
> feed VM from the buck at ~6 V. If they're 12 V units, VM straight from the battery is fine.

### 5d. Measure counts per wheel revolution (do this — you need it in Phase 6)
Mark the wheel with tape, note the count, turn the wheel **exactly one full revolution** by hand, note
the count again. **The difference = counts per wheel revolution.** That's your conversion factor from
encoder counts to real distance/velocity. Do it for both wheels.

---

# Part 6 — Debugging

**A motor doesn't spin:** STBY not HIGH (#1) · no VM / ground not shared · duty 0 · IN1==IN2 (brake).
**Only one motor spins:** check that channel's BIN1/BIN2/PWMB wiring and BO1/BO2 to the motor · could
also be a current limit — see next.
**Motor spins for a split second then stops:** inrush current tripped the battery's protection or the
driver. Fit the **bulk cap**, test one motor at a time, lower the duty, check the battery's current
rating.
**Wrong direction:** swap that channel's AIN1/AIN2 (or BIN1/BIN2), or the motor's Red/White wires.
**Board resets when a motor starts:** brownout → bulk cap (3B-1).
**An encoder never counts:** that encoder unpowered (Blue/Black) · A/B on the wrong pins · its
`HAL_TIM_Encoder_Start` not called · try internal pull-ups on those pins in CubeMX.
**An encoder counts backwards vs its wheel:** swap its Yellow/Green (PB4↔PB5, or PA15↔PA1).
**Encoder B behaves unlike A (e.g. huge numbers):** TIM2's ARR isn't 65535 — it's a 32-bit timer, see 2d.
**Count jitters ±1 when still:** normal.
**Counts barely change during motion / tiny values:** you're sampling outside the motion (see the 4d
note) — sample inside the for-loops.

---

# Part 7 — Commit + learning log
```bash
git add .
git commit -m "phase4: open-loop motor drive + encoder feedback (motor A verified)"
git push
```
`LEARNING_LOG.md`: what an H-bridge does; PWM duty → speed; the IN1/IN2/STBY truth table; quadrature +
hardware encoder mode; the JGA25-370 6-wire mapping; why two mirrored motors need a sign convention; and
the safety sequence. Note any direction/AB swaps and your counts-per-revolution numbers.

---

## ✅ Phase 4 checkpoint — what actually closed the phase
- [x] Encoder A counts by hand, up one way and down the other (5a)
- [x] Motor A spins forward and reverse under `motorA()`, speed set by duty
- [x] Encoder counts agree with commanded direction — FWD ≈ +245 / 200 ms, REV ≈ −244 / 200 ms,
      steady across 10 samples, symmetric both ways (5c)
- [x] Motor power path survives inrush — no stutter, no dropout, UART keeps printing
- [ ] You can explain: why a driver is needed, how PWM sets speed, what STBY does, how the timer counts
      the encoder, and the JGA25-370 wire functions
- [ ] Committed + LEARNING_LOG updated

### Carried into Phase 5 (deliberately, not forgotten)
- [ ] TIM2 encoder mode added in CubeMX (**ARR = 65535** — TIM2 is 32-bit on the F407)
- [ ] Motor B wired: power → BO1/BO2, encoder → PA15/PA1 (section 3.3)
- [ ] Both motors spin; **A↔B sign relationship** recorded → sets the project-wide sign convention
- [ ] Counts-per-wheel-revolution measured for both wheels (5d) — needed for the Phase 6 velocity loop
- [ ] Battery current rating confirmed; bulk capacitor fitted across VM/GND

Next: **Phase 5 — close the loop.** Step 1 is finishing the list above (motor B open-loop, same test as
this document). Only then: merge IMU tilt (Phase 3) + motor drive (Phase 4) in a fixed-rate timer
interrupt running a PID that turns tilt error into a motor command — first balance.

---

## Mini-glossary (Phase 4)
- **H-bridge / driver** — switches letting an MCU control motor speed + direction at high current.
- **TB6612FNG** — 2-channel driver: IN1/IN2 = direction, PWM = speed, STBY = enable.
- **PWM / duty** — fast on/off; % ON sets average voltage → speed. `duty = CCR/(ARR+1)`.
- **Quadrature encoder** — two 90°-offset Hall pulse trains; count = position, phase = direction.
- **Timer encoder mode** — hardware counts the encoder; read with `__HAL_TIM_GET_COUNTER`.
- **Counts per revolution (CPR)** — encoder counts for one full wheel turn; converts counts → distance.
- **Brownout** — supply dip from a motor spike resetting the MCU; fixed with the bulk cap.
- **JGA25-370 6 wires** — 2 thick (motor +/−) + 4 thin (encoder VCC, GND, A, B).

## Sources
- **TB6612FNG Hookup Guide (SparkFun)** — pinout + truth table.
- **JGA25-370 datasheet** — voltage/ratio options, encoder PPR, 6-wire functions.
- **STM32F4 RM0090** — timer PWM generation + encoder-interface mode (and which timers are 32-bit).
- **STM32 HAL** — `HAL_TIM_PWM_Start`, `HAL_TIM_Encoder_Start`, `__HAL_TIM_SET_COMPARE`,
  `__HAL_TIM_GET_COUNTER`.
