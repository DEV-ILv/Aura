# AURA Startup Diagnostic

**Date:** 2026-08-13
**Method:** Live serial capture (COM7) + static source audit. No source modified, no config changed, no firmware flashed, nothing committed/pushed.
**Scope:** Determine exactly what happens from power-on to READY, and whether the Wi-Fi/ESP-NOW architecture can explain the previous app-related ESP32 restarts.

---

## Firmware

| Item | Value |
|---|---|
| Version | 1.0.0 |
| Git commit (HEAD) | `e720c42` (Merge branch 'main' of github.com/DEV-ILv/Aura) |
| Branch | `main` |
| Build state | Working tree has **uncommitted** Wi-Fi single-ownership fixes (6 files, +183/-30). **NOT flashed** — device runs HEAD build. |
| Flashed banner | `Firmware 1.0.0, Board ESP32-WROOM-32, Headless DISABLED (normal)` |
| Build ID | `Initialized (1 events, boot 3717)` / `Uptime ... boots: 269` |

---

## Hardware

| Item | Value | Status |
|---|---|---|
| Board | ESP32-WROOM-32 (DevKit-C style) | LIVE |
| Chip | ESP32-D0WD-V3 (revision v3.1) | LIVE (esptool) |
| Cores | 2 (+LP core), 240 MHz | LIVE |
| Crystal | 40 MHz | LIVE |
| Flash | 4 MB (GigaDevice, 3.3 V strapping) | LIVE (esptool flash-id) |
| PSRAM | None reported (not present/configured) | STATIC |
| Serial port | COM7 — Silicon Labs CP210x USB-to-UART (VID_10C4 PID_EA60) | LIVE, openable |
| MAC | <DEVICE_MAC> | LIVE |
| LAN IP (STA) | <LAN_IP> (ports 80 + 81 confirmed OPEN) | LIVE |
| SD card | SDHC 15,193 MB, FAT32, mounted | LIVE |

**Conclusion:** ESP32 **is physically connected and live** at COM7. All live tests below were executed on the running device.

---

## Reset Reason

**Captured from live boot log (UptimeMonitor):**
```
[Uptime] Uptime monitor started (reset reason: power-on, boots: 269)
```

- Exact result: **POWER_ON (ESP_RST_POWERON)** — this boot was triggered by my own hardware reset (RTS pin) during capture, so a clean power-on reason is expected.
- **Lifetime boot counter: 269** (persisted in NVS). This is a hardware resets + spontaneous restarts + development-flash count accumulated across the device lifetime.
- CrashManager: **1 crash logged** (persisted); boot-loop counter currently **0/3** (clean-boot path resets it). `BOOT_LOOP_THRESHOLD = 3`.
- ErrorManager: **1 event loaded** at boot.

**Important:** Because the observed reset reason was *induced* by the diagnostic procedure, this does **not** establish the cause of the previous spontaneous restarts. No `PANIC`, `TASK_WDT`, `BROWN_OUT`, `Guru Meditation`, `abort()`, `assert()`, `LoadProhibited`, `StoreProhibited`, `IllegalInstruction`, or `InstrFetchProhibited` text appeared in any capture.

---

## Boot Timeline

All times in milliseconds from first logger timestamp. Live capture; the first ~3.2 s (ROM boot + `.ino` `logBootDiagnostics`) were not captured on the wire, so those cells are STATIC/NOT-CAPTURED.

