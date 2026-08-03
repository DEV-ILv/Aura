# AURA AI Desktop Assistant — AURA OS

**Version:** 1.0.0 · Mark III "Phoenix" · Development channel
**Platform:** ESP32-WROOM-32 (38-pin) · Arduino-ESP32 core 3.3.11
**Firmware root:** `Aura_programs/`

AURA OS is a standalone, voice-first desktop AI assistant built for the ESP32. It combines cloud AI (Gemini, Google Speech-to-Text, Google Text-to-Speech) with an offline fallback assistant, a full web configuration portal, and a suite of personal-productivity features — reminders, goals, habits, planning, study tools, daily briefings, reflections, a knowledge graph, documents, workspaces, and proactive recommendations — all driven from a small OLED display, an I2S microphone/speaker pair, an LED ring, a touch sensor, and SD-card storage.

## Documentation

| Document | Purpose |
| --- | --- |
| [SECURITY.md](SECURITY.md) | Security policy, reporting, and hardening overview |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Setup, coding standards, and contribution workflow |
| [CHANGELOG.md](CHANGELOG.md) | Release history and the Security Hardening V1 notes |
| [docs/ota-signing.md](docs/ota-signing.md) | OTA key generation, signing, rotation, CI |
| [`aura_companion/.env.example`](../aura_companion/.env.example) | Companion cloud-credentials template (placeholders only) |

## Hardware

| Component | Interface | Pins (config.h) |
|---|---|---|
5.0" IPS Capacitive Touchscreen | 800×480 | 16.7M Colors | Multi-touch | LVGL-Based User Interface
| Microphone INMP441 | I2S (mono 16-bit) | BCLK 26 · WS 25 · DATA 34 |
| Speaker MAX98357A | I2S | BCLK 27 · LRC 14 · DATA 12 |
| LED ring | GPIO (WS2812-style) | GPIO 4 |
| Touch sensor | Capacitive touch | GPIO 13 |
| SD card | SPI | CS 5 · MOSI 23 · MISO 19 · SCK 18 |

## Features

- **Voice assistant:** wake via touch, INMP441 capture → Google STT → Gemini → Google TTS → MAX98357 speaker.
- **Offline assistant (Local AI Engine V2):** `LocalAIEngine` composes context-aware, personality-styled replies across all 28 intents via a multi-engine pipeline (Conversation Context → Memory/Knowledge retrieval → Planner → Goals → Recommendation Engine → Personality → Sentence Generation), with time-of-day awareness, follow-up questions, a response cache, and guaranteed response variation. `OfflineResponseGenerator` delegates to it, preserving the Intent Classifier → Generator → managers architecture when the cloud is unreachable.
- **Web portal:** dashboard + REST/HTML configuration and CRUD for nearly every feature (token-authenticated).
- **Signed OTA updates:** ECDSA P-256 + streaming SHA-256 verification before install.
- **Productivity suite:** reminders, goals, habits, planner (Eisenhower), study sessions/flashcards, daily briefings, once-daily reflection, knowledge graph with auto-linking, documents, multi-member workspaces, semantic/unified search, decisions, learning insights, predictions, analytics, and proactive recommendations.
- **UI:** SSD1306 OLED with 11 screen types, widget/animation engine, themes, screen-timeout sleep; LED ring feedback.
- **Reliability:** 30 s watchdog, NVS boot-loop counter → Safe Mode, crash logging + web review, factory reset, low-power mode.
- **Extensibility:** configuration-based plugin marketplace, skills, automation rules, workflows, scenes, and a command palette.
- **Networking:** Wi-Fi STA/AP, mDNS (`aura-<mac>`), NTP sync, ESP-NOW device mesh / companion.

## Configuration

- `config.h` — pins, identity, AI endpoint URLs, timing constants.
- `version.h` — single-source versioning (1.0.0, Mark III "Phoenix").
- `secrets.h` — API keys, AP credentials, web credentials, signing keys (never commit real values). **This file is git-ignored** — copy the committed template before building:
  ```sh
  copy secrets.h.example secrets.h   # Windows
  cp secrets.h.example secrets.h     # macOS / Linux
  ```
- `firmware_keys.h` — ECDSA P-256 public key for OTA verification.
- Runtime settings via `SettingsManager`; Wi-Fi credentials in NVS (`aura_wifi`).

## Building

```sh
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" Aura_programs
```

> **Important:** the default partition scheme does not fit ("text section exceeds available space"). Use **Huge APP (3MB No OTA/1MB SPIFFS)** — in Arduino IDE: Tools → Partition Scheme → Huge APP.

**Build metrics:** 1,938,167 / 3,145,728 bytes flash (61%) · 79,000 / 327,680 bytes RAM (24%) · 0 warnings.

## Offline AI engine (Phase 3)

The offline assistant runs a rule-based "micro language engine" (`LocalAIEngine`)
as a drop-in upgrade over `OfflineResponseGenerator`. Pipeline:

```
Speech/Text → IntentClassifier → ConversationContextEngine → Memory retrieval
→ Knowledge-graph retrieval → Planner → GoalManager → RecommendationEngine
→ PersonalityEngine → SentenceGenerationEngine → Offline response
```

New components (`Aura_programs/`):

