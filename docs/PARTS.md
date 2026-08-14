# AURA OS — Parts & BOM

This document lists the parts for the **AURA** project in two groups:

1. **Current prototype (V1)** — the components actually used and tested in the
   current ESP32-WROOM-32 build.
2. **Planned (V2)** — the target bill of materials (from `BOM.csv`). These are
   **planned** parts; none are implemented in the current firmware.

> The machine-readable `BOM.csv` at the repo root is the canonical source for
> the V2 bill of materials (category, part number, quantity, supplier,
> purpose). This page is a rendered summary and must stay in sync with it.

---

## Current prototype parts (V1)

| Component | Function |
| --- | --- |
| ESP32-WROOM-32 (38-pin, DevKit-C style) | Main MCU |
| SH1106/SSD1306 OLED, 128×64 | Display |
| INMP441 I2S MEMS microphone | Voice input |
| MAX98357A I2S amplifier (+ speaker) | Voice output (hardware-dependent) |
| WS2812B 16-LED ring | Status indication |
| TTP223 capacitive touch sensor | Touch input |
| microSD card + SPI SD module | Optional storage |
| USB-C / 5 V power, serial flashing rig | Power + programming |

See [HARDWARE.md](HARDWARE.md) for interface details and
[WIRING.md](WIRING.md) for the verified pinout.

---

## Planned V2 bill of materials (`BOM.csv`)

| # | Category | Component | Part number | Qty | Approx price (INR) | Supplier | Purpose |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Main Controller | ESP32-P4 Development Board | ESP32-P4 | 1 | 1600 | TBD | Main high-performance controller |
| 2 | Connectivity | ESP32-CAM (camera-capable controller) | ESP32-CAM | 1 | TBD | TBD | Camera-capable controller (planned) |
| 3 | Display | 5.0" IPS Capacitive Touch Display | 800×480 | 1 | 4499 | Waveshare | Primary user interface |
| 4 | Audio | I2S MEMS Microphone | INMP441 | 1 | 250 | Amazon | Voice input |
| 5 | Audio | I2S Audio Amplifier | MAX98357A | 1 | 220 | Amazon | Speaker amplifier |
| 6 | Audio | Premium Full-Range Speaker | FRS7 4 Ω 10 W | 1 | 950 | Mouser | Voice output |
| 7 | Lighting | RGB LED Ring | WS2812B 16 LED | 1 | 420 | Amazon | Status indication / Disco Mode |
| 8 | Storage | 32 GB microSD Card (microSDHC) | Digitek DTF 32GB C40 | 1 | TBD | TBD | Audio assets and storage |
| 9 | Storage | MicroSD Card Interface Module | SPI SD Module | 1 | 150 | Amazon | SD interface |
| 10 | Input | Capacitive Touch Sensor | TTP223 | 1 | 80 | Amazon | Touch wake/control |
| 11 | Sensors | Temperature/Humidity/Pressure | BME280 | 1 | 380 | Robu | Environmental monitoring |
| 12 | Sensors | Ambient Light Sensor | BH1750 | 1 | 180 | Robu | Automatic brightness |
| 13 | Sensors | 6-Axis IMU | BMI270 | 1 | 320 | Mouser | Motion detection |
| 14 | Sensors | Time-of-Flight Distance Sensor | VL53L0X | 1 | 450 | Robu | Proximity sensing |
| 15 | Position | GPS Module | NEO-6M | 1 | 650 | Amazon | Location services |
| 16 | Time | RTC Module | DS3231 | 1 | 250 | Amazon | Real-time clock |
| 17 | Connectivity | NFC Module | PN532 | 1 | 950 | Mouser | NFC communication |
| 18 | Camera | 5 MP Camera Module | OV5640 | 1 | 1850 | Arducam | Vision features |
| 19 | Power | 5000 mAh Li-ion Battery | INR21700-50E | 1 | 850 | Robu | Portable power |
| 20 | Power | USB-C Battery Charger Module | BQ24074 | 1 | 620 | Mouser | Battery charging |
| 21 | Power | Buck-Boost Regulator | TPS63070 | 1 | 780 | Mouser | Stable power supply |
| 22 | Power | USB-C Power Connector | USB-C Breakout | 1 | 120 | Amazon | Power input |
| 23 | Mechanical | 3D Printed Enclosure | Production Prototype Case | 1 | 1800 | Local | Protective enclosure |
| 24 | Mechanical | Screws and Standoffs | M2 Screw Kit | 1 | 300 | Amazon | Assembly |
| 25 | Mechanical | Connectors | PH2.0 Connector Kit | 1 | 450 | Amazon | Power and signal connections |
| 26 | Accessories | Programming and Power Cable | USB-C Cable | 1 | 250 | Amazon | Programming and power |
| 27 | Accessories | Card Reader | MicroSD Adapter | 1 | 80 | Amazon | PC data transfer |

> All V2 components are **planned only**. The current firmware supports none of
> them (see [HARDWARE.md](HARDWARE.md) → "Planned (V2)").
