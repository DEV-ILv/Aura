# AURA OS â€” System Architecture

> **Status:** This analysis was produced during the complete system audit
> (2026-08-09) and reflects the codebase at that time; it is the canonical
> architecture reference. Cloud AI provider references have been updated to
> the current state (Sarvam AI placeholders â€” see
> [sarvam_ai.md](sarvam_ai.md)); no other content was changed.

**Project:** AURA AI Desktop Assistant (AURA OS)
**Version:** 1.0.0 â€” Mark III "Phoenix" â€” Development channel
**Target:** ESP32-WROOM-32 (38-pin) Â· Arduino-ESP32 core 3.3.11

---

## 1. Purpose

AURA OS is a standalone desktop AI assistant firmware for the ESP32. It
provides a voice-first conversational assistant with an OLED display, an I2S
microphone/speaker pair, an LED ring, a touch sensor, and SD-card storage. It
is designed to operate as a connected device (Wi-Fi, web configuration portal,
cloud AI services) with an offline fallback assistant and a large set of
"assistant" features: reminders, goals, habits, planning, study tools,
briefings, reflections, knowledge graph, documents, workspaces, and proactive
recommendations.

The system is organized around a central orchestrator (`SystemManager`) that
sequentially initializes and polls ~50 subsystems in a single main-loop task.
A secondary service framework (`Service`/`ServiceManager`), an event bus, and
a web portal provide the structured integration layer.

## 2. Responsibilities

| Area | Responsible modules |
| --- | --- |
| Orchestration / lifecycle / health | `SystemManager`, `ServiceManager`, `Service`, `HealthMonitor` |
| Persistence authority | `StorageManager` (SPIFFS + SD), `SettingsManager`, `MemoryManager`, `VaultManager` |
| Connectivity | `WifiManager`, `WebPortal`, `OtaManager`, `EspNowManager`, `DeviceMesh`, `CompanionManager`, `PlatformAbstraction` |
| Voice capture | `AudioManager` (I2S), `SarvamSpeechToText` (placeholder) via `SpeechToTextProvider` |
| Voice/audio output | `AudioManager` (I2S), `SarvamTextToSpeech` (placeholder) via `TextToSpeechProvider`, `AudioAssetManager`, `SoundManager` |
| Conversation & AI | `ConversationManager`, `GeminiClient`, `AiProvider`, `AiPipeline`, `TinyAIManager`, `IntentClassifier`, `OfflineResponseGenerator`, `LocalAIEngine`, `FunctionRouter`, `PersonalityManager` |
| UI | `DisplayManager`, `OledRenderer`, `UiFramework`, `ScreenManager`, `WidgetEngine`, `AnimationEngine`, `ThemeManager`, `InputManager`, `RendererManager`, `RendererInterface`, `LedRing` |
| Productivity/knowledge | `ExecutiveAssistant`, `BriefingManager`, `ReminderManager`, `StudyManager`, `GoalManager`, `HabitManager`, `PlannerManager`, `ReflectionManager`, `TimelineManager`, `DocumentManager`, `WorkspaceManager`, `ContextManager`, `KnowledgeGraphManager`, `SemanticSearchManager`, `SmartSearch`, `DecisionManager`, `LearningManager`, `PredictionManager`, `AnalyticsManager` |
| Reliability | `CrashManager`, `ResilienceManager`, `DiagnosticsManager`, `DiagnosticSystem`, `PerformanceManager`, `LogManager`, `Logger` |
| Extensibility | `PluginManager`, `SkillManager`, `AutomationManager`, `WorkflowEngine`, `CommandPalette`, `SceneEngine`, `StartupGreetingManager`, `CapabilityManager` |
| Security | `SecurityManager`, `FirmwareKeys`, `Secrets`, `VaultManager` |

## 3. Public API

### 3.1 `SystemManager` (global `systemManager`)
- `bool initialize() noexcept` â€” full boot orchestration.
- `void run() noexcept` / `void update() noexcept` â€” main-loop tick; calls
  `updateModules()` + `checkHealth()`.
- `void shutdown() noexcept` â€” save-all + graceful stop, reverse-order.
- `void restart() noexcept` â€” optional crash log, then `ESP.restart()`.
- `void factoryReset() noexcept` â€” format SPIFFS, clear
  credentials/settings/state, restart.
- `void enterLowPower() noexcept` / `void exitLowPower() noexcept`.
- `bool checkHealth() noexcept`, `const SystemInfo& getSystemInfo()`,
  `SystemState getState()`, `SystemError getError()`, `bool isInitialized()`,
  `bool isBusy()`, `bool isSafeMode()`.