| Stage | Start (ms) | End (ms) | Duration | Status |
|---|---|---|---|---|
| ROM bootloader / `logBootDiagnostics` | 0 | ~3000 | — | NOT CAPTURED (port open latency); chip info verified via esptool |
| ServiceStatus | 3199 | 3199 | — | OK |
| StorageManager (SPIFFS mount) | 3199 | 3706 | 507 | OK |
| SD card (async mount) | 3760 | 3784 | 24 | OK (SDHC 15 GB, FAT32) |
| ErrorManager | 3717 | 3730 | 13 | OK (1 persisted event) |
| MemoryManager | 3730 | 3833 | 103 | OK |
| ServiceManager | 3833 | 3844 | 11 | OK |
| CapabilityManager | 3844 | 3856 | 12 | OK (50 caps, 16 avail) |
| PlatformAbstraction | 3856 | 3867 | 11 | OK |
| TaskScheduler | 3867 | 3879 | 12 | OK |
| DisplayManager | 3879 | 3997 | 118 | OK |
| **WiFiManager** | 3997 | 4106 | 109 | OK |
| **STA connect begins** | 4106 | — | — | "Reconnecting to `<SSID>`" |
| UIFramework / OLED | 4120 | 4176 | 56 | OK (OLED 0x3C) |
| AudioManager (+mic self-test ~2.8 s) | 4177 | 7044 | 2867 | OK (mic RMS logged; speaker offline) |
| LedRing (16 WS2812B, GPIO4) | 7044 | 7069 | 25 | OK |
| WebPortal | 7091 | 9634 | 2543 | OK |
| SpeechToText (SarvamSTT) | 7091 | 7139 | 48 | **FAIL** (missing key/CA) — continues |
| GeminiClient | 7151 | 7162 | 11 | **FAIL** ("WiFi not connected") — continues |
| TextToSpeech (SarvamTTS) | 7166 | 7174 | 8 | OK (voice=meera) |
| ConversationManager | 7178 | 7208 | 30 | OK (offline/touch mode) |
| ReminderManager | 7219 | 7269 | 50 | OK (0 reminders) |
| OtaManager | 7280 | 7303 | 23 | **FAIL** ("WiFi not connected") — continues |
| SettingsManager | 7303 | 7312 | 9 | OK |
| PluginManager / SkillManager / Personality / Context | 7320 | 7478 | 158 | OK |
| PerformanceManager | 7478 | 7823 | 345 | OK |
| CrashManager | 7823 | 7841 | 18 | OK (1 crash logged) |
| UptimeMonitor | 7841 | 7841 | 0 | OK (power-on, boots 269) |
| Diagnostics / KnowledgeGraph / Goals / Habits / Planner / FunctionRouter / Reflection / Automation | 7841 | 8163 | 322 | OK |
| StartupGreeting / TinyAI / Timeline / Briefing / SemanticSearch / Decision / Learning / Prediction / Documents / Workspace / Vault / EventBus / Study / Companion | 8174 | 9240 | 1066 | OK |
| **EspNowManager** | 9240 | 9271 | 31 | OK — **"ESP-NOW initialized (channel 1)"** |
| HealthMonitor / SmartSearch / Analytics / DeviceMesh / Executive / AIPipeline / CommandPalette / Scene / Workflow / Log / Security / Resilience / Diagnostic | 9271 | 9452 | 181 | OK |
| **Banner + READY** | ~9452 | ~9560 | ~110 | **READY** |
| Web server + WebSocket start | 9610 | 9634 | 24 | OK (port 80, WS 81) |
| System init complete | 9645 | 9669 | 24 | "complete (9458 ms)", free heap 48088 B |

**Boot to READY ≈ 9.5 s.** No stage is unusually slow except the intentional ~2.8 s mic self-test. Full init time 9,458 ms is long but not pathological for this module count (54 modules).

---

## Memory

| Point | Free heap |
|---|---|
| Platform init (log) | 248 KB (reported, early) |
| API route registration (WebPortal) | 49,180 B |
| READY / System init complete | **48,088 B** |
| Minimum free heap | NOT CAPTURED (boot header missed) |
| Largest free block | NOT CAPTURED |

- **HEAP AT READY: 48,088 B** (~47 KB) out of ~320 KB usable.
- Boot consumes ~200 KB of heap (module registration, JSON strings, route tables, OLED/LED buffers).
- Low but **stable**: no allocation failure in any capture; the device does not crash from heap alone.
- Do **not** assume low heap automatically means a crash — no `bad_alloc`, no OOM panic observed.

---

## Wi-Fi

Live-observed sequence during boot (flashed HEAD firmware):

