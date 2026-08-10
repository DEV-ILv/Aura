Yes — you mean **one single block of text that you can copy once and paste directly into `README.md`**, without any surrounding explanation.

````markdown
# AURA — Smart Buddy

A modular smart-buddy platform built around an ESP32 controller, a 5-inch touchscreen, voice interaction, sensors, local device control, diagnostics, and a Companion App.

## Features

- 5-inch 800×480 IPS capacitive touchscreen
- INMP441 I2S MEMS microphone
- MAX98357A I2S audio amplifier
- External speaker
- 16× WS2812B RGB LED ring
- TTP223 capacitive touch sensor
- microSD storage
- Environmental and motion sensors
- GPS
- RTC
- Wi-Fi
- Companion App
- Diagnostics & Error Center
- OTA firmware support
- Privacy/microphone controls
- REST API
- WebSocket communication
- Modular firmware architecture
- ESP32-P4-ready architecture
- Future local-AI integration capability

## Architecture

```text
                         AURA SMART BUDDY
                                │
                    ┌───────────┴───────────┐
                    │                       │
              ESP32-WROOM-32          Companion App
                    │                       │
        ┌───────────┼───────────┐           │
        │           │           │           │
   Touchscreen    Audio       Sensors       │
        │           │           │           │
        └───────────┼───────────┘           │
                    │                       │
              AURA Core / API ◄─────────────┘
                    │
             Current AI Services
                    │
                    │ Future 
                    ▼
             Local AI Computer
                    │
          ┌─────────┼─────────┐
          │         │         │
         STT       LLM       TTS
                    │
                 Memory
````

The ESP32 acts as the main AURA device controller.

It manages the hardware, user interaction, device state, display, audio, LEDs, sensors, networking, REST API, WebSocket communication, Companion App communication, and diagnostics.

### Future Local AI

AURA does **not currently contain a local AI computer**.

The architecture is designed so that a local AI computer can be connected in the future.

Possible future AI hosts include:

* Raspberry Pi 5
* Raspberry Pi 5 with an AI accelerator
* NVIDIA Jetson-class computer
* Local GPU computer

The future local AI system could provide:

* Local speech-to-text
* Local LLM inference
* Local text-to-speech
* Local memory
* Local RAG/knowledge
* Fully offline AI operation

These are future/optional capabilities and are not currently built into AURA.

## Current Hardware

### Main Controller

**ESP32-WROOM-32**

* 4 MB flash
* No PSRAM
* Wi-Fi
* Bluetooth
* Main AURA controller

### Display

**5-inch IPS capacitive touchscreen**

* 800 × 480 resolution
* Primary AURA user interface
* Touch-enabled graphical interface

### Microphone

**INMP441 I2S MEMS microphone**

Current wiring:

| INMP441  | ESP32   |
| -------- | ------- |
| BCLK     | GPIO 26 |
| WS/LRCLK | GPIO 25 |
| SD       | GPIO 34 |
| L/R      | GND     |
| VDD      | 3.3V    |
| GND      | GND     |

The microphone remains inactive at boot and is activated through the AURA interaction system.

### Audio Output

**MAX98357A I2S amplifier**

Used to drive the external AURA speaker.

### LED Ring

**16× WS2812B RGB LEDs**

Normal AURA status indication uses the entire ring.

All LEDs use the same status color during normal operation.

Normal status lighting does not use:

* Sequential LED movement
* Chasing
* Comet animation
* Scanner animation
* Rotating pixels

A subtle synchronized brightness flicker may be used.

### LED Status Colors

| AURA State      | Color   |
| --------------- | ------- |
| IDLE            | Blue    |
| LISTENING       | Cyan    |
| RECORDING       | Green   |
| PROCESSING      | Yellow  |
| SPEAKING        | White   |
| SETUP           | Purple  |
| PRIVACY / MUTED | Magenta |
| ERROR           | Red     |
| OTA             | Orange  |

### Disco Mode

Disco Mode is a separate lighting mode controlled by the Companion App.

It is independent from normal AURA status indication.

### Touch Sensor

**TTP223 capacitive touch sensor**

#### Single Tap

```text
Touch
  ↓
