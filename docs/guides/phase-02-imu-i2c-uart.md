# Phase 2 — Read the IMU over I2C, print it over UART (with the *why*)

**Goal:** get your **MPU6050** motion sensor talking to the STM32 over the **I2C** bus, and stream
its live accelerometer + gyroscope numbers to your PC over **UART** (serial), so you can *see* the
data change when you tilt and rotate the sensor.

**Why this phase matters:** the whole robot balances by knowing its tilt angle. That angle comes from
this sensor. Before you can filter it (Phase 3) or act on it (Phase 5), you must first *reliably read
it* and *see it*. This phase is "get eyes on the sensor."

**Same format as Phase 1:** each step is **Do / Why / Under the hood**. Read the "Under the hood"
parts — that's the understanding.

---

# Part 0 — The concepts you need first

### 0.1 What the MPU6050 is

It's a **6-DOF IMU** (Inertial Measurement Unit) — one small chip containing two sensors:

- **Accelerometer (3 axes)** — measures **acceleration** in g's along X, Y, Z. Crucially, it always
  feels **gravity** (1 g downward). So when the sensor sits still, the direction of that 1 g tells you
  **which way is down** → i.e. how the sensor is tilted. Great for *absolute* tilt, but noisy and it
  can't tell gravity apart from real motion (shaking it fools it).
- **Gyroscope (3 axes)** — measures **angular velocity** (how fast it's rotating) in °/s about X, Y, Z.
  Smooth and fast, but it only gives *rate of change*; to get an angle you'd integrate it over time,
  and tiny errors accumulate → it **drifts** away from truth.

Each is good where the other is bad. That's why Phase 3 **fuses** them (accel for long-term truth,
gyro for short-term smoothness). For now, Phase 2 just reads both raw.

### 0.2 What I2C is (the bus that connects them)

**I2C** (say "I-squared-C") is a **2-wire** communication bus that lets chips talk:

- **SDA** — the data line (bits travel here).
- **SCL** — the clock line (ticks to time each bit).

Key ideas:
- It's **master/slave**. Your STM32 is the **master** (it starts every conversation). The MPU6050 is a
  **slave** that only answers.
- Every slave has an **address**. The MPU6050's is **0x68** (7-bit). The master says "I want to talk to
  0x68" on the bus, and only that chip responds.
- Both lines need **pull-up resistors** (they idle HIGH and get pulled LOW to signal). Your GY-521
  MPU6050 module has these pull-ups built in, so you don't add any.
- Communication is **register-based**: the sensor has internal registers (like little numbered mailboxes).
  You read/write them by number. E.g. "read register 0x3B" = "give me the accelerometer X data."

> One wire pair, and you could hang many devices on it, each with its own address. That efficiency is
> why I2C is everywhere in embedded.

### 0.3 What UART is (how the numbers reach your PC)

**UART** (serial) is a simple point-to-point link to send bytes between two devices over a **TX**
(transmit) and **RX** (receive) wire, at an agreed speed called the **baud rate** (e.g. 115200 bits/s).
We use it to send text like `AX:1024 AY:-32 AZ:16210` from the STM32 to your PC.

**Important board fact:** your F407-Discovery's on-board ST-Link has **no USB-serial bridge** (unlike a
Nucleo). So the STM32's UART pins don't reach the PC by themselves. That's exactly what your **FTDI
USB-UART adapter** is for: it converts the STM32's UART into something your PC sees as a COM port. STM32
UART-TX → FTDI-RX → USB → your PC's serial terminal.

### 0.4 The data path for this whole phase (hold this picture)

```
 MPU6050  --I2C (SDA/SCL)-->  STM32F407  --UART (TX)-->  FTDI adapter  --USB-->  PC serial terminal
 (sensor)                     (reads it)                 (converts)             (you read numbers)
```

Every step below builds one arrow of that chain.

### 0.5 The pin plan (and the one trap on this board)

| Signal | STM32 pin | Why this pin |
|--------|-----------|--------------|
| I2C1 **SCL** | **PB8** | I2C1's clock. *We deliberately avoid PB6* — see the trap below. |
| I2C1 **SDA** | **PB7** | I2C1's data. PB7 is free and safe. |
| USART2 **TX** | **PA2** | Sends data to the FTDI. |
| USART2 **RX** | **PA3** | Not needed this phase, but we enable it. |