1. `3997 ms` — WifiManager initialize → `WiFi.mode(STA)` + hostname.
2. `4106 ms` — `Connecting to saved WiFi...` → `Reconnecting to <SSID>`.
3. `~7 s` — GeminiClient/OtaManager fail because **STA not yet connected** at init time (expected; they are disabled services anyway).
4. `9240 ms` — **EspNowManager initialize runs `WiFi.mode(WIFI_AP_STA)` + `WiFi.channel(ESPNOW_CHANNEL=1)`** (HEAD code) while WifiManager is still in its STA connect sequence.
5. Banner (~9.5 s): prints `Wi-Fi : AP Mode (AURA_Setup)` **but** `REST : http://<DEVICE_IP>/api` and `WebSocket : ws://<DEVICE_IP>:81`.
6. Post-boot: device responds at **<DEVICE_IP>:80 and :81** (verified OPEN) — i.e. the radio reached STA-connected state and the web server is reachable.

### Findings
- **State inconsistency observed live:** banner reports "AP Mode" while `WiFi.localIP()` = <DEVICE_IP> (a real STA address). This is WifiManager's internal state (`m_currentState != CONNECTED`) disagreeing with the actual radio (STA has IP, server up). The ESP-NOW `WIFI_AP_STA` + channel-1 forcing at 9.27 s runs *while* WifiManager is mid-connect — a live instance of the multi-owner radio conflict.
- **Channel:** ESP-NOW forces channel 1 at init (HEAD firmware). If the router is on another channel, ESP-NOW initialization can pull the radio channel mid-boot. This is the conflict the unflashed working-tree fix removes (peer channel 0 = follow WifiManager channel).
- **Reconnect:** WifiManager state machine (15 s timeout, 5 s backoff, ≤5 attempts then ERROR 30 s retry) is bounded. No reconnect loop observed in any live capture.
- **SSID:** present in saved NVS. **REDACTED** to `<SSID>` per security policy. No password/keys exposed anywhere in this report.

---

## ESP-NOW

| Item | Value (live) |
|---|---|
| Initializes | Yes, at 9240 ms (late in boot, after WifiManager began STA connect) |
| Calls `WiFi.mode()` | **Yes** — `WiFi.mode(WIFI_AP_STA)` (HEAD code; removed in working tree) |
| Calls `WiFi.channel()` | **Yes** — `WiFi.channel(ESPNOW_CHANNEL)` = channel **1** (HEAD code; removed in working tree) |
| Result | `esp_now_init` OK, "ESP-NOW initialized (channel 1)" |
| Peer channel | `ESPNOW_CHANNEL` (=1) on HEAD; `0` (follow current) in working tree |

**Finding (P1):** On the flashed firmware, ESP-NOW **changes the Wi-Fi state established by WifiManager** by forcing `WIFI_AP_STA` + channel 1 during boot — the exact defect fixed in the unflashed working tree.

---

## Wi-Fi Ownership

Static search results for `WiFi.mode( / WiFi.channel( / WiFi.begin( / WiFi.softAP( / WiFi.disconnect( / WiFi.reconnect(`:

| # | File | Function | Purpose | Owner (flashed/HEAD) | Owner (working tree) |
|---|---|---|---|---|---|
| 1 | `wifi_manager.cpp` | `ensureMode()` | `WiFi.mode(mode)` guarded by current-mode check | WifiManager | WifiManager |
| 2 | `wifi_manager.cpp` | `connect()` | `WiFi.begin(ssid, pass)` | WifiManager | WifiManager |
| 3 | `wifi_manager.cpp` | `attemptConnection()` | `WiFi.begin(...)` | WifiManager | WifiManager |
| 4 | `wifi_manager.cpp` | `disconnect()` | `WiFi.disconnect(true)` | WifiManager | WifiManager |
| 5 | `wifi_manager.cpp` | `startAccessPoint()` | `WiFi.softAP(...)` | WifiManager | WifiManager |
| 6 | `wifi_manager.cpp` | `stopAccessPoint()` | `WiFi.softAPdisconnect(true)` | WifiManager | WifiManager |
| 7 | `wifi_manager.cpp` | scan | `WiFi.channel(index)` read-back | WifiManager | WifiManager |
| 8 | `wifi_manager.cpp` | handleEvents/checkConnection | `WiFi.channel()` read-back | WifiManager | WifiManager |
| 9 | **`esp_now_manager.cpp` (HEAD)** | `initialize()` | **`WiFi.mode(WIFI_AP_STA)` + `WiFi.channel(ESPNOW_CHANNEL)`** | **EspNowManager** | removed (working tree) |
| 10 | **`resilience_manager.cpp` (HEAD)** | `RecoverWiFi()` | **`WiFi.reconnect()`** | **ResilienceManager** | `wifiManager.reconnect()` (working tree) |
| 11 | **`system_manager.cpp` (HEAD)** | `monitorWiFi()` | **`wifiManager.reconnect()` every health tick (~5 s)** | **SystemManager** | removed (working tree) |

