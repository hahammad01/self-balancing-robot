# Hardware Inventory

## On hand (procured as of Sep 2026)

| Item | Part | Role in the robot |
|------|------|-------------------|
| MCU board | **STM32F407G-DISC1** | The brain. On-board ST-Link/V2 = flashing + SWD debug over one USB cable, no external programmer. |
| Secondary MCU | ESP32 | Parked for later (Wi-Fi/BLE telemetry). **Not used in the balancer.** |
| Motors | Geared DC motors **with quadrature encoders** | Drive wheels. Encoders are required for the outer velocity/position loop (Phase 6). |
| Motor driver | TB6612FNG | H-bridge, drives both motors from PWM + direction pins. |
| IMU | MPU6050 | 3-axis accel + 3-axis gyro over I2C. Source of the tilt angle. |
| Battery | 3S Li-ion (~11.1 V nominal) | Main power. ⚠️ see open items. |
| Regulator | Buck converter | Steps battery down to logic voltage (feeds 5V/3.3V rails). |
| UART | FTDI USB-UART adapter | Spare serial console. **Not needed to program** (DISC1 has ST-Link). |
| Misc | Wheels, chassis, breadboards, jumper wires, soldering iron, DMM, potentiometer, tactile button | Build + bring-up + live PID tuning (pot) + reset/mode (button). |

## STM32F407G-DISC1 — on-board resources to know

| Resource | Pin(s) | Note |
|----------|--------|------|
| Green LED (LD4) | PD12 | User LED |
| Orange LED (LD3) | PD13 | User LED |
| Red LED (LD5) | PD14 | User LED |
| Blue LED (LD6) | PD15 | User LED |
| User button (B1) | PA0 | **Active HIGH** — pressing drives the pin to 3.3V (it's the WKUP pin). |
| On-board ST-Link | USB "ST-LINK" port (mini/micro-USB) | Flash + debug. |
| Audio DAC (CS43L22) | I2C1 = **PB6 (SCL) / PB9 (SDA)** | ⚠️ Avoid putting the MPU6050 on these default pins — bus collision. Use a different I2C mapping. |
| MEMS sensor | SPI | On-board, leave unconfigured. |

> Pin assignments per ST's board user manual **UM1472** ("Discovery kit with STM32F407VG MCU").
> Confirm against your board revision before wiring anything in Phase 2.

## ⚠️ OPEN ITEMS — resolve before Phase 4 (motor power)

1. **Battery protection + charging (SAFETY-CRITICAL).** Confirm whether the 3S pack has a
   BMS/protection board, and how it will be charged (a balance charger such as an iMAX B6-type
   is required for a bare 3S pack). A bare, unprotected 3S Li-ion pack is a fire / over-discharge
   hazard. **Not a blocker for Phases 1–3 (all USB-powered).**
2. **Power switch.** A physical slide/rocker switch to cut battery power (separate from the tactile button).
3. **Bulk capacitor.** ~470–1000 µF electrolytic across the TB6612 motor supply rail (VM) to absorb
   current spikes during the frequent direction reversals a balancer makes (prevents brownout resets).
4. **Motor current check.** Confirm motor stall current vs TB6612FNG limits (~1.2 A continuous /
   ~3.2 A peak per channel). If stall exceeds the peak, the driver is undersized.
5. Optional: inline fuse / XT60 connector on the battery.