### 3.2 Service framework
- `Service` base: `virtual bool Initialize()`, `virtual void Update()`,
  `virtual bool Start()`, `virtual bool Stop()`, `virtual void Shutdown()`,
  `virtual void Restart()`, `virtual void HandleEvent(const String&, const String&)`;
  `BootPriority`, `kLogCategory`.
- `ServiceManager`: `Initialize()`, `Register(Service*)`, `StartService(...)`,
  `StartAll()`, `Update()`, `Shutdown()`. **Note:** `Update()` only drives
  services in `RUNNING`/`SUSPENDED` state; nothing in the codebase calls
  `StartService`/`StartAll`.

### 3.3 Event bus (global `eventBus`)
- `initialize()`, `publish(EventType, const String& source, const String& payload)`,
  `subscribe(EventType, EventCallback)`, `unsubscribe(...)`, `update()`.
  Typed `EventType` enum (includes `HEALTH_*`, `NOTIFICATION_TRIGGERED`,
  `ANALYTICS_RECORD_ADDED`, `SEARCH_STARTED`, `SEARCH_COMPLETED`, ...).
  Dispatch cadence ~50 ms.

### 3.4 Storage (`storageManager`)
- `initialize()`, `update()`, `mountSPIFFS()`, `unmountSPIFFS()`,
  `mountSD()`, `unmountSD()`, `isSdMounted()`, `readFile(path, out, StorageType)`,
  `writeFile(path, data, StorageType)`, `appendFile(...)`, `deleteFile(...)`,
  `fileExists(...)`, `createDirectory(...)`, `listDirectory(...)`,
  `getFileSize(...)`, `formatSPIFFS()`. `StorageType`: `SPIFFS=0, SD_CARD=1,
  UNKNOWN=2`. Atomic writes via `.tmp` + `rename()`; hourly age-based cleanup.

### 3.5 AI / conversation
- `SarvamSpeechToText`: `initialize()`, `update()`, `startRecognition()`,
  `stopRecognition()`, `processAudio()`, `getResult()`, `isListening()` â€”
  placeholder ("Not Implemented") until the Sarvam integration is completed.
- `GeminiClient`: `initialize()`, `update()`, `setApiKey()`, `setApiEndpoint()`,
  `generate(prompt, ...)`, `cancelRequest()`, `isBusy()`, `isInitialized()`.
- `SarvamTextToSpeech`: `initialize()`, `update()`, `speak(text)`, `stop()`,
  `isSpeaking()`, `isInitialized()` â€” placeholder.
- `ConversationManager`: `initialize()`, `update()`, `startConversation()`,
  `stopConversation()`, `handleTranscribing()`, `isBusy()`, `isInitialized()`,
  `getHistory()`, `clearHistory()`.
- `TinyAIManager` (offline fallback), `IntentClassifier`,
  `OfflineResponseGenerator` (26 intents), `LocalAIEngine` (multi-engine
  offline coordinator), `FunctionRouter` (tool-call surface).
- `AiPipeline` (Service): `StartVoicePipeline()`, `StartTextPipeline()`,
  `StopPipeline()`. **Stub:** never called.

### 3.6 Web portal (`webPortal`)
- `initialize()`, `start()`, `stop()`, `update()`, `isAuthenticated()`,
  `isAuthenticatedOrReject()`. Largest REST/HTML surface in the system
  (serves dashboard, configuration, and CRUD for nearly every manager).

### 3.7 Key manager surfaces (representative)
- `MemoryManager`: `initialize()`, `remember(category, content, ...)`,
  `recall()`, `semanticSearch(query)`, `clear()`, `save()`, `memoryCount()`,
  `cleanup()`.
- `ReminderManager`: `addReminder(text, time, ...)`, `removeReminder(id)`,
  `getReminders()`, `clearReminders()`, `update()`, `save()`, `isBusy()`.
- `ExecutiveAssistant`: `initialize()`, `update()`,
  `getActiveRecommendations()`, `dismissRecommendation(id)`,
  `markRecommendationActed(id)`, `generateAllRecommendations()`,
  `saveRecommendations()`.
- `CrashManager`: `initialize()`, `update()`, `logCrashBeforeRestart(msg)`,
  `clearBootLoopCounter()`, `clearCrashes()`, `getAllCrashes()`,
  `acknowledgeCrash(id)`, `save()`.
