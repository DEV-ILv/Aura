# Changelog

All notable changes to AURA Companion are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-08-06

### Added
- **V2 Device Control centre** — new `DeviceControlScreen` (Tools → "Control")
  with sections for Display, LED Ring, Speaker, Microphone, Network (Wi-Fi) and
  device actions, backed by new firmware endpoints (`/api/display/control`,
  `/api/led/control`, `/api/audio/control`, `/api/mic/control`, `/api/mic/level`,
  `/api/wifi/scan`, `/api/wifi/forget`).
- Declarative smoke test `test/device_control_test.dart` (page renders the
  local-only banner without exceptions).
- **LED idle power policy (firmware)** — the aura ring is now completely OFF
  while the device is idle (`IDLE`/`SLEEP`/`OFFLINE`); it is only lit for
  meaningful events and returns to OFF when they complete. Manual LED control
  from the Device Control page opens a temporary, event-driven session that
  auto-expires (60 s) back to automatic behaviour. See
  `Aura_programs/docs/aura-led-state-machine.md`.

### Changed
- App renamed to **A.U.R.A** across all user-facing branding
  (`AndroidManifest`, app title, shell/app-bar, splash, connection, settings).
  Device identity "AURA" is preserved where it refers to the physical device.
- Use Material 3 light theme (`AppTheme.light()`) as the default theme, dark
  theme retained.
- **Release hardening (Android):** R8 shrinking + resource shrinking enabled
  (`isMinifyEnabled`, `isShrinkResources`) with a new `proguard-rules.pro`.
- Debug logging gated behind `kDebugMode` so release builds emit no
  `print`-based log output (`Logger`).
- Version bumped to `1.1.0+2`.

### Fixed
- Riverpod provider disposal errors surfaced by the new widget test: the
  Device Control screen no longer uses `ref` after dispose, and
  `DeviceControlProvider`/`ConnectionNotifier` no longer register redundant
  `onDispose` / tear-down reads that could touch a disposed container.

## [Unreleased]

### Added
- **Supabase integration** (`supabase_flutter`) for remote (cloud) access.
  - Initialised during app startup with only the **publishable (anon) key**;
    the service role key is never used or exposed.
  - Email/password **sign in, sign up, forgot password, logout, and persisted
    sessions** via Supabase Auth (tokens in `flutter_secure_storage`).
  - **Connection modes** — `local` (LAN REST + WebSocket) and `remote`
    (Supabase) with automatic switching: device unreachable falls back to the
    cloud when a session exists; repeated local failures switch automatically.
  - Mode-aware login screen: local device sign-in and AURA Cloud sign-in
    (Sign Up / Forgot Password) with a "Continue with AURA Cloud" path from
    the device-unavailable panel.
  - Remote dashboard showing the user's cloud-registered devices with
    online/offline state.
  - Settings additions: **Connection** tile (mode + manual switch) and
    **Account** tile (cloud email + cloud sign-in/out).
  - `CloudRepository` — RLS-scoped PostgREST data access for `users`,
    `devices`, `commands`, `reminders`, `memory`, `notifications`, `settings`.
  - SQL migration `supabase/migrations/20260803000001_init_schema.sql` —
    schema for all seven tables, **RLS enabled on every table**, per-user
    policies (`auth.uid() = user_id`), `updated_at` triggers, and an
    auto-profile trigger for `users`.
  - Documentation: `docs/supabase.md` (schema, auth flow, local-vs-remote
    architecture, adding devices, running migrations, security notes).

### Changed
- Startup flow: after the local probe, an existing cloud session routes
  automatically to the remote dashboard.
- Logout now also signs the user out of Supabase.
- `pubspec.yaml` — added `supabase_flutter`.

### Security
- Only the publishable (anon) key ships in the app; the service role key is
  documented as server-side-only.
- RLS policies guarantee each authenticated user can only access their own rows.
- PostgREST role grants added to the schema migration (plus incremental
  `supabase/migrations/20260803000002_grant_table_access.sql`): without them
  PostgREST returns `42501 permission denied` even with RLS enabled.