**Answer: Is WifiManager currently the single authority over Wi-Fi?**
- **Flashed firmware (HEAD): NO.** Competing owners: **EspNowManager** (mode + channel), **ResilienceManager** (direct `WiFi.reconnect()`), **SystemManager::monitorWiFi()** (reconnect amplifier). 3 competing owners + WifiManager = 4 total.
- **Working tree (unflashed): YES.** All radio control confined to `wifi_manager.cpp`; the other two call sites route through `wifiManager`.

---

## Reconnect Diagnostics

| Item | Finding |
|---|---|
| `monitorWiFi()` | HEAD: calls `wifiManager.reconnect()` when state==DISCONNECTED every health tick (~5 s, once-per-500 ms task cadence). Working tree: no-op (WifiManager owns recovery). |
| `reconnect()` | Guards re-entry while `CONNECTING` (working tree). HEAD had no such guard. |
| `attemptConnection()` | Bounded: `CONNECTION_TIMEOUT_MS=15 s`, backoff `RECONNECT_DELAY_MS=5 s`, `MAX_CONNECTION_ATTEMPTS=5`, then ERROR with 30 s retry (working tree). |
| `handleReconnect()` | DISCONNECTED state: reconnects after 5 s backoff; budget exhaustion → ERROR (working tree). |
| Multiple tasks calling reconnect | **Possible on HEAD**: `monitorWiFi()` (5 s), ResilienceManager (recovery), WifiManager state machine — no interlock. **Eliminated in working tree** (single path + CONNECTING guard). |
| Reconnect while another attempt active | HEAD: possible (stacked `WiFi.begin()` = documented crash vector). Working tree: prevented. |
| Repeated `WiFi.mode()` | HEAD: possible via ESP-NOW + WifiManager. Working tree: `ensureMode()` guards so `WiFi.mode()` is skipped when mode unchanged. |
| Exponential backoff | No — fixed 5 s, then 30 s ERROR retry. Bounded rate, no loop. |
| Reconnect loop possible | No loop observed live; HEAD architecture could stack begin()/reconnect() calls from 3 sources under repeated link-loss. |

---

## WebPortal / WebSocket Startup

| Item | Value |
|---|---|
| Routes | 154 API dispatcher routes registered; standard routes (/, /status, /wifi, /restart, /factory-reset, /ota, /dashboard, etc.) |
| WebSocket | `WebSocketsServer` on port 81; `begin()` guarded by `bad_alloc` catch (HTTP-only fallback) |
| Auth | Loaded credentials; every WS client must pass `X-Auth-Token` message before telemetry (`auth_required` sent otherwise) |
| Broadcast cadence | Dashboard broadcast every `WS_PUBLISH_INTERVAL_MS = 500 ms`, ping every 30 s; **only to authenticated, connected clients** |
| Immediate startup broadcast | **No** — `webSocketBroadcastDashboard()` early-returns when `connectedClients() == 0`; no data leaks to unauthenticated clients |
| Heap around WebPortal | 49,180 B at route registration; 48,088 B at READY |
| Cross-origin guard | WS handshake origin validated against localIP/softAPIP/hostname |

**No immediate WebSocket broadcast during startup; no flood source here.**

---

## FreeRTOS

Only 4 explicit tasks are created by firmware code (everything else runs in the Arduino `loop` task):