- `WifiManager`: `initialize()`, `update()`, `connect(ssid, pwd)`,
  `reconnect()`, `disconnect()`, `startAccessPoint(ssid, pwd)`,
  `hasCredentials()`, `isConnected()`, `getState()`, `clearCredentials()`.
- `OtaManager`: `initialize()`, `update()`, `startUpdate(url)`,
  `abortUpdate()`, `cancelUpdate()`, `getProgress()`, `getState()`, `isBusy()`.
- `VaultManager`: `initialize()`, `update()`, `encryptData(plain, out)`,
  `decryptData(cipher, out)`, `save()` (mbedtls AES-256-GCM, 16-byte tag,
  `esp_random` IV).
- `SecurityManager`: `Authenticate(token)`, `Deauthenticate()`,
  `HasPermission(...)`, `CheckAccess(...)`, `LogAudit(...)`,
  `GetAuditLog(...)`, `CreateSession(...)`, `ValidateSession(...)`,
  `RevokeSession(...)`, `Encrypt/Decrypt` (return plaintext â€” stub),
  `GetSecurityScore()`. **Stub:** registered but never invoked as a service
  (the web portal uses its own token auth).

## 4. Internal Workflow

The system is a **cooperative single-task loop**. `Aura_programs.ino`
`loop()` calls `systemManager.run()`, which calls `update()`, which calls
`updateModules()` â€” a hardcoded sequential list of ~40 `.update()` calls
(`system_manager.cpp`). Modules are polled, not event-driven; the `EventBus`
and `ServiceManager` are updated from the same loop.

Initialization is a **sequential dependency-ordered boot**
(`initializeModules()`). Hard-fail modules (return `false` â†’ boot aborts â†’
`rollbackInitialization()`): `StorageManager`, `DisplayManager`,
`WiFiManager`, `AudioManager`, `SoundManager`, `WebPortal`, `EventBus`. All
others fail soft (warning + continue).

A watchdog is armed in `.ino` (`esp_task_wdt_add` on the loop task, 30 s) and
fed by each module's `update()`. Modules that miss the deadline trigger a
task-WDT reset.

Safe mode: if a boot-loop counter in NVS (`CRASH_COUNTER_NVS_NAMESPACE` /
`CRASH_COUNTER_KEY`) reaches `BOOT_LOOP_THRESHOLD` (3), or the touch pin
(GPIO13) is held for `SAFE_MODE_TOUCH_HOLD_MS` (3 s, >80% samples), only
essential modules load and OTA recovery is offered via the web portal.

Shutdown: reverse-order save-all (`shutdown()`) then graceful stop
(conversation, TTS, audio, LED, services, UI, display, web, WiFi, SPIFFS, SD).

## 5. Dependencies

### 5.1 Library dependencies
- `arduino-esp32` core 3.3.11 (ESP32 Arduino framework, FreeRTOS, mbedtls,
  `Preferences`/NVS, `ESPmDNS`, `esp_task_wdt`).
- `WiFi`, `AsyncTCP` / `ESPAsyncWebServer` (web portal).
- `Adafruit_SSD1306` / `Adafruit_GFX` (OLED 128x64 I2C).
- `ArduinoJson` (all JSON persistence and API payloads).
- WebSockets library (port 81 WebSocket server).
- mbedtls (AES-256-GCM in `VaultManager`; ECDSA P-256 + SHA-256 in OTA
  verification).

### 5.2 Internal dependency ordering (boot)
1. `StorageManager` (everything below needs it)
2. `MemoryManager` â†’ needs Storage
3. Service framework: `ServiceManager`, `CapabilityManager`,
   `PlatformAbstraction`, `TaskScheduler`
4. `DisplayManager` (status feedback early)
5. `WifiManager` â†’ Storage; starts AP if no credentials
6. `UiFramework` â†’ Display
7. `AudioManager` (I2S) â†’ `SoundManager`, `AudioAssetManager`
8. `WebPortal` â†’ WiFi
9. STT â†’ WiFi; `GeminiClient` â†’ WiFi; TTS â†’ WiFi + Audio
10. `ConversationManager` â†’ STT, Gemini, TTS
11. `ReminderManager`, `OtaManager`
12. `SettingsManager`, `PluginManager` (â†’SD), `SkillManager`,
    `PersonalityManager`, `ContextManager`
