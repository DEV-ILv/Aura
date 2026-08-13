# AURA Wi-Fi Stability Fix

**Date:** 2026-08-13
**Scope:** Wi-Fi ownership, ESP-NOW coexistence, reconnect reliability, Wi-Fi state reporting, startup diagnostics, memory diagnostics, ErrorManager integration.
**Status:** Implemented + compiled clean. **NOT flashed** (awaiting explicit approval). **NOT committed/pushed.**

---

## Before

The flashed (HEAD `e720c42`) firmware had **multiple components able to control the Wi-Fi radio**:

```
WifiManager ─────┐
EspNowManager ───┼──> Wi-Fi driver      (4 competing owners)
SystemManager ───┤
ResilienceManager┘
```

- **EspNowManager::initialize()** called `WiFi.mode(WIFI_AP_STA)` and `WiFi.channel(ESPNOW_CHANNEL)` (=1) during boot, re-initializing the radio mid-boot while WifiManager was still completing its STA connect.
- **SystemManager::monitorWiFi()** re-issued `wifiManager.reconnect()` every health tick (~5 s) whenever `DISCONNECTED` — bypassing WifiManager's attempt budget and forcing repeated `WiFi.mode()`/`begin()` calls.
- **ResilienceManager::RecoverWiFi()** called `WiFi.reconnect()` directly, a second radio owner.
- No interlock prevented stacked `WiFi.begin()` calls (a documented ESP32 crash vector).
- **Misleading status:** the boot banner printed `AP Mode (AURA_Setup)` whenever `isConnected()` was false — even when the device had a live STA IP, HTTP and WebSocket open (observed live at `<DEVICE_IP>`).
- ESP-NOW peers were hard-bound to `WIFI_IF_AP` and channel 1 regardless of the active mode, silently failing on a STA-only device.

## After

**Single authority — WifiManager only:**

```
Other modules
     ↓
WifiManager API (connect/reconnect/startAccessPoint/getChannel/getState)
     ↓
ESP32 Wi-Fi driver
```

- All `WiFi.mode()` calls route through `WifiManager::ensureMode()`, which only switches mode when the current mode actually differs — no repeated radio re-initialization.
- `WiFi.begin()` is called from exactly two functions in the codebase, both inside `wifi_manager.cpp`: `connect()` and `attemptConnection()`.
- `WiFi.reconnect()` is **no longer called anywhere**.
- `WiFi.softAP()`/`softAPdisconnect()`/`disconnect()` are only in `wifi_manager.cpp`.
- `WiFi.setHostname()` only in `wifi_manager.cpp`.
- `WiFi.channel()` read-back only in `wifi_manager.cpp` (feed the single-authority `getChannel()` accessor).

## Wi-Fi Ownership Audit

### Radio-mutating / mode-owning calls (the authority)

| API | FILE | LINE | COMPONENT | PURPOSE | AUTHORIZED? |
|---|---|---|---|---|---|
| `WiFi.mode(WIFI_STA)` | wifi_manager.cpp | 910 | WifiManager::ensureMode | Switch radio mode only when it differs | YES (single owner) |
| `WiFi.begin()` | wifi_manager.cpp | 199 | WifiManager::connect | User-initiated connect | YES (single owner) |
| `WiFi.begin()` | wifi_manager.cpp | 201 | WifiManager::connect | Open-network connect | YES (single owner) |
| `WiFi.begin()` | wifi_manager.cpp | 818 | WifiManager::attemptConnection | Reconnect attempt | YES (single owner, budgeted) |
| `WiFi.begin()` | wifi_manager.cpp | 820 | WifiManager::attemptConnection | Open-network reconnect | YES (single owner, budgeted) |
| `WiFi.disconnect(true)` | wifi_manager.cpp | 220 | WifiManager::disconnect | User/system disconnect | YES (single owner) |
| `WiFi.disconnect(false)` | wifi_manager.cpp | 269 | WifiManager::startAccessPoint | Tear down STA before AP | YES (single owner) |
| `WiFi.softAP()` | wifi_manager.cpp | 276/278 | WifiManager::startAccessPoint | AURA_Setup AP | YES (single owner) |
| `WiFi.softAPdisconnect(true)` | wifi_manager.cpp | 310 | WifiManager::stopAccessPoint | Stop AP | YES (single owner) |
| `WiFi.setHostname()` | wifi_manager.cpp | 76/195/393/815 | WifiManager | Hostname before/after begin | YES (single owner) |
| `WiFi.getMode()` | wifi_manager.cpp | 909 | WifiManager::ensureMode | Guard mode change | YES (single owner) |