> ⚠️ **The trap:** I2C1's *default* pins are PB6 (SCL) and PB7 (SDA). But on this Discovery board, **PB6
> is already wired to the on-board audio chip (CS43L22)**. If we put our sensor's clock on PB6, the two
> devices fight on the same wire. **Fix:** move only the SCL to **PB8** (a free pin). SDA stays on PB7
> (which is safe). This is why we don't just accept the defaults.

---

# Part 1 — Wire the hardware (board UNPLUGGED)

**Do:** unplug the board from USB first (never rewire a powered board). On your breadboard, connect:

**MPU6050 (GY-521 module) → STM32:**

| MPU6050 pin | Connect to | Why |
|-------------|-----------|-----|
| VCC | STM32 **3.3V** | Power. (The module has a regulator; 3.3V matches our logic and is safe.) |
| GND | STM32 **GND** | **Common ground — mandatory.** Two chips can't communicate without a shared 0 V reference. |
| SCL | STM32 **PB8** | I2C clock. |
| SDA | STM32 **PB7** | I2C data. |
| AD0 | STM32 **GND** | Sets the I2C address to **0x68**. (AD0 low = 0x68, high = 0x69.) |
| XDA, XCL, INT | *leave unconnected* | Not needed this phase. |

> **One cable only? Skip the FTDI.** If you have a single USB cable (needed for the board's ST-Link),
> you can't also plug in the FTDI. That's fine — use the **Live Expressions** method in **Part 5B**
> instead, which reads the sensor values straight through the ST-Link cable. In that case, **wire only
> the MPU6050 above and skip the FTDI table below.**

**FTDI USB-UART adapter → STM32 (for the serial output) — only if you have a 2nd USB cable:**

| FTDI pin | Connect to | Why |
|----------|-----------|-----|
| GND | STM32 **GND** | Shared ground reference for the serial link. |
| RXD (RX) | STM32 **PA2** (USART2 TX) | The STM32 *transmits*; the FTDI *receives*. **TX goes to RX** — this cross is the classic gotcha. |
| VCC / 5V / 3V3 | *leave unconnected* | The board is already powered by its own USB; don't back-power it. |

> **Set the FTDI to 3.3V logic** if it has a voltage jumper (many do: a 3V3/5V selector). The STM32 runs
> at 3.3V, so matching keeps signal levels sane.

**Why (the whole part):** you're physically building two of the arrows from 0.4 — the I2C link (sensor
↔ STM32) and the UART link (STM32 → FTDI → PC). Every wire has exactly one job; the two easiest mistakes
are (a) forgetting a common ground and (b) not crossing TX↔RX. We call both out so you don't hit them.

**Under the hood:** the MPU6050 module's built-in 4.7 kΩ pull-ups on SDA/SCL hold the bus HIGH when idle;
the chips pull it LOW to send bits. AD0 physically ties the chip's address-select pin to 0 V, hard-wiring
its address to 0x68. The FTDI contains a tiny chip (e.g. FT232) that turns UART bytes into USB packets your
PC's driver exposes as "COMx".

---

# Part 2 — Configure the peripherals in STM32CubeMX

Open your project's `.ioc` in **CubeMX** (or make a new project `phase02_imu` the same way as Phase 1 —
your call; continuing the same project is fine and simpler). Then:

### 2a. Turn on I2C1 and fix the SCL pin
**Do:**
1. Left panel → **Connectivity → I2C1** → set **I2C** in the mode dropdown. CubeMX lights up **PB6** (SCL)
   and **PB7** (SDA) by default.
2. **Move SCL off PB6:** left-click pin **PB6** on the chip → choose **Reset_State** (unassign it).
3. Left-click pin **PB8** → choose **I2C1_SCL**.
4. Leave **PB7** as **I2C1_SDA**.
5. In the I2C1 config, leave **Standard Mode / 100 kHz** (the default). That's plenty for this sensor.

**Why:** this enables the STM32's I2C1 hardware and routes it to PB8/PB7 — avoiding the PB6 audio-chip
clash from 0.5. 100 kHz is the safe standard I2C speed; we don't need fast mode.