| Component | Role |
|---|---|
| `local_ai_engine.h/.cpp` | Coordinator; composes all 28 intents, memory/knowledge/recommendation injection, follow-ups, self-test, status JSON |
| `conversation_context_engine.h/.cpp` | Rolling topic/history + user state (project, task, mood, workspace, goal, reminder, study) + time-of-day awareness |
| `sentence_generation_engine.h/.cpp` | Flash-resident fragment pools (CASUAL/NEUTRAL/FORMAL) with repeat avoidance and composition helpers |
| `personality_engine.h/.cpp` | Maps `PersonalityManager` profiles → generation knobs (register, humour, verbosity, confidence) |
| `recommendation_engine.h/.cpp` | Rule-based advisor: planner → habit → goal <40% → executive → prediction → learning |
| `local_ai_cache.h/.cpp` | 8-slot FIFO response cache, last-response tracker, phrase frequency |

Tunables live in `config.h` under `LOCAL_AI_*`. A 12-case self-test runs at boot
(`LOCAL_AI_SELF_TEST_ON_BOOT`), and `/api/offline-ai/test` remains available from
the web portal. See `CHANGELOG.md` for the full Phase 3 entry.

## Headless development mode (Phase 4)

AURA OS can run on a **bare ESP32-WROOM-32 with only a USB cable** — no OLED,
mic, speaker, LED ring, touch sensor, SD card, or sensors attached. When a
peripheral is missing the firmware logs a warning, disables **only** that module,
and continues booting. It never aborts for a missing peripheral, so the full
headless feature set stays available:

- Wi-Fi (STA with saved credentials, or AP setup portal), mDNS
- Web portal + REST API (`http://<device>/api`)
- WebSocket live feed (dashboard + `module_status` broadcasts)
- Local AI engine, Gemini, Memory, Planner, Goals, Habits, Knowledge Graph,
  Reminders, Workspaces, OTA, Settings, Companion App

### How it works

- `HEADLESS_MODE_AUTO` (default `true`) — at boot the firmware probes the OLED.
  If no display is detected it enables headless automatically and prints
  `AURA Headless Mode Enabled`.
- `HEADLESS_MODE_FORCE` (default `false`) — set to `true` to force headless even
  when all hardware is present (handy for bench testing).
- Optional modules disabled in headless mode: **Display, LED Ring, Microphone,
  Speaker, Touch, sensors**. SD card reflects its live mount state.

### Runtime status

Every service reports a runtime status (`ONLINE` / `OFFLINE` / `DISABLED` /
`ERROR`) through the `ServiceStatusManager`. `GET /api/status` now returns, in
addition to the legacy fields, the firmware version, `headless` flag + `mode`,
a per-module status map, `connected_modules` / `disabled_modules` lists,
`memory_usage`, `cpu_usage`, Wi-Fi details, and flash usage. Status changes are
pushed to WebSocket clients as `{"type":"module_status","modules":{…}}`.

### Serial boot banner

After boot, the serial console prints:

```
==============================================
        AURA AI Desktop Assistant
----------------------------------------------
Firmware : <version>
Board    : ESP32-WROOM-32
Headless : ENABLED (auto)
Enabled  : ["wifi","web_portal",...]
Disabled : ["display","microphone",...]
Wi-Fi    : CONNECTED (<ssid>) | AP Mode (<ssid>)
REST     : http://<ip>/api
WebSocket: ws://<ip>:81
Gemini   : Online | Offline
----------------------------------------------
READY
==============================================
```

### Companion App

The Companion App shows a headless banner, marks disabled modules grey in the
Module Status sections (Dashboard + System Monitor), and omits module-dependent
controls when the corresponding module is not available. Headless state updates
live over WebSocket.

## Architecture in brief

A single main-loop orchestrator (`SystemManager`) sequentially initializes and polls ~50 subsystem singletons (`initialize()/update()/save()`), with an EventBus for decoupled notifications, a state machine (BOOTING → INITIALIZING → READY ↔ BUSY/LOW_POWER/UPDATING → ERROR/SHUTDOWN), and a reverse-order rollback on init failure. `StorageManager` is the single persistence authority (SPIFFS + SD, atomic `.tmp`+`rename` writes). A service framework (`Service`/`ServiceManager`) and an in-progress AI pipeline layer are registered at boot.

See **`AURA_ARCHITECTURE.md`** for the complete 16-section architecture analysis (purpose, responsibilities, public API, workflows, dependencies, data flow, boot sequence, runtime behavior, features, hardware interaction, configuration, error recovery, architecture, feature map, code statistics, summary).

## Current development status

The firmware compiles cleanly and boots deterministically. Several subsystems are **initialized but not fully driven** and are flagged as such in the architecture report. Notable items:

- **Cloud AI endpoints** (STT/Gemini/TTS) are wired for `setApiKey()` from the web portal, but `setApiEndpoint()` is never called — cloud services fail closed unless endpoints are configured by default.
- **Service framework** is registered but services are never `StartService`'d, so their `Update()` loops are not driven.
- **Unreachable features:** `SmartSearch`, `AnalyticsManager` recording, `TimelineManager` writes, `ResilienceManager` auto-recovery, reminder creation, and the `AiPipeline` start methods have no effective callers.
- **On-demand only:** briefings are generated on demand / after daily reflection; there is no automatic morning/evening schedule.

## Repository layout

```
Aura_programs/              Firmware source (90 headers · 81 sources · 1 sketch · ~44K lines)
AURA_ARCHITECTURE.md        Complete architecture analysis (16 sections)
CHANGELOG.md                Release history
```

## License / attribution

Author: Devil · Hardware revision MK-II · Project name: AURA AI Desktop Assistant
