# AURA Companion V2 — Device Control Centre (Report)

Status: **implemented, compiled and verified** on both sides.

- Firmware: new device-control API compiled clean (`huge_app`, 1,958,683 B / 62%).
- Companion: `flutter analyze` 0 issues, `flutter test` 11/11,
  `flutter build apk --release` OK (58.6 MB), `flutter build windows` OK
  (`aura_companion.exe`).

## New features (Companion app)

A new **Device Control** screen was added to the Tools hub
(`lib/screens/device_control/device_control_screen.dart`), organised into
sections:

1. **System** — device name + firmware version, live uptime, **Restart** and
   **Factory reset** (both behind confirmation dialogs).
2. **Network** — current SSID / IP / signal (reuses status), **Scan networks**
   (tap a result to connect), **Forget Wi-Fi** (confirmation).
3. **OLED Display** — power toggle, brightness slider, invert toggle,
   rotation selector, and "show text on screen".
4. **LED Ring** — enable toggle, brightness, mood selector (17 moods), and a
   theme-colour palette.
5. **Speaker** — volume slider, mute toggle, "play test tone".
6. **Microphone** — live input level meter (polled), gain slider,
   "recalibrate noise floor".

All controls are **gated to Local Mode** (direct LAN REST). In Cloud (Supabase)
mode the page shows an informational banner and status-only content, so both
modes remain fully compatible. Controls degrade gracefully when the device is
offline.

## REST endpoints used

### Existing (reused, not duplicated)

| Endpoint | Use |
|---|---|
| `GET /api/status` | uptime, connectivity |
| `GET /api/settings` | device identity |
| `GET /api/version` | firmware mark/version/channel |
| `GET /api/wifi` | current SSID / IP / signal |
| `POST /api/wifi` | connect to a scanned network (credential handling stays in firmware) |
| `POST /restart` | device restart |
| `POST /factory-reset` | factory reset |
| `GET /api/auth/status` | session / reachability |

### New (firmware, added this pass — hardware had no existing surface)

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/wifi/scan` | POST | scan nearby networks via `WifiManager::scanNetworks` + `getNetworkInfo` |
| `/api/wifi/forget` | POST | clear saved credentials via `WifiManager::clearCredentials` |
| `/api/display/control` | GET/POST | OLED power, brightness, invert, rotation, timeout, text (via `DisplayManager` + `SettingsManager`) |
| `/api/led/control` | GET/POST | ring enable, brightness, mood, theme colour (via `LedRing` + `AuraSystem` + `SettingsManager`) |
| `/api/audio/control` | POST | volume, mute/unmute, output-to-speaker, test earcon (via `AudioManager`) |
| `/api/mic/control` | POST | mic gain, noise-floor calibration (via `AudioManager` + `SettingsManager`) |
| `/api/mic/level` | GET | live energy/peak/noise-floor/recording sample |

All new handlers follow the existing `web_portal.cpp` conventions: auth via
`isAuthenticatedOrReject()`, JSON parse via `JsonDocument`, responses via
`sendJson` / `sendSuccess` / `sendError`. Persisted settings go through the
existing `SettingsManager` (NVS) setters + `save()`; runtime hardware effects
go through the existing manager instances (`displayManager`, `ledRing`,
`auraSystem`, `audioManager`, `wifiManager`).

## New firmware files / changes

- `web_portal.cpp` — 7 new handler implementations + route registration +
  `moodToName` / `nameToMood` / `escapeJsonString` helpers.
- `web_portal.h` — handler declarations.
- No new managers were created; everything reuses existing modules.

## Remaining TODOs (deferred, per scoped decision)

- **Touch** control — no dedicated touch manager endpoint yet; input events
  are posted via `input_manager` but no touch-state REST surface exists.
- **Reminders CRUD** (create/update/delete) — firmware only bootstraps via
  the existing path; no full CRUD surface yet.
- **Memory ranked / semantic** — `GET /api/memories/ranked` exists; app-side
  ranked-memory screen still to be wired.
- **SD card / storage** — endpoints exist but the hardware SD card is faulty
  (`SD.begin()` fails, `[SD-DIAG] CS probe = HIGH`); UI shows error states
  until the card is replaced/reseated.
- **OTA JSON state** (`/api/ota/*`) — firmware only exposes the browser
  multipart upload at `/ota`; no JSON progress/state API yet.
- **WebSocket live control events** — control changes currently use REST
  request/response only; no WS push events were added.
- Live REST testing requires routing the PC onto the AURA network
  (192.168.4.1) once the firmware is flashed.