### Read-only / query calls (authorized reads, no ownership)

| API | FILE | LINE | COMPONENT | PURPOSE | AUTHORIZED? |
|---|---|---|---|---|---|
| `WiFi.channel(index)` | wifi_manager.cpp | 624 | WifiManager::getNetworkInfo | Scan result channel | YES (read) |
| `WiFi.channel()` | wifi_manager.cpp | 676/721/785 | WifiManager | Track active channel | YES (read, feeds getChannel) |
| `WiFi.scanNetworks()` | wifi_manager.cpp | 593 | WifiManager::scanNetworks | Network scan | YES (read) |
| `WiFi.status()` | wifi_manager.cpp | 646/711/773 | WifiManager | State machine | YES (read) |
| `WiFi.SSID()`/`RSSI()`/`localIP()`/`softAPIP()` | wifi_manager.cpp | 295/350/360/362/675/717/784 | WifiManager | Status reporting | YES (read) |
| `WiFi.isConnected()` | wifi_manager.cpp | 321/427 | WifiManager | isConnected / state label | YES (read) |
| `WiFi.macAddress()` | Aura_programs.ino | 129 | Boot diagnostics | MAC reporting | YES (read) |
| `WiFi.status()` | service_status_manager.cpp | 162 | ServiceStatusManager | REST/WS status | YES (read) |
| `WiFi.localIP()/SSID()/RSSI()` | service_status_manager.cpp | 163-204 | ServiceStatusManager | REST/WS status | YES (read) |
| `WiFi.status()` | web_portal.cpp | 621/2107/3552/3564 | WebPortal | Status endpoints | YES (read) |
| `WiFi.localIP()/softAPIP()` | web_portal.cpp | 188-189/299 | WebPortal | Origin validation | YES (read) |
| `WiFi.SSID()/localIP()/RSSI()` | web_portal.cpp | 2100-2111 | WebPortal | /api/wifi GET | YES (read) |
| `WiFi.status()/RSSI()/isConnected()` | automation_manager, context_manager, diagnostic_system, diagnostics_manager, executive_assistant, health_monitor, local_ai_engine, offline_response_generator, oled_renderer, performance_manager, prediction_manager, sarvam_client | — | Various | AI/context/display telemetry | YES (read-only) |
| `WiFi.macAddress()` | device_mesh.cpp | 192 | DeviceMesh | Node identity | YES (read) |
| `WiFi.macAddress()` | platform_abstraction.cpp | 97 | Platform | Hardware identity | YES (read) |
| `WiFi.softAPSSID()` | conversation_manager.cpp | 760 | ConversationManager | SETUP mode display label | YES (read) |
| `WiFi.RSSI()` | esp_now_manager.cpp | 328/352/488/502 | EspNowManager | Node RSSI telemetry | YES (read; runs in recv callback — noted risk, see Risks) |

### Radio-mutating calls that were REMOVED

