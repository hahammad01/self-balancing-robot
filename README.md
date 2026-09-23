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
| 3 | Tilt estimation (complementary filter → Kalman), logged + plotted | ✅ done |
| 4 | Drive motors (PWM + TB6612, read encoders), open-loop verify | ✅ done |
| 5 | Second motor + close the loop: PID on a fixed-rate timer ISR; first balance | ⬜ not started |
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

**Phase 3 — tilt estimation.** Complementary filter and a 2-state Kalman filter (angle + gyro bias)
run against the same logged IMU data. Gyro-only integration visibly drifts; accel-only is noisy but
unbiased; both fused estimates track ±60° steps cleanly with no overshoot. An `R_measure` sweep shows
the Kalman filter trading accelerometer-spike rejection against response lag.

**Phase 4 — open-loop drive.** Motor A under PWM + TB6612 with TIM3 hardware encoder feedback:
**+245 counts / 200 ms forward, −244 counts / 200 ms reverse** (≈1225 counts/s at 30 % duty), steady
across samples and symmetric in both directions, with measured direction matching the commanded one.

_(Step-response and disturbance-rejection plots, settling time, overshoot, loop rate, and the demo GIF
land here as Phases 5–7 complete.)_
