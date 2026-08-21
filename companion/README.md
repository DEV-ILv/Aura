# AURA Companion

The official companion app for the **AURA Personal AI Assistant** — an ESP32-powered personal AI device. AURA Companion talks to the AURA firmware's REST + WebSocket surface to deliver chat, voice, memories, reminders, device monitoring, and firmware updates from a dark, glassmorphic Flutter UI.

Supported platforms: **Android** and **Windows**.

## Features

- **Secure session login** — username + password, token session kept in `flutter_secure_storage` (never plaintext, never a stored password).
- **Automatic connection** — probes the device on startup, validates the stored session, and routes straight to the dashboard when possible.
- **Remote access via Supabase** — when the device is out of range, sign in with email/password (Sign Up + Forgot Password) and manage your AURA devices through the cloud. Automatic local → cloud switching.
- **Live dashboard** — CPU / RAM / storage / battery gauges, WiFi signal, uptime, temperature, and real firmware identity (mark, codename, channel).
- **Chat** — text chat with the on-device assistant (memory-aware).
- **Voice** — push-to-talk dictation (`speech_to_text`) and speech playback (`flutter_tts`) with rate/pitch controls.
- **Memory browser** — ranked list, full-text search, category filters, pin-to-top, archive restore.
- **Reminders** — timezone-aware local notifications.
- **OTA updates** — pick a firmware `.bin` and upload with live progress.
- **SD / storage explorer** — browse, upload, and delete files on the device SD card.
- **Proactive alerts** — low-battery, low-heap, and connection-loss notifications (toggleable).
- **Hidden developer settings** — all networking details, reconnect tuning, diagnostics, and log export are deliberately kept behind a developer unlock.

> Supabase documentation: [docs/supabase.md](docs/supabase.md) — schema, RLS,
> auth flow, local-vs-remote architecture, and how to add additional devices.

## Getting started

### Prerequisites

- Flutter SDK `^3.12.2` (Dart `^3.12`)
- Android SDK Platform 34 (for Android builds)
- A device running the AURA firmware (or a simulator for UI work)

### Run

```sh
flutter pub get
flutter run          # or: flutter run -d windows
```

### Build

```sh
flutter build apk          # Android release
flutter build apk --debug  # Android debug (avoids release AOT; see below)
flutter build windows      # Windows release
```

> **Windows App Control note:** if `flutter build apk` (release) fails with `An Application Control policy has blocked this file ... gen_snapshot.EXE`, a machine-level policy is blocking Flutter's AOT compiler — this is not a project issue. Use `flutter build apk --debug` (JIT) or clear the policy.

### Validate

```sh
flutter analyze
flutter test
```

## Configuration & Secrets

The app **never stores credentials in source code**. All build-time values are
injected with `--dart-define`; runtime secrets (device session token, username,
cloud session) live in `flutter_secure_storage`, never in plaintext.

1. Clone the repository.
2. Copy the example configuration: `copy .env.example .env` (Windows) or
   `cp .env.example .env` (macOS/Linux). The `.env` file is documentation only —
   keep it as a local reference and **never commit it**.
3. Provide your personal credentials at build/run time:
   ```sh
   flutter run \
     --dart-define=SUPABASE_URL=https://<project-ref>.supabase.co \
     --dart-define=SUPABASE_ANON_KEY=<your-publishable-anon-key>
   ```
   Use the same `--dart-define` values for `flutter build apk --release` so the
   release APK contains your cloud configuration.
4. Local development against a dev-mode device:
   ```sh
   flutter run --dart-define=AURA_DEVELOPMENT_MODE=true
   ```
   `AURA_DEVELOPMENT_MODE` is **false by default** and MUST stay false in
   release builds.
5. Never commit real `.env`, `key.properties`, keystores, or API keys — they
   are already covered by `.gitignore`.

> **Supabase keys:** only the publishable (anon) key is used. The **service role
> key** must never be embedded in the app — it bypasses Row Level Security.
> See [docs/supabase.md](docs/supabase.md).

Firmware-side secrets (Gemini/Sarvam API keys, web/AP credentials) are
configured separately in the firmware repo via the git-ignored `secrets.h`
(copy from `secrets.h.example`).

## Architecture

High-level flow:

```
App startup
   │  SharedPreferences + SecureStorage provisioned in ProviderScope
   ▼
Splash ──► ConnectionNotifier.initialize()
             1. Probe   GET /api/ping          (reachability)
             2. Validate stored token
                POST /api/auth/status         (still valid?)
             3. → dashboard            (token accepted)
                → login screen        (no token / rejected)
                → unavailable panel   (device unreachable, Retry + Advanced)
   ▼
Dashboard ──► DeviceRepository.loadDashboard()
              GET /api/status, /api/wifi, /api/performance,
              /api/settings, /api/version
              └─ WebSocket (ws://host:81) live metric feed
```

### Layers

| Layer | Responsibility | Key files |
| --- | --- | --- |
| UI | Screens, widgets, routing | `lib/screens`, `lib/widgets`, `lib/routes` |
| State | Riverpod providers / notifiers | `lib/providers` |
| Domain | Repositories (data operations) | `lib/repositories` |
| Transport | `ApiClient` (Dio), `ApiService`, WebSocket | `lib/api`, `lib/websocket` |
| Cloud | Supabase auth + PostgREST data access | `lib/core/services/supabase_service.dart`, `lib/repositories/cloud_repository.dart` |
| Services | Secure storage, notifications, TTS/STT, logging | `lib/core/services` |
| Config | Networking defaults, constants, Supabase keys | `lib/core/config`, `lib/core/constants` |

### State management

Riverpod `StateNotifierProvider`s are the single source of truth; widgets never call the network directly:

- `connectionProvider` — device lifecycle + auth (see below)
- `dashboardProvider` — metrics, merges live WebSocket events, fires alerts
- `settingsProvider` — persisted preferences + secure-session load
- `chatProvider`, `voiceProvider`, `memoryProvider`, `remindersProvider`, `otaProvider`, `storageProvider`, `notificationsProvider`

## Connection & auth flow

The login page exposes **only** AURA branding, username, password, Remember Me,
and Forgot Password — no IP/port fields. When the device is unreachable, the
screen offers AURA Cloud (Supabase) sign-in with Sign Up and Forgot Password.

### Local mode (device on the LAN)

```
ConnectionNotifier.initialize()
  ├─ probe (GET /api/ping, timeout from settings)
  │    unreachable ──► phase=unavailable
  │                    UI: "AURA device unavailable." + Retry + Advanced Settings
  │                    + Continue with AURA Cloud (or auto-remote if signed in)
  ├─ token present in secure storage?
  │    yes ──► POST /api/auth/status
  │              authenticated ──► dashboard
  │              otherwise       ──► login screen (unauthenticated)
  └─ login(username, password)
       POST /api/auth/login  ──► token
       Save token + username in flutter_secure_storage  (never the password)
       Connect WebSocket ──► dashboard
```

### Remote mode (Supabase cloud)

```
device unreachable ──► Supabase session exists? ──► yes ──► remote dashboard
                          │ no
                          ▼
        connection screen ── email + password ──► Supabase Auth (persisted)
        · Sign Up (create account)
        · Forgot Password (reset email)
        ──► remote dashboard (cloud devices, offline notice)
```

- **Reconnect** — exponential backoff starting at `settings.reconnectDelayMs`,
  driven by WebSocket + poll failures, only when Auto-Reconnect is enabled.
  After repeated local failures the app automatically falls back to remote
  mode when a cloud session exists.
- **Disconnects** and **failed re-authentication** raise a device alert (when
  Device Alerts are enabled).
- **Logout** clears the local token and the Supabase session, then returns to
  the login screen.
- Manual mode switching: **Settings → Connection**.

## Developer settings

Normal users never see networking details. Open **Settings → About → tap the version 5 times** to reveal the **Developer** tile.

Developer settings expose:

- Device **host**, **REST port**, **WebSocket port** (save → reconnect)
- **Reconnect delay** (ms)
- **Logging** toggle
- **Diagnostics** — fetches `/api/developer`
- **Export logs** — copies `/api/developer/export` to the clipboard
- **Sign out** with confirmation

### Defaults

| Setting | Default |
| --- | --- |
| Host | `192.168.4.1` (ESP32 access point) |
| REST port | `80` |
| WebSocket port | `81` |
| Reconnect delay | 2 s |

## Firmware endpoint compatibility

