# Phase 1 — Toolchain + Blink (with the *why* behind everything)

**Goal:** Prove the whole edit → configure → build → flash → run path works, using the
simplest program that exists (blink an LED). No battery, no motors, no IMU. Just the board and a
USB cable.

**How to read this guide:** every step has three parts —
- **Do** — the exact click/command.
- **Why** — the reason this step exists at all.
- **Under the hood** — what's *physically* happening in the chip or the tools. Read this part.
  It's where the understanding lives.

If any line here is unclear, that's a bug in the guide — flag it and it gets fixed.

---

# Part 0 — The mental model (read this once; everything later depends on it)

You cannot understand blinking an LED until you understand four things. They're simple.

### 0.1 What an STM32 actually *is*

Your STM32F407 is a **microcontroller (MCU)**: a single chip that contains, all together:

- A **CPU** — an ARM **Cortex-M4** core. This is the part that executes instructions, one after
  another. It runs at a configurable speed (in this project, **16 million cycles per second** —
  more on that in 0.3).
- **Flash memory** (1 MB) — **non-volatile** storage. Your compiled program lives here. "Non-volatile"
  means it survives power-off, like a hard drive. When the chip powers on, the CPU starts reading
  and executing instructions straight from flash.
- **RAM** (192 KB) — **volatile** scratch memory. Your variables live here while the program runs.
  Wiped on power-off, like a PC's RAM.
- **Peripherals** — dedicated hardware blocks built into the chip that do specific jobs so the CPU
  doesn't have to: **GPIO** (control pins), **timers**, **I2C** and **SPI** and **UART** (talk to
  other chips), **ADC** (read analog voltages), etc. The whole robot project is really just
  *learning to drive these peripherals one at a time.*

So "programming the STM32" = putting machine code into its flash, which the CPU then runs, and that
code pokes the peripherals to make things happen in the physical world.

### 0.2 The single most important idea: memory-mapped I/O

Here's how software controls hardware. **Every peripheral is controlled by special memory
addresses called registers.** A register is just a 32-bit slot at a fixed address. But unlike normal
memory, writing to a peripheral register *changes physical hardware.*

Concrete example, the one we're about to use:
- The GPIO port D has a register called **ODR** (Output Data Register) at a fixed address.
- Bit 12 of that register corresponds to physical pin **PD12**.
- Write a **1** to bit 12 → the chip drives pin PD12 to **3.3 volts** → the LED lights.
- Write a **0** to bit 12 → the chip drives pin PD12 to **0 volts** → the LED goes dark.

That's it. That's the whole trick behind embedded systems. Everything — motors, sensors, comms — is
ultimately "write the right bits to the right register address." When you later read the IMU or spin
a motor, it's the same idea, just fancier registers.

> You *could* write `GPIOD->ODR |= (1 << 12);` yourself. You usually don't — you use the HAL library
> instead (see 0.4), which does exactly this for you with a readable function name. But now you know
> what that function is really doing.

### 0.3 The clock — why the chip needs a "heartbeat"

A digital chip does nothing on its own; it needs a **clock** — an electrical signal that ticks
millions of times per second. Every tick, the CPU advances one small step. Faster clock = more work
per second = more power used.

The F407 can run as fast as **168 MHz** (168 million ticks/sec). **But in this project it's running
at 16 MHz**, from a small oscillator built into the chip (called the **HSI** — High-Speed Internal).
We haven't told it to go faster, so it uses this safe default.