- `secrets.h` is now git-ignored (committed template: `secrets.h.example`);
  the firmware `.gitignore` also excludes signing keys (`keys/`, `*.pem`).

### Fixed
- Startup no longer hangs on a malformed device payload — `ApiService._decode`
  raises a typed `ApiException` for non-map bodies and splash bootstrapping is
  guarded.
- WebSocket state subscription is cancelled on dispose (provider lifecycle
  leak).
- Reconnect timer and `_connect` are mutual-exclusion-safe and swallow async
  errors instead of crashing the zone.
- Cloud-only sign-out (Settings → Account) routes through the connection
  provider so the UI no longer stays stuck in remote mode after sign-out.
- Logout is wrapped in `try/finally` so state always resets and navigation
  always runs.
- Memory/Reminder/SD/OTA providers and the Voice/Notification services no
  longer throw unhandled exceptions when the device is unreachable; loading
  states always settle.
- System-monitor polling is gated on an active local connection so it cannot
  flip the app into remote mode while browsing other screens.

### Added
- **Secure session login.** Login screen shows only AURA branding, username, password, Remember Me, Forgot Password, and Login. Credentials are never stored; the session token, refresh token, and username are persisted via `flutter_secure_storage`.
- **Hidden developer settings** (`Settings → About → tap version 5 times`): host, REST port, WebSocket port, reconnect delay, logging toggle, diagnostics (`/api/developer`), log export (`/api/developer/export`), sign out. Networking details are intentionally hidden from normal users.
- **Connection lifecycle** in `ConnectionNotifier`: startup probe (`GET /api/ping`), stored-token validation (`GET /api/auth/status`), explicit login/logout/retry, exponential reconnect backoff driven by `reconnectDelayMs`.
- **Device-unavailable panel** on the connection screen: "AURA device unavailable." with Retry and Advanced Settings.
- **Real firmware identity on the dashboard** — device card shows model label (e.g. "AURA V1 Prototype — III Phoenix"), version, mark, codename, channel, chip, WiFi SSID/signal, and last-seen time.
- **Memory pin** action (`POST /api/memory/pin`).
- **Proactive device alerts** (toggleable in Settings → Notifications): low battery, low free heap, OTA/connection-loss notifications.
- **New settings:** WebSocket port, TTS rate/pitch sliders, reminder + device-alert toggles, reconnect delay.
- **OTA wiring** now uploads to the real firmware endpoint `POST /ota` (multipart field `firmware`) and reads the current version from `GET /api/version`.
- **Legacy plaintext auth purge** — old username/token prefs are removed when settings load.

### Changed
- Connection screen replaced IP/port fields with a pure branded login + unavailable panel.
- Login form no longer prefills any default credentials. The firmware generates a
  unique admin password on first boot and prints it to the Serial monitor; returning
  users keep their remembered username with a blank password.
- `device_repository.loadDashboard()` merges `/api/version` identity into the device model.
- Memory restore now sends `revisionId` as a form argument (matches firmware).
- Settings screen reorganized: General, Voice, Notifications, Appearance, Status, Connection, Account, About, Session.

### Fixed
- OTA upload path corrected from `/api/ota/upload` to the firmware's real `/ota`.

## [1.0.0] - Initial release

### Added
- Flutter app scaffolding, AURA design system (dark glassmorphism, electric blue/cyan accents, glow, micro animations, skeleton loading, page transitions).
- Riverpod architecture: providers, repositories, `ApiClient`/`ApiService`, `WebSocketService` (REST port 80, WebSocket port 81).
- Screens: connection, splash, dashboard, chat, voice, memory, reminders, OTA, storage explorer, settings.
- Chat with the on-device assistant; voice dictation and TTS playback; memory search/filter; timezone-aware local reminders; firmware OTA with progress; SD file browser with upload/delete.
- Device metrics: CPU, RAM, storage, battery, temperature, WiFi, uptime, live WebSocket feed merging.
- Testing scaffold (`flutter analyze` clean, `flutter test` green).
