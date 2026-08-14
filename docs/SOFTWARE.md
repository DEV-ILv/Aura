# AURA OS — Software

This document gives an overview of the software stack for **AURA OS**:
the ESP32 firmware, the web portal, the Flutter companion app, and the AI
providers. It complements [ARCHITECTURE.md](ARCHITECTURE.md) (system
architecture) and the topic documents linked from [README.md](README.md).

---

## Components

| Component | Technology | Location | Role |
| --- | --- | --- | --- |
| Firmware | C++ (C++17), Arduino-ESP32 core `3.3.11`, FreeRTOS | `Aura_programs/` | All on-device logic |
| Web portal (SPA) | Vanilla ES6 (HTML/CSS/JS, no build step) | `Aura_programs/data/` | Dashboard + configuration UI served by the device |
| Companion app | Flutter (Dart), Android + Windows | `aura_companion/` (separate repo) | Phone/desktop companion |
| Cloud backend | Supabase (auth + Postgres, RLS) | `aura_companion/` | Remote sign-in + cloud device registry |

---

## Firmware

- **Build:** `arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" Aura_programs`
- **Partition scheme:** Huge APP (3 MB No OTA / 1 MB SPIFFS) — the default
  scheme does not fit.
- **Version:** 1.0.0 — Mark III "Phoenix" — Development channel
  (`version.h` is the single source of version strings).
- **Configuration:** `config.h` (pins, identity, AI endpoint URLs, timing
  constants), `secrets.h` (API keys / credentials — **git-ignored**, copy the
  committed `secrets.h.example` template), `firmware_keys.h` (ECDSA P-256
  public key for OTA verification).

### Main subsystems

The firmware is organized around `SystemManager` (single-loop orchestration of
~50 module singletons), `StorageManager` (single write authority), the `WebPortal`
(REST + WebSocket), `AudioManager` (I2S mic/speaker), the AI providers
(`GeminiClient`, Sarvam placeholders, offline `LocalAIEngine`), and the UI
stack (`DisplayManager`, `LedRing`). For the full responsibilities, public
API, boot order, data flow, and module wiring verdicts, see
[ARCHITECTURE.md](ARCHITECTURE.md).

### Current status of cloud AI

Per the repo `README.md` and [sarvam_ai.md](sarvam_ai.md):

- **Gemini** is the reasoning engine, but the API endpoint is not wired in the
  shipped build (cloud services fail closed unless endpoints are configured).
  The **offline Local AI Engine** is the active assistant.
- **Sarvam AI** is the *declared* STT/TTS provider but ships as a
  **placeholder** (`sarvam_stt` / `sarvam_tts` return "Not Implemented"), so
  the voice pipeline degrades gracefully until the Sarvam integration is
  completed and the API key is set.
- Voice wake-word is not wired; touch (push-to-talk) is the only input.

---

## Web portal

Served by the device on port 80 (`WebPortal`). A token-authenticated SPA
(`data/index.html` + `data/js/`, `data/css/`) provides:

- Dashboard, live WebSocket feed (port 81, `ws://<ip>:81`, token handshake).
- Configuration & CRUD for reminders, goals, habits, planner, study, memory,
  knowledge graph, documents, workspaces, decisions, settings, and more
  (REST, ~154 routes).
- OTA upload (signed firmware), restart, factory reset.

Auth is enforced with `X-Auth-Token` on all authenticated routes; the WebSocket
requires an auth handshake. See `SECURITY.md` for details.

---

## Companion app (`aura_companion/`)

Flutter app for **Android** and **Windows** with a dark glassmorphic UI
(Material 3). Key capabilities:

- **Local mode:** connects to the device on the LAN via REST + WebSocket
  (chat, live metrics, memories, reminders, OTA, SD/storage explorer,
  Device Control).
- **Remote (cloud) mode:** falls back to **Supabase** when the device is out
  of range — email/password sign-in, cloud device registry. RLS protects all
  data; only the publishable (anon) key is embedded in the app.
- **Device Control centre:** display, LED ring, speaker, microphone, and
  network controls backed by firmware endpoints
  (`/api/display/control`, `/api/led/control`, `/api/audio/control`,
  `/api/mic/control`, `/api/mic/level`, `/api/wifi/scan`, `/api/wifi/forget`).
  See [companion-v2-device-control.md](companion-v2-device-control.md).

Integration specs:
- [companion-output.md](companion-output.md) — assistant-response WebSocket
  message format.
- `aura_companion/docs/supabase.md` — Supabase schema, RLS, auth flow,
  local-vs-remote architecture.

Build: `flutter pub get` → `flutter analyze` → `flutter test` →
`flutter build apk --release` / `flutter build windows`. Cloud credentials are
injected with `--dart-define=SUPABASE_URL=... --dart-define=SUPABASE_ANON_KEY=...`
(see `aura_companion/.env.example`).

---

## AI providers

| Role | Active provider | Status |
| --- | --- | --- |
| Reasoning engine | **Gemini** (`gemini_client.*`) | Endpoint not wired in shipped build; offline engine used |
| Speech-to-Text (firmware) | **Sarvam AI** (`sarvam_stt.*`) | Placeholder — not implemented |
| Text-to-Speech (firmware) | **Sarvam AI** (`sarvam_tts.*`) | Placeholder — not implemented |
| Companion on-device STT | platform `speech_to_text` plugin | Works (device recogniser) |
| Companion on-device TTS | platform `flutter_tts` plugin | Works (device engine) |

STT/TTS are interchangeable behind `SpeechToTextProvider` / `TextToSpeechProvider`
factories (see [sarvam_ai.md](sarvam_ai.md)).