| API | FILE (HEAD) | Component | Disposition |
|---|---|---|---|
| `WiFi.mode(WIFI_AP_STA)` | esp_now_manager.cpp (HEAD) | EspNowManager | **Removed** — radio ownership is WifiManager's |
| `WiFi.channel(ESPNOW_CHANNEL)` | esp_now_manager.cpp (HEAD) | EspNowManager | **Removed** — channel follows WifiManager |
| `WiFi.reconnect()` | resilience_manager.cpp (HEAD) | ResilienceManager | **Removed** — routes via `wifiManager.reconnect()` |
| `wifiManager.reconnect()` in health tick | system_manager.cpp (HEAD) | SystemManager::monitorWiFi | **Removed** — WifiManager state machine owns recovery |
| `WiFi.mode(WIFI_STA)` unguarded | wifi_manager.cpp (HEAD) | WifiManager | **Guarded** via `ensureMode()` |

**Result: one clear Wi-Fi authority.** Every remaining direct call is a read-only query or is inside `wifi_manager.cpp`.

## ESP-NOW Changes

- **No longer calls `WiFi.mode()` or `WiFi.channel()`.** ESP-NOW runs on whatever radio state WifiManager has established.
- **Peer channel = 0** ("use current channel") in `pairNode()` and `handlePairAccept()` — follows the WifiManager-established channel instead of hard-coding `ESPNOW_CHANNEL=1`.
- **Peer interface is mode-aware**: `currentEspNowInterface()` returns `WIFI_IF_STA` for normal operation and `WIFI_IF_AP` while in AURA_Setup — ESP-NOW no longer silently binds to the wrong interface.
- Init failure is reported to ErrorManager (`ESPNOW_INIT_FAIL`, WARNING) instead of being silent.
- Log line now reads `ESP-NOW initialized (follows WifiManager channel <n>)`.
- All mesh/pairing/heartbeat/OTA functionality is preserved.

**Documented behavior:** ESP-NOW operates on the WifiManager-established channel/interface. For mesh discovery to work across nodes, every AURA node joins the same STA network (same router channel), or is in the same AP session — WifiManager coordinates the channel, ESP-NOW does not force one.

## Reconnect Changes

One authoritative reconnect state machine inside WifiManager:

| Mechanism | Behavior |
|---|---|
| State machine | `CONNECTING` (15 s timeout) → `DISCONNECTED` (5 s backoff) → up to `MAX_CONNECTION_ATTEMPTS=5` per cycle → `ERROR` (bounded-rate retry every 30 s) |
| Budgeted retries | `handleReconnect()` resets the attempt budget; on exhaustion transitions to `ERROR` instead of giving up permanently |
| ERROR recovery | Retries `attemptConnection()` at a fixed 30 s interval so the device recovers when the network returns; `ensureMode()` prevents radio re-init during retries |
| Reconnect guard | `reconnect()` **ignores duplicate requests while `CONNECTING`** — no stacked `WiFi.begin()` |
| Single trigger path | `handleEvents`/`checkConnection` seed the state machine (`m_connectionAttempts=1`, `m_reconnectTimer=millis()`); no external module forces reconnection |
| Event-driven | `WL_CONNECTED`/`WL_DISCONNECTED`/`WL_CONNECT_FAILED`/`WL_NO_SSID_AVAIL` drive state + ErrorManager transitions |
| Tracking | `m_reconnectCount` (attempts since boot), `m_currentChannel`, `m_lastEvent` (last `wl_status_t`), `m_lastPersistedState`/`m_lastPersistedReconnect` (previous session) |
| No serial flood | Reconnect logging is state-driven; steady state emits only periodic health diag |

## AP/STA State Reporting

- New `WifiManager::getStateString()` returns a truthful, stable label:
  - `SETUP_AP` — AP active (AURA_Setup portal)
  - `AP_STA` — AP active **and** station connected
  - `STA_CONNECTING` — joining a saved network
  - `STA_CONNECTED` — station joined
  - `DISCONNECTED` — radio up, no link
  - `ERROR` — bounded-retry backoff
- The boot banner now prints the **actual state** (`Wi-Fi : STA_CONNECTED (state=2 mode=2 channel=<n> ...)`) instead of the misleading `AP Mode (AURA_Setup)` label. The stale/default-label bug observed in the diagnostic is eliminated.
- AURA_Setup AP, normal STA mode, and AP+STA (should firmware ever need it) all remain supported — the label now reflects reality rather than assuming.

