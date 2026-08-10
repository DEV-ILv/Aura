# AURA OS Changelog

All notable changes to AURA OS are documented here.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
AURA OS is currently at version `1.0.0` (Mark III "Phoenix"), Development channel.
Current build metrics: 62% flash (1,979,307 / 3,145,728 B) · 33% RAM (108,384 / 327,680 B) · 0 warnings in AURA code.

## [Unreleased] — Final LED Status System (solid-colour per state)

Implements the final LED spec: every AURA state paints all 16 LEDs in a single
distinct **solid colour** — no per-LED travel, chase, comet, tail, pulse,
breathing or rotation. The only allowed variation is a subtle whole-ring
synchronised brightness flicker.

### Changed

- **Normal status path rewritten to SOLID colours** (`led_ring.cpp`). All
  `play*()` renderers now call a single `renderSolidStatus(color, lo%, hi%,
  stepMax, minMs, maxMs)` helper; `update()` applies a short cross-fade between
  solid states. Exact spec palette: IDLE/Boot blue `#0080FF`, Listening cyan
  `#00FFFF`, Recording green `#00FF40`, Thinking/Processing yellow `#FFFF00`,
  Speaking white `#FFFFFF`, Setup purple `#8000FF`, Privacy/Muted magenta
  `#FF00AA`, Error red `#FF0000`, OTA orange `#FF8000`, plus distinct
  non-status solids (honour gold, success mint, reminder amber, warning
  red-orange, critical dark red, offline slate, sleep dim navy, wake azure,
  Wi-Fi steel blue / spring green).
- **Synchronised brightness flicker** — the entire ring shares ONE random-walk
  brightness level (normal ~86–100 %, quiet ~92–100 %) recomputed every
  50–150 ms; `ERROR`/`WARNING`/`CRITICAL` use a deeper, quicker flicker
  (~55–100 %, 35–90 ms). Reset on every `setMood()`.
- **Dead code removed** — old traversal constants (`kBootFillMs … kWifiStepMs`)
  and per-state chasing renderers deleted.
- **Gone** — travelling-light status animation, boot fill sweep, idle sparkle,
  heartbeat/pulse variants, and sequential colour ramps in the status path.
- **Unchanged** — Disco Mode (`renderDisco`, app-only, 10 rotating animations,
  emergency pause) remains fully separate from the status system.

### Docs

- Docset updated to the solid-colour system: `AURA_Instruction_Manual.md`
  (§8, status table, troubleshooting, FAQ), `AURA_QUICK_START.md`,
  `docs/aura-led-state-machine.md`, and `aura_mood.h` header comments.

## [Unreleased] — Touch, Microphone & Setup Interact Refinement

Makes the physical touch sensor behaviour deterministic: exactly three gestures and
no accidental activation, per the final interaction spec.

### Changed

- **Gesture model — exactly three gestures.** `ConversationManager::processTouch()`
  now recognises only: SINGLE TAP (mic on + listening), DOUBLE TAP (mic off +
  cancel voice interaction + IDLE) and 5s HOLD (AURA SETUP). The previous
  long-press privacy toggle and medium-hold dashboard-on-demand gestures were
  removed so a 4-second hold intentionally does nothing (it can never fire a
  tap, double tap, mute, or setup).
- **Setup hold fires during the press** — the 5s hold triggers at exactly
  `SETUP_HOLD_MS` while the finger is still down; `m_setupHoldTriggered` guards
  the release so it never also dispatches a single/double tap.
- **Gesture priority** — hold > double tap > single tap. A single tap is confirmed
  only after the double-tap window expires (never a single→double or
  double→single).
- **Setup mode hardening** — `enterSetupMode()` now force-stops any active voice
  interaction (cancel STT/AI/TTS, stop recording, disable VAD), pins the mic
  icon to MUTED, and shows `AURA SETUP` + the provisioning AP SSID on the OLED.