13. `PerformanceManager`, `CrashManager`, `DiagnosticsManager`
14. `KnowledgeGraphManager`, `GoalManager`, `HabitManager`, `PlannerManager`,
    `FunctionRouter`, `ReflectionManager`, `AutomationManager`,
    `StartupGreetingManager`, `TinyAIManager`
15. `TimelineManager`, `BriefingManager`, `SemanticSearchManager`,
    `DecisionManager`, `LearningManager`
16. `PredictionManager`, `DocumentManager`, `WorkspaceManager`, `VaultManager`
17. `EventBus`, `StudyManager`, `CompanionManager`, `EspNowManager`,
    `HealthMonitor`, `SmartSearch`, `AnalyticsManager`, `DeviceMesh`,
    `ExecutiveAssistant`
18. Registered services: `AiPipeline`, `CommandPalette`, `SceneEngine`,
    `WorkflowEngine`, `LogManager`, `SecurityManager`, `ResilienceManager`,
    `DiagnosticSystem`

### 5.3 API dependency (cloud)
- Gemini (`GEMINI_URL`) â€” `GeminiClient`.
- Sarvam AI (`SARVAM_BASE_URL`) â€” `SarvamSpeechToText` / `SarvamTextToSpeech`
  (placeholder; see [sarvam_ai.md](sarvam_ai.md)).

## 6. Data Flow

**Conversation prompt assembly** (`conversation_manager.cpp`): context
preamble + context memories (`contextManager`), then semantic memory search
(`memoryManager.semanticSearch`) â€” note: `SemanticSearchManager` is **not**
used here, it is portal-only. Prompt is enriched with:
`decisionManager.getRecentDecisions(3)`,
`executiveAssistant.getActiveRecommendations()`,
`predictionManager.getActivePredictions(0.5f)`,
`workspaceManager.getActiveWorkspace()`,
`studyManager.getDueSubjects()`. Response path: Gemini â†’ `FunctionRouter`
tool calls (goals, planner, etc.) â†’ TTS playback + OLED rendering.

**Offline fallback:** `OfflineResponseGenerator` / `LocalAIEngine` handle 26
intents (greeting, reminders, goals, habits, planner, memory, KG, decisions,
learning, recommendations, predictions, documents, workspaces, study,
flashcards, quizzes, skills), gated by `isInitialized()`.

**Proactive loop:** `ExecutiveAssistant.update()` recomputes prioritized
recommendations from study/workspace/habits/context/analytics; injected into
prompts and exposed via web portal. `ReflectionManager` (once/day) runs
memory consolidation â†’ `knowledgeGraphManager.autoLink()` â†’
`briefingManager.generateTodaySummary()`.

**Persistence:** `StorageManager` is the single write authority. Modules
persist JSON files to SPIFFS (`/memory`, `/reminders`, `/goals.json`,
`/habits.json`, `/plans.json`, `/reflections.json`, `/timeline`,
`/briefings`, `/knowledge_graph.json`, `/documents/`, `/workspaces/`,
`/decisions/`, `/predictions/`, `/analytics.json`, `/study/`,
`/plugin_marketplace.json`) and to SD for audio assets. Writes are atomic
(`.tmp` + `rename`). NVS holds WiFi (`aura_wifi`: `ssid`/`password`) and
crash counter (`auracrash`: `crash_cnt`).

## 7. Boot Sequence

1. `setup()`: `Serial.begin(115200)` â†’ `Logger::initialize()` â†’
   `logBootDiagnostics()` â†’ `initializeWatchdog()` (30 s task WDT) â†’
   `systemManager.initialize()`.
2. `SystemManager::initialize()`:
   - Reads NVS boot-loop counter pre-init; sets `m_safeMode` if â‰¥ 3.
   - `changeState(INITIALIZING)`; fills `SystemInfo`.
   - `initializeModules()` â€” sequential module init (see Â§5.2); on hard
     failure â†’ `rollbackInitialization()` + `ERROR`/`INIT_FAILED` +
     `displayManager.showError`.
   - Touch-based safe-mode scan (3 s non-blocking poll, watchdog-fed).
   - Safe mode: limited modules + "Use Web Portal for OTA recovery".
   - Success: `crashManager.clearBootLoopCounter()`;
     `validateCredentials()` (warns on missing `Secrets::*`);
     `startupGreetingManager.start()` (unless safe mode); `READY`.
3. `loop()`: `systemManager.run()` â†’ `update()` â†’ `updateModules()` +
   periodic `checkHealth()` (interval `kHealthCheckIntervalMs`).