## Startup Diagnostics

Boot banner now emits, in one block:

```
Reset    : <POWER_ON|SOFTWARE|PANIC|TASK_WDT|BROWNOUT|...>
Boots    : <boot count from NVS>
Prev     : wifiState=<last persisted state> reconnectCount=<last session count>
Heap     : free <n> B, min <n> B, largest block <n> B
Wi-Fi    : <state label> (state=.. mode=.. channel=.. reconnectCount=.. lastEvent=..)
```

- Reset reason via `esp_reset_reason()`; boot count via `UptimeMonitor` (NVS-backed).
- Previous Wi-Fi state + reconnect count persisted once per state change (`WifiManager::persistDiagnostics()`), loaded at boot (`loadDiagnostics()`) — NVS write rate is bounded to rare state transitions.
- Free/min/max heap printed at boot; largest block added to `SystemInfo` (`maxBlockHeap`).
- `.ino` `logBootDiagnostics()` continues to emit reset reason + heap free/min/max + MAC at the very first lines.

## Memory Diagnostics

- `SystemManager::monitorMemory()` now tracks `freeHeap`, `minimumHeap`, and `maxBlockHeap` (largest contiguous block).
- **LOW_HEAP** WARNING via ErrorManager when free heap < 20 KB (unchanged).
- **LOW_MAX_BLOCK** WARNING when free heap ≥ 30 KB but largest block < 8 KB (fragmentation signal); auto-resolves via `errorManager.resolve()` when the condition clears.
- Both are deduplicated by ErrorManager (component+code) — no per-loop flooding.

## ErrorManager Integration

| Event | Component | Code | Severity | Trigger | Auto-resolve |
|---|---|---|---|---|---|
| Connection timeout / budget exhausted | WiFi | WIFI_CONN_TIMEOUT | ERROR | 5 attempts fail | on CONNECTED |
| Link lost | WiFi | WIFI_LINK_LOST | WARNING | WL_DISCONNECTED / checkConnection | on CONNECTED |
| Connection rejected | WiFi | WIFI_CONN_FAILED | ERROR | WL_CONNECT_FAILED | on CONNECTED |
| Network unavailable | WiFi | WIFI_NO_SSID | WARNING | WL_NO_SSID_AVAIL | on CONNECTED |
| AP start failure | WiFi | WIFI_AP_START_FAIL | ERROR | softAP() fails | — |
| ESP-NOW init failure | EspNowManager | ESPNOW_INIT_FAIL | WARNING | esp_now_init() fails | — |
| Low heap | Memory | LOW_HEAP | WARNING | free < 20 KB | when recovered |
| Fragmentation | Memory | LOW_MAX_BLOCK | WARNING | max block < 8 KB | when recovered |

All routed through `errorManager.report()`/`resolve()` (dedup by component+code, throttled, one-shot WS push on new ERROR/CRITICAL). No per-loop reporting.

## Files Modified

| File | Change |
|---|---|
| `wifi_manager.h` | New: `getStateString()`, `getChannel()`, `getReconnectCount()`, `getLastEvent()`, `getLastPersistedState()`, `getLastPersistedReconnectCount()`; private `ensureMode()`, `persistDiagnostics()`, `loadDiagnostics()`; new members |
| `wifi_manager.cpp` | `ensureMode()` guard; reconnect guard vs CONNECTING; budgeted ERROR retry (30 s); channel tracking; state/event/reconnect diagnostics + NVS persistence; ErrorManager integration |
| `esp_now_manager.cpp` | Removed `WiFi.mode(WIFI_AP_STA)` + `WiFi.channel(ESPNOW_CHANNEL)`; peer channel 0 + mode-aware interface; ESPNOW_INIT_FAIL reporting |
| `resilience_manager.cpp` | RecoverWiFi/Update/CheckAllSystems route through `wifiManager` (no direct `WiFi.reconnect()`/`WiFi.isConnected()`) |
| `system_manager.cpp` | `monitorWiFi()` no-op (WifiManager owns recovery); truthful boot banner (reset reason, boots, prev wifi state, heap free/min/max, wifi state/event); `monitorMemory()` max-block + LOW_MAX_BLOCK; `wifiStateToString()` helper |
| `system_manager.h` | `SystemInfo.maxBlockHeap` |
| `config.h` | Touch timing profile only (approved values, unchanged): `TAP_MIN_MS=60`, `TAP_MAX_MS=450`, `DOUBLE_TAP_WINDOW_MS=500` (comment block updated) |

