# Phase 1 record: toolchain, GPIO output and button input

**Dates:** 2026-09-12 (built), 2026-11-23 (button fault found and fixed).
**Firmware:** [`firmware/phase01_blink`](../firmware/phase01_blink). **Learning log:** Phase 1 entry.

## Objective
Prove the build, flash and debug path to the STM32F407G-DISC1, drive an on-board LED, and read the
user button.

## Configuration
| Item | Setting |
|------|---------|
| Clock | 16 MHz HSI, no PLL (CubeMX default) |
| Outputs | PD12 `LED_GREEN`, PD13 `LED_ORANGE`, PD15 `LED_BLUE`: push-pull, start low |
| Input | PA0 `BTN_USER` (user button B1), no internal pull resistor: B1 connects PA0 to 3.3 V when pressed (active high) |
| Debugger | ST-LINK on the board, "Connect under reset" |

## Implementation
The main loop reads PA0. While it reads high, green, orange and blue are on; otherwise all three
are off. The first version, a 1 Hz toggle of PD12 with `HAL_Delay(500)`, was replaced by this code
and was not committed.

## Results
Bare board, USB only. Each flash confirmed by "Download verified successfully".

| Date | Firmware | Result |
|------|----------|--------|
| 2026-09-12 | 1 Hz toggle (not committed) | Green LED blinked at 1 Hz. |
| 2026-11-23 | September project as committed, flashed unchanged | **No LED lit**: all four off before pressing, while B1 was held, and after release (3 tries). |
| 2026-11-23 | Fixed project (below) | Green, orange and blue on while B1 is held, all off on release; red (PD14, unused) off throughout (3 tries). |

## Problem found (2026-11-23)
The committed September project did not configure the pins its code used:
- PA0 was not in the CubeMX configuration, so the GPIOA clock was never enabled and PA0 never set
  up. `HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)` therefore always returned 0, and the LEDs never turned on.
- PD13 (orange) was written but never configured as an output, and was never switched off.

The learning log and README had described the button lighting three LEDs; that description came
from reading the code, not from a test of the committed project. Whether an earlier, uncommitted
version worked is not known.

**Fix:** PA0 added in CubeMX as GPIO_Input (`BTN_USER`) and PD13 as GPIO_Output (`LED_ORANGE`),
code regenerated; the loop uses the CubeMX labels and switches all three LEDs off on release.

## Problems found during the build (September)
"No ST-LINK detected", then a "GoToUsbLoader" error during the ST-LINK firmware upgrade, then
"failed to connect to target". Fixed by setting the debugger to **Connect under reset**.
