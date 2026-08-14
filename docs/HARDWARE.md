# AURA OS — Hardware

This document describes the hardware for **AURA OS** at two levels:

1. **Current prototype (V1, MK-II)** — the hardware actually supported and
   tested by the current firmware build (ESP32-WROOM-32).
2. **Planned (V2)** — the next-generation hardware bill of materials
   (ESP32-P4 + ESP32-CAM) as listed in `BOM.csv`. None of the V2 components
   have firmware support in the current build.

Wiring and pinout live in [WIRING.md](WIRING.md); the full parts list lives in
[PARTS.md](PARTS.md).

---

## Current prototype (V1)

The firmware is built and validated against an **ESP32-WROOM-32** (38-pin)
development board using the Arduino-ESP32 core `3.3.11`. The following
components are supported and verified. Interface and pin details live in
[WIRING.md](WIRING.md); the parts list lives in [PARTS.md](PARTS.md).

| Component | Function | Interface | Status |
| --- | --- | --- | --- |
| ESP32-WROOM-32 (38-pin) | Main MCU (Wi-Fi + BLE) | on-board | Ready |
| SH1106/SSD1306 OLED, 128×64 | Display (face, clock, states) | I2C (`0x3C`) | Ready |
| INMP441 I2S MEMS microphone | Voice input | I2S | Ready |
| MAX98357A I2S amp + speaker | Voice output | I2S | Hardware-dependent (see note) |
| WS2812B 16-LED ring | Status indication | GPIO | Ready |
| TTP223 capacitive touch | Input (tap / double-tap / hold) | GPIO, active-high | Ready |
| microSD / SD module | Optional storage | SPI | Hardware-dependent (card optional) |
| Wi-Fi + Bluetooth | Networking | on-board | Ready |

> **Speaker note.** The shipped firmware is compiled with
> `AURA_HW_SPEAKER_PRESENT = 0` (see the Instruction Manual §3). The I2S
> speaker stage is disabled and the boot log reports "Speaker output skipped
> (not connected)". Spoken replies are **not** heard until a MAX98357A (or
> equivalent) is connected and the build flag is enabled. Microphone, OLED,
> LED ring, touch, Wi-Fi, and storage are unaffected.

> **Not in the current firmware** (parts-list only): a 5" touchscreen, GPS,
> NFC, camera, RTC module, IMU, distance sensor, ambient-light sensor, and
> temperature/humidity/pressure sensor. None of these have firmware support in
> the current build and are not documented as working functions.

### Headless mode

The firmware can run on a **bare ESP32-WROOM-32 with only a USB cable** — no
peripherals attached. When a peripheral is missing the firmware logs a
warning, disables only that module, and continues booting (`HEADLESS_MODE_AUTO`
probes the OLED at boot; `HEADLESS_MODE_FORCE` forces headless even when all
hardware is present). See `README.md` → "Headless development mode".

---

## Planned (V2)

The V2 hardware is **planned only** (target design sourced from `BOM.csv`);
no V2 component is implemented in the current firmware. The V2 design moves
to an **ESP32-P4** main controller paired with an **ESP32-CAM** as the
planned camera-capable controller, adds a 5.0" IPS capacitive touch display,
sensors (BME280, BH1750, BMI270, VL53L0X), GPS, RTC, NFC, and battery power —
all beyond the scope of the current build. The ESP32-CAM has **not** been
purchased or tested; no camera sensor, pinout, or model is specified.

### Hardware progression

- **ESP32-WROOM-32** — current/prototype hardware used for testing (concept
  proven; see "Current prototype (V1)" above).
- **ESP32-CAM** — planned camera-capable hardware (not yet purchased/tested).

See [PARTS.md](PARTS.md) for the full parts list and [WIRING.md](WIRING.md)
for the verified current-wiring pinout.