4. Post-boot cleanups in `rollbackInitialization()` mirror:
   `crashManager.clearCrashes()`, `reminderManager.clearReminders()`.

## 8. Runtime Behavior

- **Main loop polling:** every module's `update()` runs each loop; health
  check every `kHealthCheckIntervalMs`; `HealthMonitor` internally samples
  every 30 s (`kCheckIntervalMs`), alert cooldown 5 min; `PerformanceManager`
  samples every 5 s (`PERF_SAMPLE_INTERVAL_MS`).
- **State machine:** `SystemState` â€” `BOOTING â†’ INITIALIZING â†’ READY â†”
  BUSY/LOW_POWER/UPDATING â†’ ERROR/SHUTDOWN` â€” enforced by a static
  valid-transition matrix; invalid transitions are logged and rejected.
- **Low power:** `enterLowPower()` stops conversation/audio/display/LED,
  disconnects WiFi; `exitLowPower()` reconnects and wakes display.
- **Reminder annunciation:** state machine `IDLE â†’ WAITING â†’ TRIGGERED â†’
  SPEAKING â†’ COMPLETED/ERROR`; `isBusy()` gates system sleep.
- **Display:** `DisplayManager` drives an OLED (128x64 @ 0x3C, I2C SDA 21 /
  SCL 22) with a `DisplayState` machine, 11 screen types, 30 s
  screen-timeout auto-sleep, widget dirty-tracking; `UiFramework` +
  `ScreenManager` + `WidgetEngine` + `AnimationEngine` on top.
- **WiFi:** single radio owner â€” `WifiManager` (see
  [SOFTWARE.md](SOFTWARE.md)); connect with stored creds or start AP; bounded
  reconnect (attempt budget + backoff); mDNS host; NTP sync.
- **Wiring gaps (behavioral impact):**
  - `ServiceManager::Update()` never runs registered services (no
    `StartService`/`StartAll`); `SecurityManager`, `AiPipeline`,
    `ResilienceManager`, `CommandPalette`, `SceneEngine`, `WorkflowEngine`,
    `LogManager`, `DiagnosticSystem` are registered but inert as services
    (some expose functions driven elsewhere â€” e.g. `commandPalette` is called
    from conversation paths; their Service `Update()`/`HandleEvent` are not
    driven).
  - `GeminiClient`/`SarvamSpeechToText`/`SarvamTextToSpeech` `initialize()`
    fail closed at "API endpoint not set" / "missing key/CA" guards; cloud AI
    is effectively unavailable in the shipped build (offline engine is used).
  - `SmartSearch.search()/quickSearch()` â€” zero callers; feature unreachable.
  - `TimelineManager.addEntry()` â€” zero callers; every timeline read is empty.
  - `AnalyticsManager.record()` â€” zero callers; study minutes/trend always 0.
  - `ReminderManager.addReminder()` â€” no web-portal endpoint; reminders
    cannot be created from the UI.
  - `ContextManager` setters â€” only 4 of 16 are ever called (mood,
    conversation count, last conversation, conversation topic â€” all from
    `ConversationManager`); prompt context is largely default/static.
  - `DocumentManager.reindexAll()/linkToGraph()/summarize()` â€” zero callers;
    documents never enter the knowledge graph.
  - `LearningManager.observe()` â€” portal-only; no organic observation sources.
  - `HealthMonitor` events published with zero subscribers;
    `getSnapshot()/getActiveAlerts()` un-consumed.

## 9. User Features