**Under the hood:** the STM32 has an internal "mux" that can route a peripheral (like I2C1) to a choice of
physical pins (its *alternate functions*). Assigning I2C1_SCL to PB8 tells the generated code to configure
PB8 in alternate-function mode connected to the I2C1 clock. PB6 becomes an ordinary unused pin, so the
audio chip on it is simply off our bus.

### 2b. Turn on USART2
**Do:**
1. **Connectivity → USART2** → **Mode = Asynchronous**. CubeMX assigns **PA2** (TX) and **PA3** (RX).
2. In its config, set **Baud Rate = 115200**, and confirm **8 bits, no parity, 1 stop** (the default,
   written "8N1").

**Why:** this is the serial link to your PC. 115200 is a common, reliable baud both the STM32 and your
terminal will agree on. "Asynchronous" = plain UART (no separate clock wire; both sides just agree on the
baud rate).

**Under the hood:** the STM32's USART peripheral shifts each byte out on PA2 one bit at a time at 115200
bits/second. There's no clock wire — the receiver re-times itself from the start of each byte, which is
why both ends *must* use the same baud (mismatch = garbage characters).

### 2c. Generate
**Do:** **Project Manager** tab (name/location already set if reusing the project) → **GENERATE CODE** →
**Open Project** in CubeIDE.

**Under the hood:** CubeMX adds `MX_I2C1_Init()` and `MX_USART2_UART_Init()` to `main.c`, and creates the
handles **`hi2c1`** and **`huart2`** — the C objects your code uses to talk to each peripheral.

---

# Part 3 — The code (this is the heart of the phase)

Open `Core/Src/main.c`. We'll add four things, each in its correct USER CODE section.

### 3a. Includes and the sensor's "map" (register addresses)
Find `/* USER CODE BEGIN Includes */` and add:
```c
#include <stdio.h>
#include <string.h>
```
Find `/* USER CODE BEGIN PD */` (private defines) and add:
```c
#define MPU6050_ADDR          (0x68 << 1)  // 7-bit addr 0x68, shifted for HAL (see note)
#define REG_WHO_AM_I          0x75
#define REG_PWR_MGMT_1        0x6B
#define REG_ACCEL_XOUT_H      0x3B
#define REG_GYRO_XOUT_H       0x43
```
**Why / under the hood — the address shift (a famous bug):** the sensor's address is **0x68** in 7 bits.
But ST's HAL functions want the address in **8-bit form**, i.e. shifted left by one (`0x68 << 1 = 0xD0`),
because the lowest bit is used internally as the read/write flag. If you pass `0x68` instead of `0x68<<1`,
nothing responds and you'll swear the wiring is broken. This one line saves hours.

The register numbers come from the MPU6050 datasheet: `0x75` = WHO_AM_I (an ID register), `0x6B` = power
management, `0x3B` = first accelerometer byte, `0x43` = first gyro byte.

### 3b. Wake the sensor + verify it's there
Find `/* USER CODE BEGIN 2 */` (runs once, after peripherals init) and add:
```c
uint8_t who = 0, zero = 0x00;
char msg[64];

// Ask the sensor "who are you?" — it must answer 0x68.
HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who, 1, 100);

if (who == 0x68) {
    HAL_UART_Transmit(&huart2, (uint8_t*)"MPU6050 found.\r\n", 16, 100);
    // Wake it up: by default it powers on ASLEEP. Writing 0 to PWR_MGMT_1 clears the sleep bit.
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &zero, 1, 100);
} else {
    sprintf(msg, "MPU6050 NOT found (got 0x%02X)\r\n", who);
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
}
```
**Why:** two jobs. First, a **sanity check** — read WHO_AM_I; a correct `0x68` proves the I2C wiring and
address are right *before* you trust any data. Second, **wake the chip** — the MPU6050 boots in sleep mode
to save power; until you clear that bit, every reading is frozen/zero.

**Under the hood:** `HAL_I2C_Mem_Read(handle, devAddr, regAddr, regAddrSize, dataPtr, count, timeout)` does
a full I2C transaction: master sends START, addresses 0x68 for write, sends the register number (0x75),
sends a repeated-START, addresses 0x68 for read, clocks back 1 byte into `who`, sends STOP. `HAL_I2C_Mem_
Write` is the mirror image (it writes your byte into the given register). The `100` is a 100 ms timeout —
if the sensor never answers, the call gives up instead of hanging forever.