- **LED priority** — `LedRing::isEmergencyMood()` now includes `SETUP` and
  `PRIVACY`, so Disco Mode can never overwrite the purple setup ring or the
  privacy red. Full order: ERROR > OTA > SETUP > PRIVACY > DISCO > voice > idle.
- **Timing constants** — debounce/double-tap/setup times centralised in `config.h`
  (`TOUCH_DEBOUNCE_MS`, `TAP_MIN_MS`, `TAP_MAX_MS`, `DOUBLE_TAP_WINDOW_MS`,
  `SETUP_HOLD_MS`, `TOUCH_POLL_INTERVAL_MS`), all `millis()`-based and
  non-blocking (no `delay()`, no Wi-Fi/STT/TTS waits).

### Added

- `mic_active` field in the WebSocket dashboard payload (alias of recording).
- `POST /api/mic/control` accepts `"privacy": bool` — the Companion App / REST
  retains a way to enable and disable microphone privacy now that the touch
  long-press gesture has been retired.
- DEBUG-only gesture event logging (`TOUCH_DOWN`, `TOUCH_UP`, `SINGLE_TAP pending`,
  `DOUBLE_TAP`, `SETUP_HOLD 5000ms reached`, `HOLD ignored`, `MIC_ON`); suppressed
  in release builds by `CURRENT_LOG_LEVEL`.

### Fixed

- Privacy-mode hint on the OLED no longer tells users to "tap and hold to unmute"
  (that gesture no longer exists) — it now reads "Unmute via companion app".

### Notes

- Privacy mode remains fully functional; it is just no longer bound to the touch
  sensor (REST/Companion path).
- Hardware touch testing is pending physical device validation (see final report).

## [Unreleased] — Security Hardening V1

Authentication, transport security, and safe-by-default behavior for the public
GitHub release. Addresses the Critical (C1–C5) and High findings of the AURA V1
security review.

### Added

- **Documentation: verified INMP441 microphone wiring** — `docs/hardware-wiring.md`
  records the validated connections (VDD→3V3, GND, SCK→GPIO26, WS→GPIO25,
  SD→GPIO34, L/R→GND for the left channel) and README documents the same;
  pins match `config.h` (`MIC_BCLK_PIN=26`, `MIC_WS_PIN=25`, `MIC_DATA_PIN=34`).
- **Development / production build modes** — new `AURA_DEVELOPMENT_MODE`
  compile-time flag (defaults to `0`/production). Production keeps the random
  first-boot admin password + MAC-derived AP password. Development mode
  (`1`, local testing only) restores the well-known development credentials:
  Web Portal / REST `Devil` / `Devil`, setup AP `AURA_Setup` / `DevilDevil`.
  The flag is enabled via the git-ignored `secrets.h`; the companion prefill is
  behind `--dart-define=AURA_DEVELOPMENT_MODE=true`.

- **First-boot admin credentials (C4)** — no more hardcoded `Devil`/`DevilDevil`
  defaults in `secrets.h`. On first boot the device generates a strong random
  32-hex-char admin password (`esp_random()`), stores it in NVS, and prints it to
  the Serial monitor. The web portal flags the credential with `must_change` and
  forces a password change on first login (`/api/auth/change-password`,
  minimum 8 characters). Existing user credentials are preserved across NVS
  layout migrations (`kAuthCredVersion = 3`); they are never reset to defaults.
- **Per-IP login rate limiting** — replaces the global brute-force counter with a
  per-source-address tracker (5 attempts → 30 s lockout, bounded map of 32 IPs),
  so one attacker cannot lock out every client.
- **`/api/auth/status` and `/api/auth/change-password`** endpoints plus a web-portal
  sign-in flow: `POST /api/auth/login` returns `{ token, expiresIn, must_change }`;
  all API, OTA, restart, factory-reset, and Wi-Fi routes now require the
  `X-Auth-Token` header (401 otherwise).