| Feature | Entry point | Status |
| --- | --- | --- |
| Voice assistant (STTâ†’Geminiâ†’TTS) | Wake via touch (GPIO13) / command | Partially wired (cloud endpoints not set; Sarvam placeholders) |
| Offline assistant responses | `OfflineResponseGenerator` / `LocalAIEngine` (26 intents) | Complete |
| Web configuration & dashboard | `WebPortal` (REST + WebSocket, token auth) | Complete â€” largest live consumer |
| OTA firmware update (signed) | `OtaManager` + web portal | Complete (ECDSA P-256 + streaming SHA-256 verify) |
| Reminders | `ReminderManager` | Broken â€” cannot be created |
| Goals / Habits / Planner | Managers + web portal | Complete |
| Study (subjects, sessions, flashcards, quizzes) | `StudyManager` + web portal | Complete (portal start-session path broken) |
| Daily briefings | `BriefingManager` | On-demand only (no auto schedule) |
| Daily reflection | `ReflectionManager` (once/day) | Complete |
| Knowledge graph | `KnowledgeGraphManager` + web portal | Complete (auto-link via reflection) |
| Documents + workspaces | `DocumentManager` / `WorkspaceManager` + web portal | Partial (docs never linked/summarized/reindexed) |
| Semantic search | `SemanticSearchManager` (portal only) / `MemoryManager.semanticSearch` (conversation) | Partial |
| Unified search | `SmartSearch` | Dead (never called) |
| Decisions | `DecisionManager` + web portal | Complete |
| Learning/insights | `LearningManager` | Partial (manual observations only) |
| Predictions | `PredictionManager` | Structurally complete; consumes empty history |
| Analytics | `AnalyticsManager` | Dead writer (never recorded) |
| Recommendations | `ExecutiveAssistant` | Complete, fed by empty analytics |
| Plugins / skills | `PluginManager` / `SkillManager` | Config-only plugins; SD discovery stubbed |
| Automation / workflows / scenes / command palette | `AutomationManager`, `WorkflowEngine`, `SceneEngine`, `CommandPalette` | Partially wired (see Â§8) |
| Startup greeting | `StartupGreetingManager` | Complete |
| Personality profiles | `PersonalityManager` | Complete |
| LED ring effects | `LedRing` (GPIO4) | Complete |
| Companion / device mesh (ESP-NOW) | `CompanionManager`, `EspNowManager`, `DeviceMesh` | Partially wired |

## 10. Hardware Interaction

See [../testing/WIRING.md](../testing/WIRING.md) for the full verified pinout. Summary (pins from
`config.h`):

| Hardware | Interface | Pins (config.h) | Driver |
| --- | --- | --- | --- |
| OLED (128x64) | I2C @ 0x3C | SDA=21, SCL=22 | `DisplayManager`/`OledRenderer` (Adafruit_SSD1306) |
| Mic INMP441 | I2S (mono, 16-bit) | BCLK=26, WS=25, DATA=34 | `AudioManager` (I2S_NUM_0) |
| Speaker MAX98357A | I2S | BCLK=27, LRC=14, DATA=12 | `AudioManager` (I2S_NUM_1) |
| LED ring | GPIO (WS2812-style) | GPIO4 | `LedRing` |
| Touch sensor | Capacitive touch | GPIO13 (threshold 40, debounce, safe-mode hold 3 s) | `InputManager`, boot scan |
| SD card | SPI | CS=5, MOSI=23, MISO=19, SCK=18 | `StorageManager` (SD via SPI) |
| Flash / NVS | internal | â€” | Storage, `Preferences`, crash counter |

## 11. Configuration