### 3c. Read + print in the loop
Find `/* USER CODE BEGIN 3 */` (inside `while(1)`) and add:
```c
uint8_t buf[6];
int16_t ax, ay, az, gx, gy, gz;
char line[96];

// Accelerometer: 6 bytes starting at 0x3B (XH,XL,YH,YL,ZH,ZL)
HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
ax = (int16_t)((buf[0] << 8) | buf[1]);
ay = (int16_t)((buf[2] << 8) | buf[3]);
az = (int16_t)((buf[4] << 8) | buf[5]);

// Gyroscope: 6 bytes starting at 0x43
HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_GYRO_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
gx = (int16_t)((buf[0] << 8) | buf[1]);
gy = (int16_t)((buf[2] << 8) | buf[3]);
gz = (int16_t)((buf[4] << 8) | buf[5]);

sprintf(line, "ACC %6d %6d %6d | GYR %6d %6d %6d\r\n", ax, ay, az, gx, gy, gz);
HAL_UART_Transmit(&huart2, (uint8_t*)line, strlen(line), 100);

HAL_Delay(200);  // ~5 readings per second — slow enough to read with your eyes
```
**Why:** this is the actual measurement. Read 6 accel bytes, rebuild them into three signed numbers, do the
same for the gyro, format a line of text, send it over UART. The 200 ms delay just slows it so your eyes can
follow; later phases read far faster.

**Under the hood — why `(buf[0] << 8) | buf[1]`:** each axis is a **16-bit** value, but I2C moves **8 bits**
at a time, so the sensor sends it as two bytes: the **high** byte first, then the **low** byte (this is
called big-endian). To rebuild the number you shift the high byte up by 8 and OR in the low byte. The cast
to `int16_t` makes it **signed**, so tilting the other way gives negative numbers (the data is two's-
complement). `sprintf` formats the numbers into a text string; `%6d` just pads to 6 columns so they line up
neatly.

---

# Part 4 — Install a serial terminal on your PC

**Do:** install one of these free serial terminals:
- **Tera Term** (simple, recommended for Windows), or **PuTTY**.
- (STM32CubeIDE also has a built-in serial console via the "Console" view, but a dedicated terminal is
  easier to see.)

**Why:** the STM32 is sending text out the FTDI as a COM port; you need a program on the PC that opens that
COM port and displays the incoming text. That's what a serial terminal does.

---

# Part 5 — Build, flash, and watch the data

**Do:**
1. Plug the board's ST-LINK USB into the PC. Make sure the FTDI's USB is also plugged into the PC.
2. In CubeIDE: **Ctrl+B** to build, then **Run ▶** to flash (remember Phase 1's appendix if the ST-Link
   acts up — try "Connect under reset").
3. Find the FTDI's COM port: **Device Manager → Ports (COM & LPT) → "USB Serial Port (COMx)"**. Note the
   number.
4. Open your serial terminal → select that **COMx**, set **115200 baud, 8N1, no flow control** → Connect.
5. You should see: `MPU6050 found.` then a stream of `ACC … | GYR …` lines.
6. **Tilt and rotate the sensor** — the ACC numbers change with tilt, the GYR numbers spike while you're
   rotating and return to ~0 when you stop.

