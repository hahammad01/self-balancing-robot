# Learning Log

One dated entry per working session. Keep it honest and specific: what you *did*,
what you *learned* (the concept, not just the action), what broke and how you fixed it,
and what you didn't understand yet. This file is as much a portfolio artifact as the code —
it shows how you think and debug.

Template for each entry:

```
## YYYY-MM-DD — Phase N: <title>
\\\*\\\*Did:\\\*\\\* 
\\\*\\\*Learned (the why):\\\*\\\* 
\\\*\\\*Broke / debugged:\\\*\\\* 
\\\*\\\*Open questions:\\\*\\\* 
\\\*\\\*Commit:\\\*\\\* <short hash / message>
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



**## 2026-09-13 — Phase 2: IMU over I2C → UART (complete)**

**\*\*Did:\*\* Wired an MPU6050 module to the STM32 over I2C (SCL=PB8, SDA=PB7, AD0→GND for**

**address 0x68) and an FTDI adapter for serial out (PA2→FTDI RX, common GND). Configured**

**I2C1 + USART2 (115200) in CubeMX. Wrote code to check WHO\_AM\_I, wake the sensor**

**(PWR\_MGMT\_1=0), read 6 accel + 6 gyro bytes, reassemble them into signed 16-bit values,**

**and stream them over UART to Tera Term. Verified live: flat sensor reads \~1g on Z, and**

**values respond correctly to tilt and rotation. Pushed the repo to GitHub.**



**\*\*Learned (the why):\*\***

**- I2C is a 2-wire master/slave bus (SDA data, SCL clock); devices have addresses (0x68) and**

&#x20; **register-based comms — you read/write numbered registers.**

**- HAL wants the address shifted left by 1 (0x68<<1) because the bottom bit is the R/W flag.**

**- Each axis is 16 bits sent as two bytes, high byte first (big-endian) → rebuild with**

&#x20; **(int16\_t)((hi<<8)|lo). The int16\_t cast makes it signed so tilt-direction gives +/-.**

**- Accel gives absolute tilt (feels gravity) but is noisy; gyro is smooth but has a constant**

&#x20; **bias (\~-2.6°/s at rest) that would make an integrated angle drift. This is the reason**

&#x20; **Phase 3 needs sensor fusion.**

**- The F407-Discovery has no built-in USB-serial, so an FTDI adapter is needed to see UART.**



**\*\*Broke / debugged:\*\***

**1. Build error "huart2 undeclared / implicit HAL\_UART\_Transmit" → I hadn't enabled USART2 in**

&#x20;  **CubeMX, so the handle didn't exist. Rule learned: an undeclared hX handle = peripheral not**

&#x20;  **enabled in CubeMX.**

**2. "MPU6050 NOT found (got 0x70)" → NOT a wiring fault. Reading the actual value showed the**

&#x20;  **bus works and a chip answered — it's an MPU6500 clone (0x70), not a real MPU6050 (0x68).**

&#x20;  **Register-compatible, so I just accepted 0x70. Lesson: read the actual value, don't just**

&#x20;  **react to "not found".**

**3. "found" printed but no data → I'd put the loop code between END WHILE and BEGIN 3 (outside**

&#x20;  **the USER CODE markers), so regenerating for USART2 wiped it. Moved it inside BEGIN 3/END 3.**

&#x20;  **Rule: all my code goes between USER CODE BEGIN/END or it gets deleted on regenerate.**



**\*\*Debugging method that worked:\*\* the "found/NOT found" print isolates UART from I2C — if it**

**prints at all, UART works, so the problem is I2C or logic. Cut every problem in half.**



**\*\*Open questions:\*\* \[what you're still unsure about — e.g. exactly what the WHO\_AM\_I register**

**is for, or how the I2C repeated-START in HAL\_I2C\_Mem\_Read works]**



**\*\*Commit:\*\* \[paste short hash from `git log --oneline -1`]**