- **Firmware OTA signature verification (C3)** — `OtaManager::verifyFirmwareSignature`
  is now fail-closed (rejects when no key is embedded) and the web-portal OTA upload
  verifies an optional `X-Signature`/`signature` ECDSA P-256 DER signature against the
  embedded public key before applying (SHA-256 of the uploaded image). Missing
  signature is logged as a warning. Tooling: `tools/generate_keypair.ps1` and
  `tools/firmware_signer.ps1` (Windows PowerShell/.NET) alongside the existing
  Python tools.
- **ESP-NOW hardening** — encrypted links (`encrypt = true`, LMK derived from a
  strong shared PMK instead of the guessable `"AURA_ESPNOW_KEY"`) and privileged
  message types (text/commands/OTA) are only processed from PAIRED nodes.
- **Async Wi-Fi recovery** — `RecoveryWiFi()` no longer blocks the main loop for
  up to 10 s; recovery runs as a state machine finalized by `Update()`.
- **`JSONBuilder::finalize()` off-by-one fix** — guarantees room for the closing
  brace and terminator, preventing a write past the end of the buffer.
- **`/api/status` is now authenticated** — status telemetry is no longer public.
- **WebSocket authentication (C1)** — port 81 now requires a token handshake:
  after connect the server sends `{"type":"auth_required"}` and the client must
  reply `{"type":"auth","token":...}` (constant-time comparison + expiry) before
  any telemetry is sent; invalid tokens are disconnected immediately. Cross-origin
  browser handshakes are rejected via Origin validation. Telemetry broadcasts are
  routed only to authenticated clients.
- **Web SPA login** — `data/js/auth.js` + `data/css/auth.css`: full-screen sign-in
  overlay, change-password modal, nav sign-out button, `X-Auth-Token` header on all
  fetches and the OTA XHR, WebSocket auth handshake, and 401-driven session expiry.
  Token persists per-tab in `sessionStorage` (never `localStorage`).

### Changed

- `secrets.h` / `secrets.h.example` — no default passwords (`WEB_PASSWORD ""`,
  `AP_PASSWORD ""`); `WEB_USERNAME "admin"` remains the default username.
- `security_manager.cpp` — boots unauthenticated (no implicit trust), `Authenticate`
  validates a real token, `CheckToken` performs constant-time comparison, and
  `Encrypt`/`Decrypt` delegate to `VaultManager`.
- `wifi_manager.cpp` — `connect()` persists credentials to NVS so Wi-Fi settings
  survive reboots (C2); `/api/wifi` POST saves and connects.
- `ota_manager.cpp` — WDT feed after blocking HTTP calls and inside the download
  chunk loop.
- `web_portal.cpp` — factory reset now clears the `auraauth` NVS namespace so the
  next boot regenerates fresh admin credentials.
- Web SPA (`api.js`, `websocket.js`, `app.js`, `settings.js`, `ota.js`) — all
  requests carry the session token; the UI blocks behind the sign-in overlay.

### Security notes

- ESP-NOW PMK ships in firmware by design (single-user device mesh). A future
  release should replace it with a per-install key exchanged during an
  authenticated pairing handshake.
- Web OTA accepts unsigned images with a warning only; production deployments
  should always sign firmware and send `X-Signature`.
- `Secrets::WEB_PASSWORD`/`AP_PASSWORD` are now empty; any existing firmware
  flashing must occur over USB with the new first-boot flow.

## [Unreleased] — Phase 3: Local AI Engine V2

The offline assistant has been upgraded from a static template generator into a
multi-engine "micro language engine". The existing architecture is preserved:
`IntentClassifier` → `OfflineResponseGenerator` → managers. `OfflineResponseGenerator`
now delegates to the new `LocalAIEngine` coordinator, so **no public API changed**
(Gemini, REST/WebSocket, Companion, voice, and web-portal surfaces are untouched).

### Added — new engines (drop-in upgrade of the offline pipeline)