Microphone / Listening
  ↓
AURA interaction
```

#### Double Tap

```text
Double Tap
  ↓
Cancel active interaction
  ↓
Microphone OFF
  ↓
IDLE
```

#### 5-Second Hold

```text
Hold for 5 seconds
  ↓
AURA SETUP
  ↓
AURA_Setup access point
```

Setup Mode is intended to remain active until an explicit setup-exit action occurs.

## Privacy Mode

Privacy Mode prevents microphone activation.

When Privacy Mode is active:

* Microphone activation is blocked
* Active conversations are cancelled
* AURA indicates the muted state
* Touch-to-listen activation is blocked

## Wi-Fi & Setup Mode

AURA supports normal Wi-Fi operation and a dedicated setup access point.

Setup access point:

```text
AURA_Setup
```

Setup Mode is entered by holding the touch sensor for approximately 5 seconds.

## Voice Architecture

AURA uses a modular voice architecture:

```text
Microphone
    ↓
Speech-to-Text
    ↓
AI Processing
    ↓
Text-to-Speech
    ↓
Speaker
```

Cloud voice services may be used by the current implementation.

Cloud-service failures must not prevent the core AURA hardware from operating.

If a voice provider becomes unavailable, AURA records the appropriate diagnostic event and continues operating in its available hardware/offline mode.

## Diagnostics & Error Center

AURA includes a structured diagnostic and error-reporting system.

Diagnostic events can contain:

* Unique ID
* Timestamp
* Uptime
* Severity
* Component
* Error code
* Title
* Message
* Active/resolved state
* Acknowledgement state
* Occurrence count
* Boot ID

### Severity Levels

| Severity | Meaning              |
| -------- | -------------------- |
| INFO     | Informational event  |
| WARNING  | Degraded condition   |
| ERROR    | Feature failure      |
| CRITICAL | Major system failure |

AURA maintains a bounded diagnostic history of up to **200 events**.

Repeated failures are deduplicated to prevent a single problem from filling the entire history.

Diagnostic history is stored locally.

### Companion App Error Center

The Companion App provides:

* Device health
* Severity filters
* Error history
* Error details
* Occurrence counts
* Active/resolved status
* Error acknowledgement
* Error history clearing
* Real-time error notifications

WebSocket diagnostic events use:

```text
aura_error
```

## REST API & WebSocket

AURA provides a local REST API and WebSocket interface.

The firmware uses a wildcard `/api/*` dispatcher.

The API provides access to functions including:

* Device status
* Microphone control
* LED control
* Diagnostics
* OTA
* Storage
* Device tools
* Voice state
* Companion App communication

## Companion App

The AURA Companion App provides the software interface for interacting with the device.

Current areas include:

* Dashboard
* Device control
* Chat
* Settings
* Local/remote connectivity
* Notifications
* OTA
* Tools
* Diagnostics/Error Center
* LED controls
* Disco Mode
* Device status

The application communicates with AURA through REST APIs and WebSocket communication.

## Storage

AURA supports microSD storage for device data and audio-related assets.

Internal filesystem storage is also used for system information such as diagnostic history.

## OTA

AURA supports firmware updates through its OTA architecture.

Production deployments should use appropriate firmware authentication and security controls.

Development configuration should not be used for production distribution.

## Offline-Core Architecture

A key AURA design principle is:

> Cloud service failure must never disable the physical device.

```text
                 Cloud unavailable
                        │
                        ▼
                  Diagnostic Event
                        │
                        ▼
              ┌─────────────────┐
              │    AURA CORE    │
              │                 │
              │ Touch     ✓     │
              │ Display   ✓     │
              │ LED       ✓     │
              │ Setup     ✓     │
              │ Hardware  ✓     │
              └─────────────────┘
```

Cloud AI availability is therefore treated as a service dependency rather than a requirement for booting the core device.

## ESP32-P4-Ready Architecture

AURA currently runs on the **ESP32-WROOM-32**.

The architecture is being prepared for a future **ESP32-P4** hardware revision.

### Current

```text
ESP32-WROOM-32
```

### Future

```text
ESP32-P4
```

AURA is being designed so the core software architecture can be migrated to the ESP32-P4 while minimizing changes to the high-level application logic.

Potential board-specific migration areas include:

* GPIO mappings
* I2C
* SPI
* I2S
* Audio drivers
* Display drivers
* Timers
* Watchdog
* Networking
* Flash configuration
* Partition layout
* Memory configuration
* Peripheral initialization

**ESP32-P4-ready does not mean ESP32-P4 firmware is currently implemented or physically tested.**

The current production target remains the ESP32-WROOM-32.

## ESP32-C6

ESP32-C6 is considered as a future/alternative AURA hardware platform.

Potential applications include:

* Connectivity-focused variants
* Lower-power designs
* Specialized companion hardware

The current ESP32-WROOM-32 firmware should not be assumed to be directly compatible with ESP32-C6.

**Status: Future / Experimental**

## Platform Status

| Component                  | Status                |
| -------------------------- | --------------------- |
| ESP32-WROOM-32             | Current               |
| 5-inch 800×480 touchscreen | Current               |
| INMP441 microphone         | Current               |
| MAX98357A audio            | Current               |
| 16× WS2812B LED ring       | Current               |
| TTP223 touch sensor        | Current               |
| Diagnostics/Error Center   | Current               |
| Companion App              | Current               |
| ESP32-P4                   | Future / P4-ready     |
| ESP32-C6                   | Future / Experimental |
| Local AI computer          | Future / Optional     |
| Raspberry Pi 5 AI backend  | Future / Optional     |

## Firmware Architecture

AURA is designed as a modular firmware system.

```text
AURA Core
├── System Manager
├── Conversation Manager
├── Display Manager
├── LED Manager
├── Audio Manager
├── Microphone
├── Speech-to-Text
├── Text-to-Speech
├── Wi-Fi Manager
├── Web Portal
├── Storage Manager
├── SD Manager
├── OTA Manager
├── Reminder Manager
├── Sensors
└── Error Manager
```

This modular architecture makes it possible to migrate individual hardware layers to future controller platforms without redesigning the entire AURA system.

## Development Status

AURA is an actively developed project.

The current ESP32 implementation includes verified core device functionality such as:

* Boot and initialization
* Touch interaction
* Microphone control
* Setup Mode
* Privacy behavior
* LED status system
* Diagnostics architecture
* Companion App communication

Some advanced features depend on additional hardware or external service configuration.

## Roadmap

### AURA V1

* [x] ESP32-WROOM-32 controller
* [x] 5-inch 800×480 touchscreen
* [x] Touch interaction
* [x] Microphone
* [x] LED status ring
* [x] Setup Mode
* [x] Privacy Mode
* [x] Companion App
* [x] Diagnostics/Error Center
* [x] REST API
* [x] WebSocket
* [x] OTA architecture

### Future AURA

* [ ] ESP32-P4 hardware revision
* [ ] Expanded hardware capabilities
* [ ] Improved local processing
* [ ] Additional peripherals

### Local AI Expansion

* [ ] Local AI host
* [ ] Local STT
* [ ] Local LLM
* [ ] Local TTS
* [ ] Local memory
* [ ] Fully offline AI mode

## Vision

The long-term goal is to evolve AURA into a private, modular, locally capable Smart Buddy.

```text
ESP32 Controller
       ↓
Reliable AURA Hardware
       ↓
ESP32-P4-ready Architecture
       ↓
Local AI Computer
       ↓
Local STT + LLM + TTS
       ↓
Local Memory
       ↓
Fully Local Smart Buddy
```

AURA's design philosophy is:

> **Build the hardware first. Keep the architecture modular. Make cloud services optional. Move intelligence local when the hardware is ready.**
