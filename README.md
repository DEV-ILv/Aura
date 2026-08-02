# AURA AI Desktop Assistant — AURA OS

**Version:** 1.0.0 · Mark III "Phoenix" · Development channel
**Platform:** ESP32-WROOM-32 (38-pin) · Arduino-ESP32 core 3.3.11
**Firmware root:** `Aura_programs/`

AURA OS is a standalone, voice-first desktop AI assistant built for the ESP32. It combines cloud AI (Gemini, Google Speech-to-Text, Google Text-to-Speech) with an offline fallback assistant, a full web configuration portal, and a suite of personal-productivity features — reminders, goals, habits, planning, study tools, daily briefings, reflections, a knowledge graph, documents, workspaces, and proactive recommendations — all driven from a small OLED display, an I2S microphone/speaker pair, an LED ring, a touch sensor, and SD-card storage.

## Hardware

| Component | Interface | Pins (config.h) |
|---|---|---|
| OLED SSD1306 128x64 | I2C @ 0x3C | SDA 21 · SCL 22 |
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
- `secrets.h` — API keys, AP credentials, web credentials, signing keys (never commit real values).
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