**What the numbers mean (so they're not just noise):**
- With the default settings, the accelerometer reads **±2 g** full-scale = **16384 counts per g**. So a
  flat, still sensor shows one axis near **±16384** (that's the 1 g of gravity on the Z axis) and the other
  two near 0.
- The gyro default is **±250 °/s** = **131 counts per °/s**. Still = near 0; rotating shows large values.
- These are **raw counts**. Converting to real units (g and °/s) is the stretch goal below.

**Why this is the win:** seeing live numbers respond to physical motion proves the *entire* chain from
0.4 works: I2C read, byte reassembly, UART transmit, PC display. You now have working eyes on the sensor.

---

# Part 5B — Viewing the data WITHOUT the FTDI (one-cable method: Live Expressions)

Use this instead of Parts 4–5 if you only have one USB cable (or just prefer it). It needs **only the
ST-Link cable already in the board** — no FTDI, no UART, no second cable, no soldering.

**The idea:** when you debug an STM32, the ST-Link can **read the chip's memory while the program is
still running**, without stopping it. So if your sensor values live in **global variables**, the
debugger can show them updating live. You're using the debugger itself as your display. This is a
standard professional technique (reading RAM over SWD is non-intrusive on Cortex-M).

### 5B.1 Make the sensor values global
Live Expressions needs variables at a fixed, always-known address, so make them **global** (file scope),
not local. In `main.c`, find `/* USER CODE BEGIN PV */` and add:
```c
uint8_t  who = 0;                 // will read 0x68 (=104) if the sensor is found
int16_t  ax, ay, az, gx, gy, gz;  // live accel + gyro
```
Then **remove those same declarations from inside the loop/Part-2 block** (so you're not re-declaring
them locally) and just *assign* to them. Your `while(1)` body becomes simply:
```c
uint8_t buf[6];

HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
ax = (int16_t)((buf[0] << 8) | buf[1]);
ay = (int16_t)((buf[2] << 8) | buf[3]);
az = (int16_t)((buf[4] << 8) | buf[5]);

HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_GYRO_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
gx = (int16_t)((buf[0] << 8) | buf[1]);
gy = (int16_t)((buf[2] << 8) | buf[3]);
gz = (int16_t)((buf[4] << 8) | buf[5]);

HAL_Delay(200);
```
(You can drop the `sprintf`/`HAL_UART_Transmit` lines entirely — they're the UART path you're not using.
The WHO_AM_I read and the wake-up write in Part 3b stay; just write the result into the global `who`.)

### 5B.2 Start a DEBUG session (not just Run)
- Build (**Ctrl+B**).
- Click the **bug icon** (Debug), not the play icon. First time: **Debug As → STM32 C/C++ Application**.
- The debugger flashes the chip and **halts at the start of `main`**. Click **Resume** (green
  play in the debug toolbar, or **F8**) so the program **runs freely**.

### 5B.3 Open Live Expressions and watch
- Menu: **Window → Show View → Live Expressions** (if not already visible).
- In that panel, click **"Add new expression"** and type a variable name — add `who`, then `ax`, `ay`,
  `az`, `gx`, `gy`, `gz` (one per row).
- With the program **running** (resumed), the values **update live**. `who` should show **104 (0x68)**.
  **Tilt the sensor** → `ax/ay/az` change; **rotate it** → `gx/gy/gz` spike and settle.

> To see `who` in hex: right-click it in Live Expressions → Number Format → Hex. 104 decimal = 0x68.

### 5B.4 If the values look frozen
- Make sure you clicked **Resume (F8)** — if the program is paused at `main` or a breakpoint, nothing
  updates.
- Make sure you launched **Debug**, not Run. Live Expressions only works inside a debug session.
- The default Debug build is unoptimized (`-O0`), which keeps your variables intact — don't change that.

### 5B.5 What you lose vs. UART, and when you'll want a 2nd cable
Live Expressions is perfect for **watching** values (exactly Phase 2's goal). What it *doesn't* do is
easily **log a stream to a file** for plotting. In **Phase 3** you'll want to record a few seconds of
data to plot a chart — for that you'll eventually want either:
- a **second USB cable** so the FTDI + UART path works (then a terminal like Tera Term can log to a file), or
- **STM32CubeMonitor** — a free ST tool that plots live variables over the **ST-Link** (no FTDI needed).

So: **Live Expressions now gets you through Phase 2 with one cable.** Grabbing a cheap second USB cable
before Phase 3 is worth it, but not required today.

---

# Part 6 — Debugging (read this BEFORE you panic; it's structured, not random)

The golden rule: **isolate the two halves.** The message `MPU6050 found.` (or `NOT found`) tells you which
half is broken, because that line only prints if UART already works.

> **Using the Live Expressions method (Part 5B)?** You have no UART, so your indicator is the global
> **`who`** in Live Expressions: it should read **104 (0x68)**. If `who` is 104 → I2C works, focus on data.
> If `who` is 0 or 255 → it's an I2C wiring/address problem (Symptom C below). Symptoms A/B (UART/terminal)
> don't apply to you.

### Symptom A — the terminal shows *nothing at all* (not even "found")
Then **UART or the terminal** is the problem, not the sensor. Check in order:
1. **Wrong COM port** — re-check Device Manager; pick the FTDI's exact COMx.
2. **Baud mismatch** — terminal must be **115200**. Wrong baud = nothing or gibberish.
3. **TX/RX not crossed** — STM32 **PA2 (TX)** must go to FTDI **RX**, not TX. Swap them.
4. **No common ground** — FTDI GND must connect to STM32 GND.
5. **FTDI driver / port** — is "USB Serial Port" even present in Device Manager? If not, reinstall the FTDI
   (FTDI VCP) driver, try another USB port.

### Symptom B — the terminal shows garbled/random characters
Almost always a **baud rate mismatch** (or the FTDI set to the wrong logic voltage). Set the terminal to
115200 and the FTDI jumper to 3.3V.

### Symptom C — it prints "MPU6050 NOT found (got 0xNN)"
Then **UART works but I2C doesn't.** The sensor isn't answering correctly. Check in order:
1. **Address not shifted** — you must pass `0x68 << 1`, not `0x68` (see 3a).
2. **SDA/SCL swapped or wrong pins** — SDA→PB7, SCL→PB8. Double-check the breadboard and the CubeMX pin
   assignment.
3. **AD0** — must be tied to GND for address 0x68 (floating AD0 can read as 0x69).
4. **No common ground** between the sensor and the STM32.
5. **Power** — is the module's VCC actually getting 3.3V? Measure with your DMM.
6. **Pull-ups** — GY-521 has them built in; if you're using a bare chip without pull-ups, the bus can't work.

> **Debugger tactic:** set a breakpoint on the `if (who == 0x68)` line, run in Debug mode, and inspect the
> variable `who` and the **return value** of `HAL_I2C_Mem_Read` (change it to
> `HAL_StatusTypeDef s = HAL_I2C_Mem_Read(...);` and watch `s`). `HAL_OK` = the sensor acknowledged;
> `HAL_ERROR`/`HAL_TIMEOUT` = it never answered → wiring/address/power. This is far faster than guessing.
> You can also call `HAL_I2C_IsDeviceReady(&hi2c1, MPU6050_ADDR, 3, 100)` — returns `HAL_OK` only if
> something answers at that address.

### Symptom D — "found" prints, but all data reads 0 (or stuck at -1 / 32767)
- All **0** → you didn't wake the sensor (the `PWR_MGMT_1` write in 3b) or it didn't take. Confirm that
  write runs and returns `HAL_OK`.
- Stuck **-1 (0xFFFF)** → the read is failing and returning all-ones; treat it like Symptom C (a bus fault
  mid-run — often a loose wire).

### Symptom E — numbers are jumpy / noisy even when still
That's **normal.** Raw IMU data is inherently noisy, and the accel picks up every vibration. Taming this is
exactly the job of Phase 3 (filtering). Don't try to fix it here.

---

# Part 7 — Stretch goals (do at least the first; it deepens understanding)

**7a. Convert raw counts to real units.** Divide accel by 16384.0 → g's; divide gyro by 131.0 → °/s. Print
those instead.
- **Gotcha (important):** printing floats with `%f` in `sprintf` needs float-printf support turned on in
  CubeIDE, or you'll see blank/garbage. Either enable it (Project → Properties → C/C++ Build → Settings →
  MCU Settings → check **"Use float with printf"**), **or** avoid `%f` by printing milli-g as integers, e.g.
  `int mg = (int)(ax / 16.384);` and print `mg`. The integer route is simpler and teaches you to sidestep a
  real embedded limitation (float printing is expensive on an MCU).

**7b. Scan the bus.** Loop an address from 1..127 calling `HAL_I2C_IsDeviceReady` and print any that answer.
You'll see `0x68` light up. Great debugging tool and it teaches how addressing works.

---

# Part 8 — Commit + learning log

**Do:**
```bash
git add .
git commit -m "phase2: read MPU6050 over I2C, stream accel+gyro over UART"
```
Then add a `LEARNING_LOG.md` entry: what you wired, the concepts (I2C master/slave + registers, the address
shift, big-endian byte reassembly, UART baud, why the FTDI is needed), what broke and how you isolated it
(UART vs I2C), and one open question.

**Why:** the debugging story here (how you isolated UART from I2C) is genuinely impressive on a portfolio —
it shows methodical thinking, not luck.

---

## ✅ Phase 2 checkpoint — done when:

- [ ] Terminal prints `MPU6050 found.`
- [ ] Live ACC/GYR numbers stream and **respond correctly to tilt/rotation**
- [ ] You can explain: what I2C's SDA/SCL do, why the address is shifted, why each axis is two bytes, and
      why the FTDI adapter is needed on this board
- [ ] Committed + LEARNING_LOG updated

Next: **Phase 3 — turn this raw data into a clean tilt angle** (complementary filter, then a simple Kalman
filter), and log it so you can plot your first results chart.

---

## Appendix B — Common BUILD errors (compile-time, before it ever runs)

These happen at **Build (Ctrl+B)** — they're about the *code/config*, not the wiring. Read the first
error line, not the last; the rest are usually knock-on noise.

**`'huart2' undeclared` and `implicit declaration of function 'HAL_UART_Transmit'`**
- **Cause:** USART2 was **not enabled in CubeMX**, so the `huart2` handle object and the UART driver
  were never generated. Your code refers to `huart2`, but it doesn't exist in the project.
- **Fix:** CubeMX → **Connectivity → USART2 → Asynchronous** (PA2/PA3, 115200) → **GENERATE CODE** →
  rebuild.
- **General rule (remember this):** any `hi2cX` / `huartX` / `htimX` that comes up **"undeclared"**
  means you forgot to enable that peripheral in CubeMX. The handle only exists *after* you enable the
  peripheral and regenerate. "implicit declaration of `HAL_..._Transmit/Init`" is the same story — the
  driver for a peripheral is only pulled into the build when that peripheral is enabled.

**`'hi2c1' undeclared`** — same cause, for I2C: enable **I2C1** in CubeMX and regenerate.

**Your added code vanished after you regenerated in CubeMX** — you wrote it **outside** the
`USER CODE BEGIN/END` markers, so CubeMX overwrote it. Only code *between* the markers survives.

**Blank or garbage where a `%f` float should print** — printing floats with `sprintf`/`%f` needs float
support turned on: **Project → Properties → C/C++ Build → Settings → Tool Settings → MCU Settings →
"Use float with printf"**. Or avoid `%f` (print integers, e.g. milli-g), which is cheaper on an MCU.

---

## Appendix C — Code walkthrough: the lines that matter (so you can debug by hand)

If the program builds but misbehaves, these are the lines to reason about. For each: what it does, and
**what you'd see if it's wrong.**

```c
#define MPU6050_ADDR (0x68 << 1)
```
The sensor's 7-bit address is `0x68`. HAL wants it **shifted left by 1** (`= 0xD0`) because the bottom
bit is the read/write flag. **If wrong** (you pass `0x68`): every I2C call fails, `who` stays 0, and you
get "MPU6050 NOT found (got 0x00)". This is the #1 silent I2C bug.

```c
HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who, 1, 100);
```
Read **1** byte from register `0x75` of the device at `MPU6050_ADDR` into the variable `who`, giving up
after `100` ms. The arguments in order: *(which I2C, device address, register number, register-size is
8-bit, where to store, how many bytes, timeout).* **Debug tip:** capture the return value —
`HAL_StatusTypeDef s = HAL_I2C_Mem_Read(...);` — then breakpoint and inspect `s`. `HAL_OK` = the sensor
answered; `HAL_ERROR`/`HAL_TIMEOUT` = nobody answered → wiring/address/power/pull-ups.

```c
if (who == 0x68) { ... } else { ... }
```
`WHO_AM_I` must read back `0x68` — it's a fixed ID baked into the chip. **This is your gate:** if this is
true, I2C is definitely working, so any later problem is elsewhere. **If `who` is `0x00`** → no reply
(wiring/address). **If `0xFF`** → the bus is floating (often no pull-ups / bad connection).
**If it's another *specific* value like `0x70` / `0x72` / `0x98`** → I2C is working fine and a chip DID
answer, but your module is an **MPU6050 clone / MPU6500**, not a genuine MPU6050. IDs: genuine MPU6050 =
`0x68`, **MPU6500 = `0x70`**, MPU9250 = `0x71`, common clones = `0x72`/`0x98`. These are
register-compatible for basic accel/gyro (0x6B, 0x3B, 0x43) and use the same scale factors — so just
**accept the value** and proceed: `if (who == 0x68 || who == 0x70) { ... }`. (Half the cheap "MPU6050"
modules are actually MPU6500s. A non-0x68 value here is almost always this, not a bug.)

```c
HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &zero, 1, 100);
```
Writes `0x00` into the power-management register `0x6B`, which **clears the sleep bit** and wakes the
sensor. **If you skip/miss this:** it builds and "found" prints, but **all readings stay 0** because the
sensor is still asleep.

```c
HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);
```
Reads **6 bytes in one go** starting at `0x3B`. The sensor auto-increments its internal address, so you
get XH, XL, YH, YL, ZH, ZL in `buf[0..5]` — all three accel axes in one transaction (efficient).

```c
ax = (int16_t)((buf[0] << 8) | buf[1]);
```
Each axis is **16 bits sent as two 8-bit bytes, high byte first** (big-endian). `buf[0] << 8` puts the
high byte in the upper half, `| buf[1]` drops the low byte in. The `(int16_t)` cast makes it **signed**,
so tilting the other way gives negatives. **If your numbers look doubled/halved or bytes swapped**, you
combined them in the wrong order (low/high flipped).

```c
sprintf(line, "ACC %6d ... \r\n", ax, ...);
HAL_UART_Transmit(&huart2, (uint8_t*)line, strlen(line), 100);
```
`sprintf` formats the numbers into the text buffer `line`; `HAL_UART_Transmit` sends those bytes out
USART2. `strlen(line)` = how many bytes to send (so you never miscount). `\r\n` = carriage-return +
newline, so each reading lands on its own line in the terminal. **If you see nothing in the terminal but
"found" logic should have run** → it's UART/terminal (COM port, baud, TX↔RX, ground), not the sensor —
see Part 6, Symptom A.

```c
HAL_Delay(200);
```
Waits 200 ms so the stream is ~5 lines/second — readable by eye. (Blocking, like Phase 1 — fine here,
replaced by a timer in Phase 5.)

> **The manual-debug mindset:** every I2C call returns a `HAL_StatusTypeDef`. When something's wrong,
> stop guessing and *look at that return value* in the debugger. `HAL_OK` vs `HAL_ERROR` instantly tells
> you whether the sensor is even talking, which cuts the problem in half every time.

---

## Mini-glossary (Phase 2)

- **IMU** — Inertial Measurement Unit: accelerometer + gyroscope in one chip.
- **Accelerometer** — measures acceleration incl. gravity → gives absolute tilt (noisy).
- **Gyroscope** — measures rotation rate → smooth but drifts.
- **I2C** — 2-wire master/slave bus: **SDA** (data) + **SCL** (clock).
- **Slave address** — the number that selects a chip on the bus (MPU6050 = 0x68).
- **Register** — a numbered internal location in the sensor you read/write.
- **UART / baud** — serial link; baud = bits per second (both ends must match).
- **Big-endian** — high byte sent first; why we do `(high << 8) | low`.
- **Pull-up resistor** — holds an I2C line HIGH when idle (built into the GY-521 module).
- **FTDI** — USB-to-UART adapter; makes the STM32's serial appear as a PC COM port.

---

## Sources

- **MPU6050 Register Map & datasheet** (InvenSense) — the authority for register numbers (WHO_AM_I 0x75,
  PWR_MGMT_1 0x6B, ACCEL_XOUT_H 0x3B, GYRO_XOUT_H 0x43) and scale factors (16384 LSB/g, 131 LSB/°/s).
- **UM1472** — F407 Discovery board manual (confirms PB6/PB9 are the on-board audio I2C — why we avoid PB6).
- **RM0090** — STM32F4 Reference Manual (I2C and USART peripheral chapters, if you want register-level depth).
- Optional video backup only if a concept won't click: ControllersTech "STM32 I2C" and "MPU6050"; DigiKey
  Intro to STM32 (UART episode).
