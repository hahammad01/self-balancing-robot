# Phase 1 — Toolchain + Blink

**Goal:** Prove the entire edit → build → flash → run → debug path works end-to-end,
using the simplest possible program (blink an LED). Nothing here touches the battery,
the motors, or the IMU. Pure USB power, zero hardware risk.

**Time:** ~half a day.

**Why this phase exists (don't skip it):** In embedded work, "Hello World" is a blinking
LED. The point is *not* the LED — it's that by the end you have a confirmed working toolchain:
the IDE talks to the board, your code actually runs on the silicon, and the debugger can stop
it. Every later phase assumes this path works. If something is broken (driver, cable, IDE,
board), you want to find it now with 5 lines of code, not while chasing a phantom "IMU bug"
that is really a flashing problem.

**What you'll actually learn:**
- The STM32 toolchain (STM32CubeIDE = editor + compiler + CubeMX config + ST-Link flasher + debugger)
- What a GPIO pin is and how the HAL library drives one
- How firmware gets from your PC onto the chip (ST-Link over SWD)
- The anatomy of a generated STM32 `main.c` (clock setup, HAL init, the `while(1)` super-loop)

---

## Step 0 — Initialize the git repo

Unzip the scaffold I gave you, then inside the `self-balancing-robot/` folder:

```bash
cd self-balancing-robot
git init
git add .
git commit -m "chore: repo scaffold (docs, README, gitignore)"
```

> **Why now:** every phase ends with a commit. Starting the repo before you write any
> firmware means your very first blink is already version-controlled. Create a GitHub repo
> (private is fine for now) and `git remote add origin ...` whenever you're ready to push.

---

## Step 1 — Install STM32CubeIDE

1. Go to ST's page: **https://www.st.com/en/development-tools/stm32cubeide.html**
2. Download **STM32CubeIDE** (the **Eclipse-based** one, not the new VS Code extension).
   It's free, multi-OS (Windows / Linux / macOS, 64-bit). You'll need a free ST account to download.
3. Install it. On **Windows**, the installer also installs the **ST-Link USB driver** — accept it.
   On **Linux**, you may need to install the ST-Link udev rules (the installer offers this;
   say yes, or you'll get "permission denied" on the ST-Link).

> **Why the Eclipse version, not VS Code:** the Eclipse STM32CubeIDE has the graphical pin
> configurator (CubeMX) built in. For *learning*, seeing the chip's pins and clocks laid out
> visually and watching it generate the init code is far more instructive than a bare VS Code
> setup. You can move to VS Code later once the concepts are second nature.

---

## Step 2 — Orient yourself on the board

Look at your STM32F407G-DISC1. Find:
- The **four user LEDs** in the middle: green (PD12), orange (PD13), red (PD14), blue (PD15).
- The **blue user button** B1 (that's PA0).
- The **two USB ports**. The one labelled **ST-LINK** (top edge) is the one you plug into your
  PC — it powers the board AND programs/debugs it through the on-board ST-Link. (The other,
  USB OTG, is for later.)

Plug the **ST-LINK** USB port into your computer. A couple of LEDs (power + ST-Link comms)
should light. That's it — no battery, no wiring.

> Reference: ST board user manual **UM1472** confirms every pin. Keep it open in a tab.

---

## Step 3 — Create the project in STM32CubeIDE

1. **File → New → STM32 Project**.
2. In the selector, click the **Board Selector** tab, search **STM32F407G-DISC1**, select it.
   - When it asks "Initialize all peripherals with their default mode?" choose **No** for now —
     we want a minimal project so you see exactly what you add, not a wall of auto-config.
   - (If you only find the MCU selector, pick MCU **STM32F407VGTx** — that's the exact chip on the board.)
3. Name the project e.g. `phase01_blink`, language **C**, finish.
4. It opens the **`.ioc` file** — this is the CubeMX graphical config. You'll see the chip package
   with all its pins.

---

## Step 4 — Configure the LED pin (GPIO output)

In the `.ioc` pinout view:
1. Click pin **PD12**. A menu appears → choose **GPIO_Output**. The pin turns green.
2. (Optional but nice) right-click PD12 → **Enter User Label** → type `LED_GREEN`. Now your
   code can refer to `LED_GREEN_Pin` / `LED_GREEN_GPIO_Port` instead of raw `GPIO_PIN_12`.
3. Press **Ctrl+S**. CubeIDE asks to generate code → **Yes**. It writes `main.c` with all the
   init for that pin already done.

> **What a GPIO output is:** a single physical pin the CPU can drive HIGH (3.3 V) or LOW (0 V)
> under software control. An LED + resistor sits between the pin and ground, so HIGH = lit,
> LOW = off. This is the most fundamental output in all of embedded.

---

## Step 5 — Write the blink

Open `Core/Src/main.c`. Find the main loop — it looks like:

```c
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
```

Put your code **between the `USER CODE BEGIN/END` markers** (critical — see the warning below):

```c
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);  // flip the pin HIGH<->LOW
    HAL_Delay(500);                                           // wait 500 ms
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
```

(If you didn't add the label, use `GPIOD, GPIO_PIN_12` instead of `LED_GREEN_GPIO_Port, LED_GREEN_Pin`.)

> ⚠️ **The #1 beginner mistake:** CubeMX *regenerates* `main.c` every time you change the `.ioc`.
> Anything **outside** the `USER CODE BEGIN/END` comment blocks gets **wiped** on regeneration.
> Always write your code *between* those markers. This will save you hours of confusion later.

---

## Step 6 — Build and flash

1. **Build:** hammer icon, or **Project → Build Project** (Ctrl+B). Watch the console: `Build Finished. 0 errors`.
2. **Flash + run:** click **Run ▶** (the green play button). First time, it creates a "Run
   Configuration" — accept defaults (ST-Link, SWD). It downloads the `.elf` to the chip and resets it.
3. The **green LED blinks** once per second (500 ms on, 500 ms off).

🎉 That's the whole toolchain proven: your source compiled, flashed over the on-board ST-Link,
and is running on the STM32.

**If it doesn't work:**
- No LED: confirm you built *and* ran (not just built); confirm you toggled PD12.
- "No ST-Link detected": wrong USB port (use the **ST-LINK** one) or missing driver (Step 1).
- Build errors: you likely pasted code outside the USER CODE markers or mistyped the pin label.

---

## Step 7 — Understand what you just ran

Open `main.c` and read top-to-bottom. You're looking for four things:

1. `HAL_Init();` — boots the HAL library and the SysTick timer (that's what makes `HAL_Delay` work).
2. `SystemClock_Config();` — sets up the clock tree. The F407 can run to 168 MHz; this function
   configures the PLL to get there. (You don't need to understand every field yet — just know
   *this* is where the chip's speed is set.)
3. `MX_GPIO_Init();` — the generated code that configured PD12 as an output (enables the GPIOD
   clock, sets mode/speed). Read it: this is what your two `.ioc` clicks produced.
4. `while(1){ ... }` — the **super-loop**. Bare-metal firmware with no RTOS runs forever in this
   loop. Everything the robot does will eventually hang off a loop like this (later, driven by a
   timer interrupt instead of `HAL_Delay`).

> **Concept — why `HAL_Delay` is a crutch we'll drop:** `HAL_Delay(500)` blocks the CPU doing
> nothing for half a second. Fine for blinking. Fatal for a balancer, which must run its control
> math at a precise, fixed rate (~200 Hz) no matter what. In Phase 5 you'll replace blocking
> delays with a **timer interrupt** that fires exactly every 5 ms. Note that mental bookmark now.

---

## Step 8 — Stretch it a little (cements the learning)

Do these small variations — they take minutes and teach more than the blink itself:
1. Blink all **four** LEDs in sequence (add PD13/14/15 as outputs, toggle in turn) — a "Larson
   scanner" / Knight Rider sweep.
2. Make the **button (PA0)** control the LEDs: add PA0 as **GPIO_Input**, and in the loop use
   `if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) { ... }`. Remember PA0 is **active
   HIGH** on this board (pressed = SET). This is your first *input* — reading the physical world,
   which is exactly what reading the IMU will be in Phase 2, just more complex.

---

## Step 9 — Commit + log (the habit that makes this a portfolio)

```bash
git add .
git commit -m "phase1: blink LED + button input on F407-DISC1 (toolchain verified)"
```

Then open `LEARNING_LOG.md` and fill in today's entry: what you did, the concept you learned
(GPIO, super-loop, why HAL_Delay won't survive), anything that broke and how you fixed it.
Commit that too.

---

## ✅ Phase 1 checkpoint — report back with:

- [ ] STM32CubeIDE installed, board detected
- [ ] Green LED blinks
- [ ] Button controls an LED (Step 8.2)
- [ ] First commit(s) made
- [ ] One thing you didn't fully understand (so we dig into it before Phase 2)

When that's done, Phase 2 is reading the MPU6050 IMU over I2C — and that's where the
pin-collision gotcha (avoid PB6/PB9) and your first real peripheral config come in.

---

## Sources

- **STM32CubeIDE** (download, docs): https://www.st.com/en/development-tools/stm32cubeide.html
- **UM1472** — Discovery kit with STM32F407VG MCU (board user manual, pin map, LEDs, button): search
  "UM1472 STM32F407 discovery" on st.com.
- **DigiKey — "Introduction to STM32"** video series (excellent, beginner-paced CubeIDE + GPIO).
- **ControllersTech** (controllerstech.com) — short, practical STM32 HAL how-tos per peripheral.
- **Phil's Lab** (YouTube) — deeper embedded + firmware architecture (more useful from Phase 3 on).