- **`local_ai_engine.h/.cpp`** — `LocalAIEngine` coordinator implementing the pipeline
  `Intent → Context Engine → Memory Retrieval → Knowledge Retrieval → Planner →
  Goals → Recommendation Engine → Personality → Sentence Generation → Response`.
  - `generate(intent)` and `generate(intent, rawText)` entry points; raw text enables
    exact-question cache hits and topic tracking.
  - Per-intent data composition for all 28 `IntentType` values, reading live state
    from the existing managers (never replacing them).
  - Enrichment: memory injection (`memoryManager.semanticSearch`), knowledge-graph
    injection (`searchNodes`), recommendation injection (`RecommendationEngine`),
    per-intent follow-up questions, time-of-day prefix/clauses, and guaranteed
    response variation (never repeats the last emitted wording verbatim).
  - `runSelfTest()` — 12-case suite (greetings, reminders, goals, habits, planner,
    memory, knowledge, study, workspace, unknown, …) run once per day at boot when
    `LOCAL_AI_SELF_TEST_ON_BOOT` is enabled.
  - `getStatusJSON()` for the web portal / REST surface.
- **`conversation_context_engine.h/.cpp`** — rolling conversational state: current /
  previous topic, intent, response, last user text, 6-turn history, plus ambient
  user state (project, task, activity, mood, workspace, active goal, upcoming
  reminder, study session, recent recommendation). Time awareness via
  `TimePeriod` buckets, weekend detection, time-based greetings and clauses.
  (Struct renamed `ContextTurn` to avoid a collision with `GeminiClient`.)
- **`sentence_generation_engine.h/.cpp`** — micro language engine assembling
  responses from flash-resident fragment pools (greetings, closings, transitions,
  verbs, adjectives, endings, connectors, confidence phrases, number words,
  synonyms) across three registers (CASUAL / NEUTRAL / FORMAL), with immediate-repeat
  avoidance. Composition helpers: `countPhrase`, `listItems`, `numberWord`,
  `capitalise`, `join`, `acknowledgement`.
- **`personality_engine.h/.cpp`** — expands `PersonalityManager` profiles into
  generation knobs (vocabulary register, humour, verbosity, confidence, follow-up
  offers). Maps jarvis/professional/teacher/programmer/friendly/minimal to knobs;
  delegates all fragment selection to the SentenceGenerationEngine.
- **`recommendation_engine.h/.cpp`** — rule-based advisor evaluated before each
  reply. Priority order: highest-priority due planner task → due habit (streak
  hook) → active goal under 40% → executive recommendation → high-probability
  prediction (≥ 0.7) → learned-pattern suggestion.
- **`local_ai_cache.h/.cpp`** — 8-slot FIFO response cache with exact-match lookups
  (hit counter now tracked), last-response memory for variation guarantees, and
  phrase-frequency checks. Configurable via `LOCAL_AI_CACHE_SIZE`.

### Changed

- **`offline_response_generator.cpp`** — `generate(intent)` now delegates to
  `localAIEngine.generate(intent)`. All legacy handler methods remain intact as a
  compatibility fallback.
- **`tiny_ai_manager.cpp`** — `process()` routes through
  `localAIEngine.generate(intent, userText)` (raw-text pass-through); boot
  self-test hook when `LOCAL_AI_SELF_TEST_ON_BOOT` is enabled. `m_generator` member
  retained for compatibility.
- **`config.h`** — added `LOCAL_AI_*` constants: `LOCAL_AI_HISTORY_TURNS` (6),
  `LOCAL_AI_CACHE_SIZE` (8), `LOCAL_AI_RETRIEVAL_MEMORIES` (3),
  `LOCAL_AI_RETRIEVAL_GRAPH` (2), `LOCAL_AI_MAX_FOLLOWUP_LEN` (96),
  `LOCAL_AI_MAX_DATA_ITEMS` (5), `LOCAL_AI_SELF_TEST_ON_BOOT` (true),
  `LOCAL_AI_VARIATION_POOL` (4).

### Fixed

- `LocalAICache::lookup()` now increments the hit counter it exposes via
  `hitCount()`.