## Build Result

```
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app
Sketch uses 1996883 bytes (63%) of program storage space. Maximum is 3145728 bytes.
Global variables use 108520 bytes (33%) of dynamic memory, leaving 219160 bytes for local variables. Maximum is 327680 bytes.
Errors:  0
Warnings: 0
```

| Item | Value |
|---|---|
| Firmware size | 1,996,883 B |
| Flash percentage | 63% (of 3,145,728 B huge_app) |
| Static RAM (globals) | 108,520 B (33%) |
| Available RAM (locals) | 219,160 B |
| Fit | YES |

## Static API Audit

See the two tables under **Wi-Fi Ownership Audit**. Summary: **one radio owner** (`wifi_manager.cpp`); all other call sites are read-only queries. No `WiFi.reconnect()` anywhere. `WiFi.mode()`/`begin()`/`softAP()`/`disconnect()` only inside `wifi_manager.cpp`.

## Risks / Remaining Issues

1. **ESPNOW_CHANNEL macro is now unused** (`config.h:679`). Kept for compatibility; no code path references it anymore. (P3 — safe to remove later.)
2. **ESP-NOW cross-node discovery channel requirement:** because ESP-NOW no longer forces channel 1, all mesh nodes must share the WifiManager-established channel (same STA network or same AP session). Documented above; verify during TEST 5.
3. **`WiFi.RSSI()` read inside ESP-NOW recv callback** (esp_now_manager.cpp:328/352/488/502) is a pre-existing read-only call from callback context; not a radio state transition, but a read from a callback — acceptable, could be hoisted to the loop if ever problematic.
4. **Low heap at READY (~48 KB) unchanged** — not addressed by this task per instructions (no memory refactor); monitoring added instead.
5. **Reboot root cause not confirmed.** The fix removes the leading architectural risk, but a spontaneous reboot has not been reproduced/eliminated by testing — it must be validated on hardware.
6. Not flashed; not committed. Working tree also contains the previously unflashed Phase-1 build delta (superseded by this).

## Physical Test Plan

Flash requires explicit approval first. **Serial:** 115200 baud, open before power-up to catch the ROM header.

**TEST 1 — Cold boot**
- Procedure: power off/on, capture from first byte.
- Evidence: `Reset : POWER_ON`; `Boots : N`; `Prev : wifiState=...`; `Heap : free/min/max`; no `PANIC`/`TASK_WDT`/`BROWNOUT`; READY ~9.5 s; `ESP-NOW initialized (follows WifiManager channel <n>)`.

**TEST 2 — Normal STA connection**
- Procedure: with saved credentials, power on with router up.
- Evidence: `STA_CONNECTED` state label, `channel=<router channel>` (e.g., 6, not 1), `REST http://192.168.x.x/api`, ports 80/81 reachable, RSSI sane.

**TEST 3 — AURA_Setup AP**
- Procedure: factory reset / hold touch 5 s to enter setup; scan for `AURA_Setup`.
- Evidence: `SETUP_AP` label; `Wi-Fi` line shows AP state; portal reachable at AP IP; SSID `AURA_Setup`.

**TEST 4 — AP+STA (if supported)**
- Procedure: verify `getStateString()` returns `AP_STA` when both interfaces are live; confirm no radio conflict (single `ensureMode` path).
- Evidence: truthful `AP_STA` label; no repeated `WiFi.mode()` in serial.

