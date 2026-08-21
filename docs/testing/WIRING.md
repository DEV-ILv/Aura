# AURA OS â€” Hardware Wiring (Verified)

Verified connections for the AURA OS **MK-II prototype** (ESP32-WROOM-32).
These match the pin definitions in `config.h` and have been validated on the
physical build. This document supersedes the previous `hardware-wiring.md`
content and is the canonical wiring reference.

## Microphone â€” INMP441 (I2S)

| INMP441 pin | Connect to | ESP32 GPIO | config.h symbol |
| --- | --- | --- | --- |
| **VDD** | 3.3 V | 3V3 | â€” |
| **GND** | GND | GND | â€” |
| **SCK** (bit clock) | BCLK | **GPIO26** | `MIC_BCLK_PIN` |
| **WS** (word select) | LRCLK | **GPIO25** | `MIC_WS_PIN` |
| **SD** (serial data out) | DATA | **GPIO34** | `MIC_DATA_PIN` |

**L/R select â†’ GND** â€” ties the mic to the *left* channel for mono capture.
This is required; a floating or high L/R select leaves the left slot high and
produces saturated/noise readings.

The firmware captures **mono, 16 kHz, Philips I2S, 32-bit slot** from the left
channel (see `audio_manager.cpp` / `config.h`).

```
INMP441                     ESP32-WROOM-32
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”             â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ VDD         â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚ 3V3           â”‚
â”‚ GND         â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚ GND           â”‚
â”‚ SCK (BCLK)  â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚ GPIO26        â”‚
â”‚ WS (LRCLK)  â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚ GPIO25        â”‚
â”‚ SD (DATA)   â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚ GPIO34        â”‚
â”‚ L/R         â”œâ”€â”€â”€â”€ to GND â”€â”˜ (left channel) â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜             â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```

## Speaker â€” MAX98357A (I2S)

| MAX98357A pin | ESP32 GPIO | config.h symbol |
| --- | --- | --- |
| BCLK | **GPIO27** | `SPK_BCLK_PIN` |
| LRC | **GPIO14** | `SPK_LRC_PIN` |
| DIN | **GPIO12** | `SPK_DATA_PIN` |

## Other peripherals

| Component | Interface | GPIO / pins | config.h symbol |
| --- | --- | --- | --- |
| SSD1306 OLED (0x3C) | I2C | SDA **GPIO21**, SCL **GPIO22**, addr `0x3C` | `OLED_SDA_PIN`, `OLED_SCL_PIN`, `OLED_ADDRESS` |
| LED ring (WS2812-style) | GPIO | **GPIO4** | `LED_RING_PIN` |
| Touch sensor (TTP223, active-high) | GPIO | **GPIO13** | `TOUCH_PIN` |
| MicroSD card | SPI | CS **GPIO5**, MOSI **GPIO23**, MISO **GPIO19**, SCK **GPIO18** | `SD_CS_PIN`, `SD_MOSI_PIN`, `SD_MISO_PIN`, `SD_SCK_PIN` |

## Notes

- All configured pins are defined in `config.h`; keep the firmware and this
  document in sync.
- GPIO34 is input-only (used for the microphone data line only).
- I2S uses two peripherals: microphone on `I2S_NUM_0` (GPIO 26/25/34) and
  speaker on `I2S_NUM_1` (GPIO 27/14/12) â€” they do not conflict.
See [../architecture/HARDWARE.md](../architecture/HARDWARE.md) for the hardware overview and
[../development/PARTS.md](../development/PARTS.md) for the parts list.