**Why that's totally fine for now:** blinking an LED doesn't need speed. Later (Phase 5, the control
loop) we'll crank it up to 168 MHz because balancing math must run fast. For now, 16 MHz keeps the
setup simple and nothing about blink cares. (This corrects something I said earlier — your chip is
*not* at 168 MHz in this project, and it doesn't need to be.)

The clock also drives timing. That's how the chip knows what "500 milliseconds" means — it counts
clock ticks. See `HAL_Delay` in Part 6.

### 0.4 The two tools, and the HAL library

- **STM32CubeMX** — a configuration tool. You visually pick which pins do what and how fast the clock
  runs, click **Generate Code**, and it writes all the tedious startup C code for you (setting up
  clocks, enabling the GPIO peripheral, etc.). It generates code that uses the **HAL**.
- **HAL (Hardware Abstraction Layer)** — a library of C functions ST provides so you write
  `HAL_GPIO_TogglePin(...)` instead of memorizing register addresses and bit layouts. It's the
  "human-friendly wrapper" over the memory-mapped registers from 0.2.
- **STM32CubeIDE** — where you write your own logic, **build** (compile) it, and **flash** it onto
  the chip, and later **debug** it.

Division of labour, and you'll use it for the whole project: **CubeMX = configure the hardware →
generate. CubeIDE = write logic, build, flash, debug.**

### 0.5 How code physically gets onto the chip (ST-Link + SWD)

Your Discovery board has a second chip on it (top section) called the **ST-Link**. It's a built-in
*programmer/debugger*. When you plug the ST-LINK USB port into your PC and hit Run:

1. CubeIDE hands the compiled program to the ST-Link over USB.
2. The ST-Link talks to the main STM32 over a 2-wire protocol called **SWD** (Serial Wire Debug —
   just two signal wires, already routed on the board between the two chips).
3. Over SWD it **writes your program into the STM32's flash**, then resets the chip so it runs.

SWD also lets you **pause the running CPU, step line by line, and inspect variables** — that's
debugging, which you'll use heavily from Phase 2 on. This is why the on-board ST-Link matters: no
separate programmer gadget needed.

That's the whole mental model. Now the steps make sense.

---

# Part 1 — Install the toolchain (two programs)

> **Version note:** Since STM32CubeIDE **2.0** (you have 2.2.0), ST **split CubeMX out of the IDE**.
> So you install two programs: **CubeMX** (configure + generate) and **CubeIDE** (write/build/flash).
> Older tutorials show CubeMX built into the IDE — that's pre-2.0 and no longer how it works.

**Do:**
- STM32CubeIDE: https://www.st.com/en/development-tools/stm32cubeide.html → Get Software → install.
  Accept the **ST-Link USB driver** prompt during install.
- STM32CubeMX: https://www.st.com/en/development-tools/stm32cubemx.html → Get Software → install.
  (Same free ST account. It bundles its own Java; you don't install Java yourself.)

**Why:** these are the whole free toolchain. Nothing else is needed to build and flash.

**Under the hood:** CubeIDE bundles the actual compiler — **arm-none-eabi-gcc**, a version of the GCC
compiler that produces ARM machine code (not x86 code for your PC — code for the Cortex-M4). It also
bundles the ST-Link flashing tools and a debugger (GDB). The ST-Link *driver* is what lets Windows
recognize the board when you plug it in.

---

# Part 2 — Orient on the board

**Do:** find the four user LEDs (green **PD12**, orange **PD13**, red **PD14**, blue **PD15**), the
blue user button **B1 (PA0)**, and the two USB ports. Plug the port labelled **ST-LINK** into your PC.

**Why:** you need to know which pin drives which LED, because in software you address the *pin*, not
the LED. And you must use the ST-LINK USB port specifically — it's the one wired to the programmer.

**Under the hood:** "PD12" means **P**ort **D**, pin **12**. The STM32 groups its pins into ports
named A through I, each port controlling up to 16 pins. Port D's registers control pins PD0–PD15.
So "green LED on PD12" tells you: to light it, set bit 12 in Port D's output register (exactly the
example from 0.2). The LED is wired on the board between the pin and ground through a resistor, so a
HIGH pin (3.3 V) pushes current through it and it lights. That's why these LEDs are **active-high**:
1 = on.

---

# Part 3 — Create the project in STM32CubeMX

**Do:**
1. Open CubeMX → **ACCESS TO BOARD SELECTOR** → search **STM32F407G-DISC1** → select → **Start
   Project**. (You used **ACCESS TO MCU SELECTOR → STM32F407VGTx** instead — that's the identical
   chip, perfectly fine.)
2. "Initialize all peripherals with their default Mode?" → **No**.

**Why:** this tells CubeMX which exact chip/board you have so it generates the right code (correct
memory sizes, correct available pins). Saying **No** to auto-init gives you a **clean slate** — nothing
configured except what *you* choose — so you actually see and understand each pin you turn on,
instead of inheriting a pile of pre-configured peripherals you didn't ask for.

**Under the hood:** CubeMX now knows the chip's full "database" — every pin, every peripheral, the
memory map, the max clock. The `.ioc` file it creates is just a saved record of your choices; when you
Generate Code, CubeMX reads the `.ioc` and writes matching C code. Board-selector vs MCU-selector only
differs in that the board version would have pre-labelled the on-board LEDs/button for you. You
labelled them yourself — which is more instructive.

---

# Part 4 — Configure the LED pin, then generate

**Do (pin):**
1. Left-click **PD12** on the chip diagram → **GPIO_Output**. It turns green.
2. Right-click PD12 → **Enter User Label** → `LED_GREEN`.

**Why (pin):** a physical pin can behave as an input (reads a voltage) or an output (drives a
voltage), among other modes. The LED needs the pin to *drive* voltage, so you set it to **output**.
The **label** just makes your code readable: CubeMX will create `LED_GREEN_Pin` and
`LED_GREEN_GPIO_Port` so you never hand-type `GPIO_PIN_12` / `GPIOD`.

**Under the hood (pin):** choosing GPIO_Output makes CubeMX generate code that, at startup,
configures PD12's *mode* registers: it enables Port D's clock (a peripheral is asleep until its
clock is switched on — a power-saving design), sets PD12 to push-pull output, default speed. After
that, your program can drive it HIGH/LOW anytime. The label becomes `#define` lines in `main.h`:
`#define LED_GREEN_Pin GPIO_PIN_12` and `#define LED_GREEN_GPIO_Port GPIOD`. So `LED_GREEN_Pin` is
literally the number `(1<<12)` and `LED_GREEN_GPIO_Port` is a pointer to Port D's registers.

**Do (generate):**
3. **Project Manager** tab → **Project Name** `phase01_blink`, **Project Location** = your repo's
   `firmware` folder, **Toolchain/IDE** = **STM32CubeIDE**.
4. **GENERATE CODE**. Approve the F4 firmware-package download if asked. Then **Open Project**.

**Why (generate):** this turns your visual choices into an actual C project the IDE can build.
Putting it in `firmware/` means git tracks it. Setting Toolchain = STM32CubeIDE makes CubeMX lay out
the project files in the format CubeIDE expects.

**Under the hood (generate):** CubeMX writes a full project: the **HAL library source** (the driver
functions), a **startup file** (assembly that runs first on reset), a **linker script** (`.ld` —
tells the compiler *where* in flash/RAM to place code and variables), and **`main.c`** with your
config baked into init functions. The "firmware package" it downloads is **STM32Cube F4** — ST's
official HAL + register definitions for the F4 family. It's downloaded once and reused.

---

# Part 5 — Write the blink code (in the exact protected spot)

**Do:** open `phase01_blink → Core → Src → main.c`. Ctrl+F for `USER CODE BEGIN 3`. Make it read:

```c
    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    HAL_Delay(500);
  }
  /* USER CODE END 3 */
```

(If you didn't add the label, use `GPIOD, GPIO_PIN_12` instead of the two `LED_GREEN_...` names.)
Save with **Ctrl+S**.

**Why the exact spot:** CubeMX *regenerates* `main.c` every time you change the `.ioc` and re-generate.
It only preserves text **between** the `USER CODE BEGIN … / … END` comment markers. Anything outside
them is **erased and rewritten**. `BEGIN 3 / END 3` sits *inside* the `while(1)` loop, so your two
lines run forever and survive regeneration. Put code outside the markers and you'll lose it later and
not know why — this is the single most common STM32 beginner mistake.

**Under the hood (the two lines):**
- `HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin)` — reads PD12's current output state and
  flips it: if it was HIGH it drives LOW, if LOW it drives HIGH. Internally it writes Port D's set/reset
  register (**BSRR**) to change just that one bit atomically. So each time this line runs, the LED
  switches on↔off.
- `HAL_Delay(500)` — pauses the CPU for **500 milliseconds** (half a second). Without a delay, the
  toggle would run millions of times per second and the LED would look permanently half-lit. The delay
  is what makes the blink visible.
- Net effect: on → wait 0.5 s → off → wait 0.5 s → on… = a **1 Hz blink** (one full on-off cycle per
  second).

---

# Part 6 — Build and flash

**Do:**
1. **Ctrl+B** (Build). Watch the **Console** end with `Build Finished. 0 errors`.
2. Green **Run ▶** button. First time: an "Edit Configuration" dialog → click **OK**/**Run**.
3. Green LED (**LD4**) blinks once per second.

**Why:** "Build" turns your C into a binary the chip can run. "Run" copies that binary onto the chip
and starts it. Two separate actions — building doesn't touch the board; running does.

**Under the hood:**
- **Build = compile + link.** The compiler (`arm-none-eabi-gcc`) translates each `.c` file into
  Cortex-M4 machine-code object files. The **linker** then stitches them together and, using the
  linker script, assigns real addresses — code into flash (starting at address `0x0800_0000`),
  variables into RAM (starting at `0x2000_0000`) — producing a single **`.elf`** file (your complete
  program). `0 errors` means the C is valid and everything resolved.
- **Flash = program + reset.** CubeIDE tells the ST-Link (via USB → SWD) to erase the relevant flash
  pages and write your program into flash, then pulse reset. On reset the Cortex-M4 jumps to the
  startup code (`startup_stm32f407vgtx.s`), which sets up the stack, calls `SystemInit()`, copies
  initialised data into RAM, zeroes `.bss`, then calls `main()`. `main()` itself then calls
  `HAL_Init()`, `SystemClock_Config()` and the `MX_..._Init()` functions. Your `while(1)` starts looping, and the LED blinks. It now runs **every time the board is
  powered**, with or without the PC, because it's stored in non-volatile flash.

---

# Part 7 — Read the generated `main.c` (understand what's running)

Open `main.c` and read `main()` top to bottom. Four things matter:

1. **`HAL_Init();`** — boots the HAL and starts the **SysTick** timer. SysTick is a tiny counter
   inside the Cortex-M core that HAL sets to fire an interrupt **every 1 millisecond**. That millisecond
   tick is the "clock" `HAL_Delay` counts. So `HAL_Delay(500)` = "wait for 500 SysTick ticks."
2. **`SystemClock_Config();`** — sets the chip's clock speed. With your default settings this selects
   the internal **16 MHz** oscillator (HSI) as the system clock. *This* is the function you'd edit
   later to reach 168 MHz. It also tells the HAL what the clock speed is, so timing stays accurate.
3. **`MX_GPIO_Init();`** — the generated function that configures PD12: it switches on Port D's clock
   and sets PD12 as a push-pull output. This is literally your two CubeMX clicks turned into C. Open it
   and read it — you'll recognize `GPIO_PIN_12`, `GPIOD`, `GPIO_MODE_OUTPUT_PP`.
4. **`while (1) { … }`** — the **super-loop**. Bare-metal firmware (no operating system) runs forever
   in one loop. Everything the robot ever does hangs off a loop like this.

> **Bookmark this for Phase 5:** `HAL_Delay` is a **blocking** delay — during those 500 ms the CPU sits
> and does nothing else. Fine for blinking. **Fatal for a balancer**, which must run its control math at
> a precise, fixed rate no matter what. In Phase 5 you'll replace blocking delays with a **timer
> interrupt** that fires exactly every 5 ms and runs the control loop. Notice now *why* the blink
> structure won't scale — that realization is the bridge to real-time control.

---

# Part 8 — Stretch goals (do these; they teach more than the blink)

**8a. Sweep all four LEDs.** Add PD13/PD14/PD15 as outputs in CubeMX (label them), regenerate, and in
the loop toggle them in turn with short delays.
- **Why/under the hood:** proves you can drive several pins independently and reinforces that each pin
  is just another bit in the port's output register.

**8b. Read the button (your first *input*).** Add **PA0** as **GPIO_Input** in CubeMX, regenerate. In
the loop:
```c
if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);   // pressed → LED on
} else {
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET); // released → LED off
}
```
- **Why/under the hood:** `HAL_GPIO_ReadPin` reads the pin's **Input Data Register (IDR)** bit — the
  chip senses the voltage on PA0 and reports 1 (HIGH) or 0 (LOW). On this board the button is
  **active-high**: pressing it connects PA0 to 3.3 V, so pressed = `GPIO_PIN_SET`. This is the exact
  same "read a register bit" idea you'll use for the IMU in Phase 2 — reading the physical world into
  software. `HAL_GPIO_WritePin(..., SET/RESET)` is the non-toggling cousin of `TogglePin`: it forces
  the pin HIGH or LOW explicitly.

---

# Part 9 — Commit + learning log (this is what makes it a portfolio)

**Do:**
```bash
git add .
git commit -m "phase1: blink + button input on F407-DISC1 (toolchain verified)"
```
Then fill in today's `LEARNING_LOG.md` entry: what you did, the concept you learned, anything that
broke and how you fixed it.

**Why:** the commit history and log are *evidence of how you think and debug* — which is what a
Master's admissions reviewer actually reads, more than the fact that an LED blinked. Every phase ends
this way.

---

## ✅ Phase 1 checkpoint — you're done when:

- [ ] Both CubeMX and CubeIDE installed; board detected
- [ ] Green LED blinks at 1 Hz
- [ ] Button (PA0) turns an LED on/off (8b)
- [ ] First firmware commit made + LEARNING_LOG entry written
- [ ] You can explain, in your own words: what a register is, what GPIO output vs input means, and
      why `HAL_Delay` won't work for the control loop

Next: **Phase 2 — read the MPU6050 IMU over I2C.** That's where you meet your first *communication*
peripheral and the pin-collision gotcha (avoid I2C1 on PB6/PB9 — the on-board audio DAC sits there).

---

## Appendix — Flashing / ST-Link troubleshooting (you'll reuse this every phase)

Flashing means "PC → ST-Link (over USB) → your STM32 (over SWD)." A failure is always in one of
those hops. Diagnose in this order:

**A. "No ST-LINK detected" — the PC can't see the programmer (first hop).**
- Wrong USB socket: use the **ST-LINK** port (top of the board), *not* the USB-OTG port (bottom) —
  the OTG port powers the board but can't program it.
- **Charge-only cable:** the #1 silent cause. A cable can power+detect the board yet not carry data.
  Swap to a known-good **data** cable, plugged **directly** into the PC (no hub).
- Check Windows **Device Manager → Universal Serial Bus devices** for "STMicroelectronics STLink".

**B. "ST-LINK firmware verification / upgrade" errors (e.g. "Error after GoToUsbLoader command").**
- CubeIDE wants a newer ST-Link firmware. The firmware is on the *ST-Link chip*, separate from your
  STM32 program — **upgrading it does not erase your code.**
- The upgrade fails at "GoToUsbLoader" (a USB re-enumeration) when the cable/port is marginal. Fix:
  close all other ST tools (only one app can own the ST-Link), swap to a solid data cable, plug
  direct into the PC (prefer a USB 2.0 port), power-cycle the board, then retry the upgrade.

**C. "Failed to start GDB server / Error initializing ST-LINK device / Failed to connect to device —
check power and cabling to target" — ST-Link works but can't reach the STM32 (second hop).**
- **Power-cycle** first: unplug, wait 10 s, replug direct with a good cable; retry Run.
- **Connect under reset** (most common software fix): Run → Debug Configurations → phase01_blink →
  Debugger tab → set reset mode to **Connect under reset** → Apply → Debug. This holds the chip in
  reset while grabbing the debug link.
- **Board power jumpers:** **CN3** (two "ST-LINK" jumpers, connect ST-Link↔target) and **JP1 (IDD)**
  (feeds power to the STM32) must both be **fitted**. If JP1 is off, the chip has no power → this
  exact error.
- Last resorts: lower the SWD frequency in the Debugger tab; try a different PC.

> _Phase 1 note: this is what got the green LED flashing after the ST-Link initially wouldn't connect._

## Mini-glossary

- **MCU** — microcontroller: CPU + memory + peripherals on one chip.
- **Register** — a fixed-address 32-bit slot that controls or reports on hardware.
- **GPIO** — General-Purpose Input/Output: a pin you can drive or read in software.
- **HAL** — Hardware Abstraction Layer: ST's C library that wraps registers in readable functions.
- **Port / pin** — pins are grouped into ports A–I; "PD12" = port D, pin 12.
- **Push-pull output** — an output that can actively drive both HIGH and LOW.
- **SysTick** — core timer HAL ticks every 1 ms; the basis of `HAL_Delay`.
- **Super-loop** — the `while(1)` that bare-metal firmware runs forever.
- **ST-Link** — on-board programmer/debugger chip.
- **SWD** — Serial Wire Debug: 2-wire protocol ST-Link uses to flash and debug the STM32.
- **`.elf`** — the compiled program file that gets flashed.
- **Flash vs RAM** — flash = non-volatile program storage; RAM = volatile variable storage.

---

## Sources

- **STM32CubeIDE**: https://www.st.com/en/development-tools/stm32cubeide.html
- **STM32CubeMX**: https://www.st.com/en/development-tools/stm32cubemx.html
- **Why two tools now (CubeMX separated in v2.0):** ST "What's new in STM32CubeIDE 2.0.0".
- **UM1472** — STM32F407 Discovery board user manual (pins, LEDs, button, ST-Link).
- **RM0090** — STM32F4 Reference Manual (the authoritative register-level reference; your deep-dive
  source when you want to see what a HAL function touches).
- Optional video backup (only if a concept won't click from text): DigiKey "Introduction to STM32",
  ControllersTech (per-peripheral HAL how-tos), Phil's Lab (firmware architecture).