- Compile fixes during Phase 3 bring-in: `ContextTurn` rename (collision with
  `GeminiClient::ConversationTurn`), `const char*` concatenation corrections,
  follow-up pool pointer typing.

### Notes

- All new pools and fragment data are flash-resident (`PROGMEM`-style `const`)
  arrays, keeping RAM impact minimal (+880 B over the Phase-2 baseline).
- No generic-chatbot conversion; the engine is a strict superset of the previous
  offline behaviour and still rule-based.

---

## [Unreleased] — Phase 4: Headless Development Mode

Firmware now boots and runs on a bare ESP32-WROOM-32 with **no external
hardware** (no OLED, mic, speaker, LED ring, touch, SD or sensors). Boot never
aborts for a missing peripheral: each optional module is detected, logged, and
disabled individually, while every headless-capable feature (Wi-Fi, AP/STA, web
portal, REST, WebSocket, auth, Local AI, Gemini, Memory, Planner, Goals, Habits,
Knowledge Graph, Reminders, Workspaces, OTA, Settings, Companion App) remains
fully active.

### Added

- **`service_status_manager.h/.cpp`** — central runtime-status registry for every
  firmware service (`ONLINE` / `OFFLINE` / `DISABLED` / `ERROR`), headless-mode
  tracking (`normal` / `auto` / `forced`), JSON payloads for REST + WebSocket,
  and change tracking for live module-status broadcasts.
- **Headless Mode config** (`config.h`): `HEADLESS_MODE_AUTO` (default `true`,
  enables headless when the OLED is not detected at boot) and
  `HEADLESS_MODE_FORCE` (default `false`, forces headless regardless of hardware).
- **`GET /api/status` full payload** — firmware version, `headless` flag, `mode`,
  per-module status map, `connected_modules` / `disabled_modules` lists,
  `memory_usage`, `cpu_usage`, `wifi` (connected/ssid/ip/rssi), `flash_used`,
  `flash_total`, plus all legacy keys (`running`, `uptime`, `heap_free`,
  `wifi_connected`, `requests`) for backward compatibility.
- **WebSocket module-status broadcasts** — `{"type":"module_status","modules":{…}}`
  emitted on status changes and on client connect (full snapshot), so the
  Companion App reflects headless detection and hot-plug events live.
- **Serial boot banner** — post-init banner showing AURA Firmware / Version /
  Board / Headless Mode / Enabled Modules / Disabled Modules / Wi-Fi / REST /
  WebSocket / Gemini / Ready, plus an `AURA Headless Mode Enabled` log on
  auto-detection.
- **Companion App headless UI** — headless banner on the shell + dashboard,
  `SystemStatus` model extended with `headless`, `mode`, `modules`,
  `connectedModules` / `unavailableModules`, and module-status badge grids on the
  Dashboard and System Monitor screens (disabled modules render grey).

### Changed

- **`system_manager.cpp`** — `initializeModules()` no longer aborts on failed
  optional hardware (Display, Audio, Sound, LED Ring); headless auto-detection is
  applied at the display probe; statuses are synced to `serviceStatusManager`
  during boot and refreshed every health-check cycle; `printBootBanner()` prints
  the serial boot banner.
- **`web_portal.cpp`** — `handleApiStatus()` delegates to
  `serviceStatusManager.getStatusJson()`; `webSocketBroadcastModuleStatus()` added
  to the `update()` loop and the WS `CONNECTED` handler.
- **`system_manager.h`** — added `isHeadless()`, `getHeadlessMode()`,
  `syncServiceStatuses()`, `refreshDynamicServiceStatus()`, `printBootBanner()`.
- **`web_portal.h`** — added `webSocketBroadcastModuleStatus()`.
- **`Aura_programs.ino`** — unchanged (banner is printed by `SystemManager`).

### Fixed

