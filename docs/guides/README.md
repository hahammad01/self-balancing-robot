# Build guides

Step-by-step instructions followed while building each phase, kept for reference. They are
written as instructions to the builder, not as a record of results.

What was actually built, configured, measured and corrected in each phase is in the phase
records in [`docs/`](..) and in [`LEARNING_LOG.md`](../../LEARNING_LOG.md). Where a guide and a
record disagree, the record and the committed code are correct.

| Guide | Phase |
|-------|-------|
| [`phase-01-toolchain-blink.md`](phase-01-toolchain-blink.md) | 1: toolchain, GPIO |
| [`phase-02-imu-i2c-uart.md`](phase-02-imu-i2c-uart.md) | 2: IMU over I2C, UART output |
| [`phase-03-tilt-estimation.md`](phase-03-tilt-estimation.md) | 3: complementary and Kalman filters |
| [`phase-04-motors-encoders.md`](phase-04-motors-encoders.md) | 4: PWM motor drive, quadrature encoders (written for two motors; built with motor A) |
| [`complementary-vs-kalman-notes.docx`](complementary-vs-kalman-notes.docx) | 3: notes on reading the filter plots |