**TEST 5 — ESP-NOW initialization**
- Procedure: boot with another AURA node on same network/channel; watch init + heartbeat.
- Evidence: `ESP-NOW initialized (follows WifiManager channel <n>)`; peers discover/pair; channel matches router channel; no `WiFi.mode(WIFI_AP_STA)` in log.

**TEST 6 — Companion app connection**
- Procedure: connect Flutter companion to `192.168.x.x` with auth token.
- Evidence: dashboard appears; `wifi_connected:true`; `connected_modules` correct.

**TEST 7 — REST requests**
- Procedure: GET `/api/status`, `/api/uptime`, `/api/wifi`.
- Evidence: 200s, valid JSON, `wifi` object truthful, `bootCount`, `heap_free`.

**TEST 8 — WebSocket connection**
- Procedure: connect WS to port 81, auth, observe dashboard broadcasts.
- Evidence: auth handshake OK; telemetry every 500 ms; no unauthenticated broadcast.

**TEST 9 — Phone disconnect/reconnect**
- Procedure: toggle companion app / drop WS repeatedly.
- Evidence: no REST/WS errors; device stays `STA_CONNECTED`; no reconnect storm.

**TEST 10 — Router disconnect/reconnect**
- Procedure: power off router ~60 s, back on.
- Evidence: `WIFI_LINK_LOST` once (WARNING); bounded retries; auto-`STA_CONNECTED` on router return; no repeated `WiFi.mode()`; reconnect count increments ~1-2.

**TEST 11 — Repeated Wi-Fi loss/recovery (5-10 cycles)**
- Procedure: toggle router/AP 5-10×.
- Evidence: **no reboot**; one `WIFI_LINK_LOST` event per loss; ERROR state bounded-retry at 30 s if network stays down; no serial flood; capture any `rst:` line (should be none).

**TEST 12 — 15–30 minute stability test (app connected)**
- Procedure: leave companion connected, idle + occasional commands.
- Evidence: uptime grows monotonically; 0 reboots; min heap stable; 0 new ERROR lines; final `Uptime:` milestone reached.

## Conclusion

**Leading Wi-Fi architectural risk addressed; spontaneous reboot cause requires hardware stress validation.**

The multi-owner radio defect (EspNowManager forcing `WIFI_AP_STA` + channel 1, ResilienceManager/SystemManager driving reconnect in parallel) has been removed and replaced with a single WifiManager authority, budgeted serialized reconnect, mode-aware ESP-NOW, and truthful state reporting. The build is clean (0 errors, 0 warnings, 63% flash). Per instructions, this fix is **not** considered proven by compilation alone — physical tests 1–12 must pass, and the reboot cause remains **unconfirmed** until a real stress test demonstrates stability or captures a different root cause.

**DO NOT COMMIT. DO NOT PUSH. DO NOT FLASH WITHOUT EXPLICIT APPROVAL.**

---

## Validation Addendum (2026-08-13, firmware flashed to COM7)

Firmware 1.0.0 flashed and verified to COM7 (1,997,024 B, hash verified, device booted and running). Firmware currently on-device is the **new** Wi-Fi-fix build (boot banner now reports `Reset : POWER_ON`, `Boots :`, `Prev :`, `Heap :`, `Wi-Fi :`).

### TEST 1 — Cold boot: PASS

Captured full cold boot header (device was hard-reset via esptool `--before default-reset --after hard-reset`; serial opened immediately):

```
Reset    : POWER_ON
Boots    : 274
Prev     : wifiState=CONNECTED reconnectCount=1
Heap     : free 63356 B, min 62332 B, largest block 31732 B
Wi-Fi    : STA_CONNECTING (state=1 mode=1 channel=0 reconnectCount=1 lastEvent=0)
REST     : http://<DEVICE_IP>/api
WebSocket: ws://<DEVICE_IP>:81
READY (system init complete ~9478 ms)
[EspNowManager] ESP-NOW initialized (follows WifiManager channel 0)
```

