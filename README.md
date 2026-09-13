# Self-Balancing Robot — STM32F407

A two-wheeled self-balancing robot built on an STM32F407 as a hands-on study of
**real-time embedded control**. The headline of this project is not "it balances" —
that is the baseline. The headline is a **quantitative comparison of a PID controller
against an LQR controller** on the same physical plant, with measured step-response
and disturbance-rejection plots.

> Built as a portfolio project for a Master's application (embedded / control systems / robotics).

---

## Status

| Phase | Goal | State |
|-------|------|-------|
| 1 | Toolchain + blink (prove the flash/debug path) | ✅ done |
| 2 | Read the IMU (I2C → MPU6050, raw data over UART) | ✅ done |
| 3 | Tilt estimation (complementary filter → Kalman), logged + plotted | ⬜ not started |
| 4 | Drive motors (PWM + TB6612, read encoders), open-loop verify | ⬜ not started |
| 5 | Close the loop: PID on a fixed-rate timer ISR; first balance | ⬜ not started |
| 6 | Add outer velocity loop + LQR; **PID vs LQR comparison** | ⬜ not started |
| 7 | Document: report, plots, wiring diagram, demo GIF | ⬜ not started |

---

## Hardware

Core: **STM32F407G-DISC1** (on-board ST-Link/V2 — no external programmer needed).
Full bill of materials and open items: [`docs/hardware-inventory.md`](docs/hardware-inventory.md).

## Toolchain

- **STM32CubeIDE** — firmware, HAL config, build, flash, SWD debug
- **Git** — one commit per phase milestone, with notes
- **python-control** + numpy/scipy/matplotlib — control design and plots (free MATLAB replacement)
- **Overleaf / LaTeX** — final report

## Repo layout

```
docs/        phase guides, hardware inventory, wiring diagrams
firmware/    STM32CubeIDE projects (one per phase or evolving)
control/     python-control scripts, system-ID data, plots
media/       demo GIFs, photos, result plots
LEARNING_LOG.md   dated log of what was learned and why
```

## Results

_(Plots, numbers — settling time, overshoot, loop rate — and the demo GIF land here as phases complete.)_
