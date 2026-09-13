# Learning Log

One dated entry per working session. Keep it honest and specific: what you *did*,
what you *learned* (the concept, not just the action), what broke and how you fixed it,
and what you didn't understand yet. This file is as much a portfolio artifact as the code —
it shows how you think and debug.

Template for each entry:

```
## YYYY-MM-DD — Phase N: <title>
\*\*Did:\*\* 
\*\*Learned (the why):\*\* 
\*\*Broke / debugged:\*\* 
\*\*Open questions:\*\* 
\*\*Commit:\*\* <short hash / message>
```

\---

## 2026-09-12 — Phase 1 kickoff

**\*\*Did:\*\* Installed STM32CubeMX + STM32CubeIDE 2.2.0. Created phase01\_blink in CubeMX**

**for the STM32F407, set PD12 as GPIO\_Output (label LED\_GREEN), generated code, imported**

**to CubeIDE. Added HAL\_GPIO\_TogglePin + HAL\_Delay(500) in the while(1) loop. Built and**

**flashed over the on-board ST-Link. Green LED blinks at 1 Hz. Added PA0 button input.**



**\*\*Learned (the why):\*\* Software controls hardware by writing bits to peripheral registers**

**(memory-mapped I/O) — HAL functions just wrap that. A GPIO output drives a pin to 3.3V/0V;**

**these LEDs are active-high. HAL\_Delay is BLOCKING and runs off the 1ms SysTick — it can't**

**drive a fixed-rate control loop, which is why Phase 5 needs a timer interrupt. Code must sit**

**between the USER CODE markers or CubeMX erases it on regeneration. The chip runs at 16 MHz**

**(internal HSI) by default, not its 168 MHz max.**



**\*\*Broke / debugged:\*\* ST-Link wouldn't connect: "No ST-LINK detected" → firmware upgrade**

**"GoToUsbLoader" error → "failed to connect to target". Fixed by \[WHICH FIX WORKED FOR YOU:**

**cable swap / power-cycle / Connect-under-reset]. Lesson: flashing failures are almost always**

**cable/port/connection, not code.**



**\*\*Open questions:\*\* \[what you're still unsure about — e.g. what SystemClock\_Config's PLL**

**actually does, or how the BSRR register differs from ODR]**



**\*\*Commit:\*\* \[paste the short hash from `git log --oneline -1`]**