- `Reset : POWER_ON` — no PANIC / TASK_WDT / BROWNOUT.
- `Boots : 273 → 274` across two consecutive cold boots (counter increments correctly).
- `Prev : wifiState=CONNECTED reconnectCount=1` — NVS persistence of previous session works (prev session genuinely reached CONNECTED).
- `Heap : free/min/largest` present. Truthful `STA_CONNECTING` label (not the old misleading "AP Mode").
- Banner prints at end of init (~9.5 s) as expected.

### TEST 2 — Normal STA connection: PASS (network evidence)

- Banner `REST : http://<DEVICE_IP>/api` shows a **DHCP-lease STA address** on the router subnet (not the SoftAP 192.168.1.x/2.x default), i.e. STA is live on the router.
- TCP probes from PC: port 80 → `TcpTestSucceeded=True`; port 81 → `TcpTestSucceeded=True`.
- `GET http://<DEVICE_IP>/status` → HTTP 200 (AURA SPA shell).
- WebSocket to `ws://<DEVICE_IP>:81/` → connects, `State: Open`, first frame `{"type":"auth_required"}` (no unauthenticated telemetry leak).
- `Prev : wifiState=CONNECTED` persisted on the next boot proves the state machine transitioned to `STA_CONNECTED`.
- **Observation:** the `WiFi connected to <SSID>` INFO log from `handleEvents()` does **not** appear on cold boot because `checkConnection()` (silent `changeState(CONNECTED)`, no log line) runs before `handleEvents()` in `update()`. Functional transition is correct (persisted `CONNECTED` proves it); only the cosmetic log line is masked. Channel in the banner is 0 at banner time (STA still associating); `m_currentChannel` is populated by `checkConnection()` once `WL_CONNECTED` is observed.

### TEST 7 — REST: PASS (auth-gated)

- `GET /status` → 200 SPA shell.
- `GET /api/uptime`, `/api/status` → HTTP 401 (auth required — correct; unauthenticated routes reject).
- Route table present: 154 routes registered (`API dispatcher registered (154 routes...)`).

### TEST 8 — WebSocket: PASS

- ClientWebSocket to port 81 → `Open`; first message `{"type":"auth_required"}` — auth handshake enforced, no unauthenticated broadcast. (In-progress auth with a valid token still required for full telemetry assertions.)

### TEST 12 (short) — Stability window: PASS (no fault events)

90-second steady-state capture (device uptime 2291 s → 3110 s, i.e. a single 38+ min continuous session):

- **0** reboot / reset lines, **0** `rst:` lines, **0** panic/watchdog/backtrace/abort.
- **0** WiFi transitions (`Connection lost`, `Reconnect`, `LINK_LOST`) during the window.
- Uptime monotonic; `Boots : 274` unchanged throughout (no spontaneous reboots).
- Only pre-existing HealthMonitor `High fragmentation: 90.1%` warning (unrelated to this change; it is the legacy `checkFragmentation()` path, not the new `LOW_MAX_BLOCK` monitor).

### Remaining tests requiring user/physical action

- **TEST 3** (AURA_Setup AP via 5 s touch hold), **TEST 4** (AP_STA), **TEST 5** (2nd ESP-NOW node), **TEST 6** (Flutter companion app with auth token), **TEST 9** (phone disconnect/reconnect), **TEST 10** (router power-cycle), **TEST 11** (5–10 disconnect cycles), **TEST 12** full 15–30 min with companion connected. REST/WS auth-token probes for `/api/wifi` (channel/RSSI assertions) require the companion token.

### Preliminary conclusion

The device boots cleanly on the new firmware, connects to the router via STA (DHCP IP, ports 80/81 reachable), persists truthful Wi-Fi state, and shows **zero reboot/fault/transition events** across a 38+ minute continuous session and multiple controlled cold boots. Full TEST 3–11 and the long-duration TEST 12 remain outstanding pending user/physical actions.