- **`config.h`** â€” hardware pins, `aura::identity` (`kProjectName` "AURA AI
  Desktop Assistant", `kHardwareRev` "MK-II", `kAuthor` "Devil"), AI endpoint
  URLs, timing constants (`PERF_SAMPLE_INTERVAL_MS`=5000,
  `SAFE_MODE_TOUCH_*`, watchdog params, `WS_PING_INTERVAL_MS`=30000).
- **`version.h`** â€” single-source versioning: `kMajor/kMinor/kPatch` (1.0.0),
  `kMark`=3 (MARK III), `kCodename` "Phoenix", `kChannel` "Development",
  `kOsName` "AURA OS", compile-time build date/time.
- **`secrets.h`** â€” `GEMINI_API_KEY`, `SARVAM_API_KEY`, `AP_SSID`,
  `AP_PASSWORD`, `WEB_USERNAME`, `WEB_PASSWORD`, OTA credentials, future
  service placeholders. Missing values are warned at boot via
  `validateCredentials()`. Git-ignored; use the `secrets.h.example` template.
- **`firmware_keys.h`** â€” ECDSA P-256 public key for OTA signature
  verification.
- **Runtime settings** â€” `SettingsManager` (user prefs), WiFi creds in NVS
  `aura_wifi`, personality profile selection, plugin marketplace file
  `/plugin_marketplace.json`.

## 12. Error Recovery

- **Watchdog:** 30 s task WDT on the loop task; stuck module â†’ device reset.
- **Boot-loop protection:** NVS counter incremented on abnormal restart;
  â‰¥3 â†’ Safe Mode; cleared on successful boot
  (`crashManager.clearBootLoopCounter()`).
- **Safe mode:** touch-held or boot-loop triggered; loads
  storage/display/wifi/portal/OTA only; message "Use Web Portal for OTA
  recovery".
- **Crash reporting:** `CrashManager` persists reports to `/crash/` (max 10),
  `logCrashBeforeRestart()` from the error handler; review/ack via web portal
  (getAllCrashes/acknowledgeCrash/clearCrashes).
- **Rollback:** failed `initializeModules()` triggers
  `rollbackInitialization()` â€” reverse-order save + graceful stop per
  initialized module index.
- **ResilienceManager:** 12 `FailureType` recovery plans (WiFi/SD/Renderer/AI)
  with attempts/cooldowns â€” **dead code**: only `Initialize()` is called;
  `ReportFailure()`/`AttemptRecovery()`/`Recover*()` have zero callers.
  Crashes are logged but never automatically recovered.
- **Factory reset:** `factoryReset()` clears settings, credentials, and all
  module state, then restarts.
- **Network resilience:** WiFi auto-reconnect with retries/backoff; AI
  provider initialization is non-fatal (warning + continue).

## 13. System Architecture

```
                    Aura_programs.ino (setup / loop)
                              â”‚
                    â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â–¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
                    â”‚    SystemManager   â”‚  state machine + health
                    â”‚  (single loop)     â”‚
                    â””â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”˜
                        â”‚      â”‚      â”‚
        updateModules() â”‚      â”‚      â”‚ checkHealth()
   (sequential poll,    â”‚      â”‚      â””â”€ memory/WiFi/OTA/reminder/conversation monitors
    ~40 managers)       â”‚      â”‚
                        â”‚      â””â”€â”€ ServiceManager (registered services â€” NOT started)
                        â”‚                 â”œâ”€ CapabilityManager, PlatformAbstraction,
                        â”‚                 â”œâ”€ TaskScheduler, AiPipeline, CommandPalette,
                        â”‚                 â”œâ”€ SceneEngine, WorkflowEngine, LogManager,
                        â”‚                 â”œâ”€ SecurityManager, ResilienceManager, DiagnosticSystem
                        â”‚
        Core: StorageManager Â· MemoryManager Â· EventBus Â· Logger
        Voice: AudioManager(I2S) Â· SarvamSTT Â· GeminiClient Â· SarvamTTS Â· TinyAI Â· Offline
        UI:    DisplayManager Â· UiFramework Â· ScreenManager Â· WidgetEngine Â· LedRing
        Net:   WifiManager Â· WebPortal Â· OtaManager Â· EspNowManager Â· DeviceMesh Â· Companion
        Prod:  ExecutiveAssistant Â· Briefing Â· Reminder Â· Study Â· Goal Â· Habit Â· Planner Â·
               Reflection Â· Timeline Â· Document Â· Workspace Â· Context Â· KG Â· SemanticSearch Â·
               SmartSearch Â· Decision Â· Learning Â· Prediction Â· Analytics
        Rel:   CrashManager Â· ResilienceManager Â· Diagnostics Â· Performance Â· HealthMonitor
        Ext:   Plugin Â· Skill Â· Automation Â· WorkflowEngine Â· SceneEngine Â· CommandPalette
```

**Design pattern:** each subsystem is a global singleton manager exposing
`initialize()/update()/save()`, wired to the `SystemManager` loop. An EventBus
provides decoupled notifications. A parallel Service framework exists but is
not driven into the RUNNING state. The web portal is the primary interactive
surface (REST + HTML + WebSocket) and the sole trigger for several features.

## 14. Feature Map

**Wiring verdict legend:** COMPLETE (driven end-to-end) Â· PARTIAL (initialized
+ partially consumed) Â· DEAD/STUB (no external callers).

| Module | Verdict | Notes |
| --- | --- | --- |
| SystemManager | COMPLETE | Orchestrates all |
| StorageManager | COMPLETE | Single write authority |
| MemoryManager | COMPLETE | Semantic search used by conversation |
| WifiManager | COMPLETE | AP/STA, NVS creds, mDNS, NTP |
| WebPortal | COMPLETE | Largest consumer, own auth |
| OtaManager | COMPLETE | Signed updates |
| DisplayManager / UiFramework / OledRenderer | COMPLETE | 11 screens, 30 s timeout |
| AudioManager / SoundManager / AudioAssetManager | COMPLETE | I2S mic+spk, asset playback |
| LedRing | COMPLETE | GPIO4 effects |
| InputManager | COMPLETE | Touch |
| ConversationManager | COMPLETE | Enriched prompt, tool routing |
| TinyAIManager / OfflineResponseGenerator / IntentClassifier | COMPLETE | 26 offline intents |
| LocalAIEngine | COMPLETE | Multi-engine offline coordinator (Phase 3) |
| FunctionRouter | COMPLETE | Tool-call execution |
| PersonalityManager | COMPLETE | Profiles |
| ExecutiveAssistant | COMPLETE | Fed by empty analytics |
| BriefingManager | COMPLETE (on-demand) | No auto morning/evening schedule |
| StudyManager | COMPLETE | Portal start-session path broken |
| GoalManager / HabitManager / PlannerManager | COMPLETE | Full CRUD + prompt injection |
| ReflectionManager | COMPLETE | Once/day chain |
| KnowledgeGraphManager | COMPLETE | Auto-link via reflection |
| DocumentManager | PARTIAL | store/search OK; reindex/link/summarize dead |
| WorkspaceManager | COMPLETE | CRUD + prompt |
| ContextManager | PARTIAL | 12/16 setters unused; mostly static prompt |
| SemanticSearchManager | PARTIAL | Portal-only; conversation bypasses it |
| SmartSearch | DEAD | search/quickSearch never called |
| DecisionManager | COMPLETE | Prompt injection + portal |
| LearningManager | PARTIAL | Portal-only observations |
| PredictionManager | COMPLETE | Consumes empty history |
| AnalyticsManager | DEAD | record() never called |
| TimelineManager | DEAD (write path) | addEntry never called; reads always empty |
| ReminderManager | DEAD (create path) | addReminder rejected; no portal endpoint |
| CrashManager | COMPLETE | NVS counter, logs, portal ack |
| ResilienceManager | DEAD | Only Initialize() called |
| HealthMonitor | PARTIAL | Publishes events with zero subscribers |
| PerformanceManager | PARTIAL | Consumed by portal/REST; latency fields dead |
| DiagnosticsManager / DiagnosticSystem / LogManager | PARTIAL | Surface exposed; not fully driven |
| SecurityManager | STUB | Registered only; Encrypt/Decrypt return plaintext; portal uses own auth |
| VaultManager | COMPLETE | AES-256-GCM |
| PluginManager | PARTIAL | Config-only; SD discovery stubbed |
| SkillManager | PARTIAL | Registered; limited callers |
| AutomationManager / WorkflowEngine | PARTIAL | Registered; partial trigger surface |
| CommandPalette / SceneEngine | PARTIAL | Registered as services (not started) + direct calls |
| StartupGreetingManager | COMPLETE | Runs on boot (non-safe mode) |
| AiPipeline | DEAD | Start*Pipeline never called |
| EspNowManager / DeviceMesh / CompanionManager | PARTIAL | Init + update; limited external consumers |

## 15. Code Statistics

- **Total:** ~160 source files â€” ~84 headers, ~75 `.cpp`, 1 `.ino`.
- **Total lines:** ~42K (headers ~12K Â· sources ~29.5K Â· sketch ~134).
- **Largest module:** `web_portal.cpp` ~2,900 lines.
- **Build config:** `arduino-cli compile --fqbn
  "esp32:esp32:esp32:PartitionScheme=huge_app" Aura_programs`.
- **Flash:** ~1,938,167 / 3,145,728 bytes (61%) â€” *default partition scheme
  fails ("text section exceeds available space"); requires Huge APP (3MB No
  OTA/1MB SPIFFS).* (Latest builds ~63%.)
- **RAM:** ~79,000 / 327,680 bytes (24%) global.
- **Warnings:** 0 (clean build).
- **Toolchain:** arduino-cli, Arduino IDE 2.x backend, ESP32 core 3.3.11.

## 16. Final Summary

AURA OS is a feature-dense, single-loop ESP32 assistant firmware with a clean
`SystemManager`-orchestrated lifecycle, robust persistence (atomic writes via
a single StorageManager authority), signed OTA, NVS-based boot-loop
protection, and a large web portal surface. It compiles clean and boots
deterministically.

The most significant architectural observation is that a substantial fraction
of the code is **initialized but not driven**: the entire Service framework is
registered but never started; the cloud AI endpoints are never configured in
the shipped build (offline engine is used); and several whole features are
unreachable (`SmartSearch`, `ResilienceManager` recovery,
`AnalyticsManager` recording, `TimelineManager` writing, reminder creation,
`AiPipeline`). The web portal is the dominant live consumer of the
productivity features, while the conversational assistant path depends on
cloud configuration that is never applied at runtime. Modules flagged
STUB/DEAD in Â§14 should be treated as scaffolding or reserved functionality
rather than active capabilities.