| Task | Priority | Stack | Core | Purpose | Notes |
|---|---|---|---|---|---|
| `aura_sd` | 1 | 4096 | tskNO_AFFINITY | Async SD mount | One-shot, deletes itself; prevents SD probe stalling WDT loop |
| `restart_task` | 1 | 4096 | any | Delayed `ESP.restart()` (web /restart) | One-shot |
| `factory_reset_task` | 1 | 4096 | any | Clear creds + restart | One-shot |
| `ota_restart_task` | 1 | 4096 | any | Restart after OTA | One-shot |

- Wi-Fi/WebSocket/storage logic runs on the main loop task (registered with the 30 s task WDT; re-created from the Arduino core's tighter default timeout).
- No task created before its dependency initializes (all tasks created after their owning modules init).
- No evidence of a race condition; small stack sizes are limited to short-lived one-shot tasks (4096 B is fine for them).

---

## Errors

| Severity | Event (live boot) | Resolution |
|---|---|---|
| WARNING | `ServiceManager AIPipeline health: 1` (DEGRADED, repeats ~10 s) | Gemini provider never initialized → pipeline reports DEGRADED every health tick. Cosmetic; unresolved in steady state. |
| ERROR | `GeminiClient WiFi not connected` / `Error 1` | Init ran before STA connected; Gemini disabled anyway. Resolved as non-issue (offline mode). |
| ERROR | `OtaManager WiFi not connected` / `Error 1` | Same; OTA disabled in this build. |
| WARNING | `SarvamSTT Initialize failed (missing key/CA or no heap)` | STT unavailable → touch/mic-only mode (by design). |
| ERROR | `SarvamClient No root CA configured - TLS cannot be validated (failing closed)` ×4 | Startup greeting TTS synthesis attempted, failed closed, `SarvamTTS Synthesis failed (err=8)`. |
| INFO | `ErrorManager Loaded 1 events` | 1 persisted event from a previous boot. |
| INFO | `CrashManager Initialized (1 crashes logged)` | 1 persisted crash record (contents not readable without auth). |

**ErrorManager starts early (module #2, at 3.7 s) — early enough to capture startup failures.** No startup error remains fatal at READY; all failures degrade gracefully to offline/touch mode.

---

## Serial Flood

Steady-state output after READY (live 122 s capture):

- `TouchDiag` every **5 s** (rate-limited by `TOUCH_DIAG_INTERVAL_MS = 5000`) — **not** a flood.
- `ServiceManager AIPipeline health: 1` every **10 s**.
- `health diag: runs/logs` every **60 s**.
- Total ≈ **0.3–0.5 lines/second** steady state. No repeated Wi-Fi/reconnect warnings, no duplicate initialization.

The earlier ~49 `rst:ets` lines in a 14 s window were an **artifact of my own DTR/RTS assertion when opening the port** (CP210x auto-reset toggles the chip), not a device crash loop — confirmed because a clean 122 s capture with DTR/RTS held false showed **0 reboots**. The RMT `src/fl/...` lines are normal ESP32 core RMT debug, not a backtrace.

**AIPipeline flood fix is intact** (single warning per 10 s, no burst).

---

## Stability

- Observation: **122,857 ms (~2 min) continuous**, DTR/RTS held low.
- **Reboots: 0** | Reset reasons: none observed | Min uptime: 161,694 ms | Max uptime: 283,869 ms (monotonically increasing)
- **ERROR/CRITICAL lines: 0**
- Additional earlier 60 s observation: 0 reboots, uptime 88 s → 144 s continuously.
- WiFi reconnect count: 0 during observation (link never dropped).

**Caveat:** 2 minutes of idle observation is not enough to declare the device stable. The observed session was stable and free of Wi-Fi/app-induced restarts, but the previously reported restarts occurred under app interaction, which was **not** re-tested here.

---

## Findings

| ID | Severity | Finding | Evidence |
|---|---|---|---|
| F1 | **P1** | **Multiple Wi-Fi radio owners on flashed firmware:** EspNowManager forces `WiFi.mode(WIFI_AP_STA)` + `WiFi.channel(1)` during boot; ResilienceManager and SystemManager also drive reconnect directly. | Live boot log ("ESP-NOW initialized (channel 1)") + static audit (3 owners + WifiManager). |
| F2 | **P1** | **Live state inconsistency at boot:** banner says "AP Mode (AURA_Setup)" while `WiFi.localIP()` = <DEVICE_IP> and server is reachable — WifiManager state disagrees with the actual radio. Consistent with F1 (ESP-NOW rewrote the radio mode mid-STA-connect). | Captured banner + port probes (80/81 OPEN). |
| F3 | **P2** | ESP-NOW forced channel 1 can fight the router channel during STA connect on the flashed build. | HEAD `esp_now_manager.cpp` + live "channel 1" log. |
| F4 | **P2** | Heap at READY is only **48,088 B** (~200 KB consumed at boot). Low, but stable; not shown to cause a crash. | Live heap log. |
| F5 | **P2** | Reconnect amplifier: HEAD `monitorWiFi()` re-issues `wifiManager.reconnect()` every health tick with no interlock → can stack `WiFi.begin()` calls (documented ESP32 crash vector). | Static audit. |
| F6 | **P3** | SarvamSTT missing key/CA and SarvamClient TLS "no root CA" fail-closed on the startup greeting → 4 ERROR lines + greeting speech unavailable. Cosmetic, offline-by-design. | Live boot log. |
| F7 | **P3** | AIPipeline reports DEGRADED every 10 s (no Gemini). Cosmetic health noise. | Live capture. |
| F8 | INFO | 269 lifetime boots, 1 persisted crash, 1 persisted error event — boot-loop counter is 0/3 (no active boot loop). | Live uptime + CrashManager logs. |

---

## Root-Cause Confidence (previous ESP32 restart issue)

### MEDIUM — leaning HIGH for the Wi-Fi conflict hypothesis

**Why not HIGH/confirmed:**
- The reset-reason capture on the *live* device was **induced by the diagnostic itself** (power-on from my RTS reset), so no spontaneous restart was observed during this session.
- 2-minute idle stability does not prove immunity to app-induced restarts.

**Why MEDIUM (not LOW):**
- The multi-owner defect (F1/F5) is **verified present on the flashed firmware** and matches the previously identified failure mode (repeated radio re-initialization from `WiFi.mode()`/`begin()` is a documented ESP32 crash vector).
- The live state inconsistency (F2) shows the conflict occurring *during every boot* on this hardware.
- The unflashed working-tree fix removes all competing owners, which directly targets the leading hypothesis.

**UNCONFIRMED until:** a spontaneous restart is captured with its actual `esp_reset_reason()` (e.g. `PANIC`/`TASK_WDT`/`BROWNOUT`), ideally while the app is connected.

---

## Recommended Next Steps (recommendations only — not implemented)

1. **Flash the working-tree Wi-Fi single-ownership build** (already compiled, 0 errors/warnings) and re-run the live boot capture — verify the banner now shows a consistent STA state and ESP-NOW logs "follows WifiManager channel".
2. **Reconnect the app (Flutter companion)** and stress: connect/disconnect repeatedly, WebSocket open/close, ~10 min idle while connected; capture `rst:` + `esp_reset_reason()` on any restart to identify the true reset class.
3. **Add a one-shot serial log of the ROM header / `esp_reset_reason()` at the very first line of `setup()`** (it already logs reset reason via `.ino`; ensure serial capture starts before reset to record it).
4. **Consider raising boot heap headroom:** audit route strings / JSON reserve sizes; track `ESP.getMinFreeHeap()` from boot to READY (currently not captured).
5. **Resolve the AIPipeline DEGRADED + Sarvam TLS fail-closed noise** (disable greeting TTS when no root CA; treat Gemini-disabled as HEALTHY-with-no-provider) to remove recurring warnings.
6. After physical app stress passes, decide whether to commit the Wi-Fi fix; keep `config.h` touch profile unchanged (`TAP_MIN=60, TAP_MAX=450, DOUBLE_TAP=500, SETUP_HOLD=5000`).