- `ServiceStatusManager` enum names are prefixed (`SVC_*`, `SS_*`, `HM_*`) to
  avoid collisions with ESP32 core macros (`DISPLAY`, `ERROR`, …).
- `wifiManager.isInitialized()` does not exist; Wi-Fi availability is now derived
  from `WifiManager::getState()`.

### Notes

- Optional peripherals disabled in headless mode: Display, LED Ring, Microphone,
  Speaker, Touch, sensors; SD card is reported per live mount state.
- RAM impact of the new registry: ~110 B; no changes to the Phase 3 flash
  baseline (61% flash, 24% RAM, 0 warnings).

---

## [Unreleased] — Startup Reliability & Authentication Standardization

### Fixed

- **Startup crash on bare ESP32 (boot loop / abort).** Global-manager
  constructors eagerly `reserve()`d large containers before the heap was ready
  (`_GLOBAL__sub_I_*` → `bad_alloc` in a `noexcept` ctor → `__terminate` →
  `abort`, PC `0x40205ca3`). Moving the reserves into `initialize()` still blew
  the ~230 KB heap (cumulative preallocation; `KnowledgeGraphManager::initialize()`
  failed at `system_manager.cpp:860`). **All eager `reserve()` calls were removed
  — in constructors and initializers — across 43 managers; containers now grow on
  demand.** Verified at the binary level: all 96 `_GLOBAL__sub_I_*` constructors
  disassemble to zero `operator new` / `__cxa_throw` calls.
- **Watchdog lifecycle.** `esp_task_wdt_init()` returned `ESP_ERR_INVALID_STATE`
  (259, "TWDT already initialized"); the old handler treated it as a failure and
  never registered the loop task, producing 5,968× `task_wdt_reset(): task not
  found` in 40 s. `initializeWatchdog()` now adopts the existing TWDT
  (`esp_task_wdt_add(nullptr)`), and every reset site is guarded by
  `esp_task_wdt_status(nullptr) == ESP_OK`. Verified on hardware: 0 abort / 0
  panic / 0 WDT spam, ≥341 s continuous uptime.

### Changed

- **Authentication standardized** across firmware and Companion App:
  - Web Portal / REST credentials default to **`Devil` / `Devil`**
    (`Secrets::WEB_USERNAME` / `WEB_PASSWORD`).
  - Setup hotspot is **`AURA_Setup` / `DevilDevil`** (WPA2 requires ≥8 chars;
    the previous 5-char `Devil` would make `WiFi.softAP()` fail per core
    `AP.cpp:224`).
  - **`web_portal.cpp`** — NVS credential migration: a version marker
    (`kAuthCredVersion`, key `version` in the `auraauth` namespace) resets any
    stale stored credentials to the standardized defaults once on first boot
    after upgrade, then leaves user-changed credentials untouched.
    `saveAuthCredentials()` stamps the current version.
  - **Companion App** `connection_screen.dart` — login fields are prefilled with
    `Devil` / `Devil` on first run (returning users keep their remembered
    username with a blank password).

---

## [1.0.0] — Mark III "Phoenix" (baseline)

Initial published development firmware. Feature baseline, hardware config, and
architecture described in `AURA_ARCHITECTURE.md` and `README.md`.

### Features (baseline)

- Voice assistant (touch-wake, INMP441 → STT → Gemini → TTS → MAX98357).
- Offline assistant: `OfflineResponseGenerator` with 26 handled intents.
- Web portal with token-authenticated REST/HTML config and CRUD.
- Signed OTA updates (ECDSA P-256 + streaming SHA-256).
- Productivity suite: reminders, goals, habits, planner (Eisenhower), study,
  briefings, reflections, knowledge graph, documents, workspaces, semantic search,
  decisions, learning insights, predictions, analytics, recommendations.
- UI: SSD1306 OLED, widget/animation engine, LED ring feedback.
- Reliability: watchdog, Safe Mode, crash logging, factory reset, low-power mode.
- Networking: STA/AP, mDNS, NTP, ESP-NOW mesh/companion.
