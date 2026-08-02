# AURA OS Changelog

All notable changes to AURA OS are documented here.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
AURA OS is currently at version `1.0.0` (Mark III "Phoenix"), Development channel.
Build metrics baseline: 61% flash (1,938,167 / 3,145,728 B) · 24% RAM (79,000 / 327,680 B) · 0 warnings.

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
