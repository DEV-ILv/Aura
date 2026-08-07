# AURA OS — Hardware Wiring (Verified)

Verified connections for the AURA OS MK-II hardware. These match the pin
definitions in `config.h` and have been validated on the physical build.

## Microphone — INMP441 (I2S)

| INMP441 pin | Connect to | ESP32 GPIO | config.h symbol |
| --- | --- | --- | --- |
| **VDD** | 3.3 V | 3V3 | — |
| **GND** | GND | GND | — |
| **SCK** (bit clock) | BCLK | **GPIO26** | `MIC_BCLK_PIN` |
| **WS** (word select) | LRCLK | **GPIO25** | `MIC_WS_PIN` |
| **SD** (serial data out) | DATA | **GPIO34** | `MIC_DATA_PIN` |

**L/R select → GND** — ties the mic to the *left* channel for mono capture.
This is required; a floating or high L/R select leaves the left slot high and
produces saturated/noise readings.

The firmware captures **mono, 16 kHz, Philips I2S, 32-bit slot** from the left
channel (see `audio_manager.cpp` / `config.h`).

```
INMP441                     ESP32-WROOM-32
┌─────────────┐             ┌───────────────┐
│ VDD         ├────────────►│ 3V3           │
│ GND         ├────────────►│ GND           │
│ SCK (BCLK)  ├────────────►│ GPIO26        │
│ WS (LRCLK)  ├────────────►│ GPIO25        │
│ SD (DATA)   ├────────────►│ GPIO34        │
│ L/R         ├──── to GND ─┘ (left channel) │
└─────────────┘             └───────────────┘
```

## Speaker — MAX98357A (I2S)

| MAX98357A pin | ESP32 GPIO | config.h pin |
| --- | --- | --- |
| BCLK | **GPIO27** | `SPK_BCLK_PIN` |
| LRC | **GPIO14** | `SPK_LRC_PIN` |
| DIN | **GPIO12** | `SPK_DATA_PIN` |

## Other peripherals

| Component | Interface | GPIO | config.h pin |
| --- | --- | --- | --- |
| SSD1306 OLED (0x3C) | I2C | SDA/SCL (board I2C bus) | — |
| LED ring (WS2812-style) | GPIO | **GPIO4** | — |
| Touch sensor (TTP223, active-high) | GPIO | **GPIO13** | `TOUCH_PIN` |
| MicroSD card | SPI | CS **GPIO5**, MOSI **GPIO23**, MISO **GPIO19**, SCK **GPIO18** | `SD_*` / SD SPI |

## Notes

- All configured pins are defined in `config.h`; keep the firmware and this
  document in sync.
- GPIO34 is input-only (used for the microphone data line only).
- I2S uses two peripherals: microphone on `I2S_NUM_0` (GPIO 26/25/34) and
  speaker on `I2S_NUM_1` (GPIO 27/14/12) — they do not conflict.