Companion features vs. the AURA firmware (`web_portal.cpp`). `✔` = implemented and verified against source, `✖` = the firmware does not implement this surface yet (the app degrades gracefully).

| Feature | Endpoint | Firmware |
| --- | --- | --- |
| Ping probe | `GET /api/ping` | ✔ |
| Status | `GET /api/status` | ✔ |
| WiFi | `GET /api/wifi` | ✔ |
| Performance | `GET /api/performance` | ✔ |
| Settings/identity | `GET /api/settings` | ✔ |
| Version identity | `GET /api/version` (mark/codename/channel/build) | ✔ |
| Auth login | `POST /api/auth/login` | ✔ |
| Auth status | `GET /api/auth/status` | ✔ |
| Auth logout | `POST /api/auth/logout` | ✔ |
| Chat | `POST /api/chat` (plain-text reply) | ✔ |
| Memories ranked | `GET /api/memories/ranked` | ✔ |
| Memories search | `GET /api/memories/search?q=` | ✔ |
| Memory pinned | `GET /api/memory/pinned` | ✔ |
| Memory pin | `POST /api/memory/pin` (arg `id`) | ✔ |
| Memory archive | `GET /api/memory/archived` | ✔ |
| Memory restore | `POST /api/memory/restore` (arg `revisionId`) | ✔ |
| OTA version query | `GET /api/ota` | ✖ (use `GET /api/version`) |
| OTA upload | `POST /ota` (multipart field `firmware`) | ✔ |
| OTA JSON progress | `GET /api/ota/progress` | ✖ (progress is transfer-based) |
| Storage list | `GET /api/storage` | ✔ |
| Storage files/upload/delete | `GET/POST/DELETE /api/storage/*` | ✔ |
| Reminders API | `GET /api/reminders` | ✖ (local notifications only) |
| Streaming/SSE chat | — | ✖ (request/response only) |
| Diagnostics | `GET /api/developer`, `/api/developer/export` | ✔ |
| WebSocket live feed | `ws://host:81/` | ✔ |

> **Security principle:** the app stores only the **token, refresh token, and username** via `flutter_secure_storage`. Passwords are never persisted. Legacy plaintext auth prefs are purged on load.

## Project layout

```
lib/
├── api/            # ApiClient (Dio), ApiService, exceptions, paths
├── core/
│   ├── config/     # DeviceConfig (hidden networking), SupabaseConfig (anon key)
│   ├── constants/  # AppConstants, ApiPaths, StorageKeys
│   ├── services/   # storage, secure storage, supabase, notifications, tts, logger, validators, formatters
│   ├── theme/      # AURA design tokens, colors, typography, spacing
│   └── utils/
├── models/         # typed data models
├── providers/      # Riverpod providers/notifiers (incl. connection mode, supabase auth, cloud)
├── repositories/   # data access layer (incl. CloudRepository)
├── routes/         # route table
├── screens/        # connection, splash, dashboard, chat, voice, memory, reminders, ota, storage, settings, developer
├── websocket/      # WebSocketService (reconnect, event stream)
└── widgets/        # GlassCard, MetricCard, MetricBar, StatusBadge, skeletons, page transitions

supabase/
└── migrations/     # SQL schema + RLS policies (apply via SQL editor or `supabase db push`)
```

## Supabase

AURA Companion uses Supabase for remote (cloud) access when the local device
is out of range. See **[docs/supabase.md](docs/supabase.md)** for the schema,
RLS policies, authentication flow, the local-vs-remote architecture, and how
to register additional AURA devices.

- Only the **publishable (anon)** key is embedded in the app
  (`lib/core/config/supabase_config.dart`); the **service role key is never
  used or exposed**.
- Every table has RLS enabled with per-user policies, so an authenticated user
  can only access their own data.
- Apply the migration with the Supabase SQL editor or `supabase db push`.

## Design system

Dark, futuristic glassmorphism built on Material 3:

- Electric blue `#00B4FF` / cyan `#00E5FF` accents on near-black surfaces
- 16px radius cards, glass blur, glow, hero + micro animations
- Skeleton loading and consistent page transitions
- See `lib/core/theme/` for tokens.

## Testing

```sh
flutter test
```

Covers validators, formatters, and JSON model parsing. Endpoint behavior is verified against the firmware source; final validation happens on real hardware.
