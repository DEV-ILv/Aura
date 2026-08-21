# AURA Companion — Supabase Integration

AURA Companion uses **Supabase** for the *remote (cloud)* half of its
architecture. When the ESP32 is reachable on the local network the app talks to
it directly; when it is out of range the app falls back to Supabase for
authentication and cloud data.

This document covers:

- [Database schema](#database-schema)
- [Row Level Security](#row-level-security)
- [Authentication flow](#authentication-flow)
- [Local vs remote architecture](#local-vs-remote-architecture)
- [Adding additional AURA devices](#adding-additional-aura-devices)
- [Running the migration](#running-the-migration)
- [Security notes](#security-notes)

---

## Project

| Item | Value |
| --- | --- |
| Project URL | `https://<project-ref>.supabase.co` |
| App key stored | Publishable (anon) key — client-safe |
| Service role key | **Never** embedded in the app |

Only the publishable key lives in the Flutter app. It is injected at build
time via `--dart-define` (see `.env.example`); no real project values are
committed. All data protection comes from Row Level Security, never from
client-side filtering.

## Database schema

All tables live in the `public` schema and share two conventions:

- `id uuid primary key default gen_random_uuid()`
- `user_id uuid not null references auth.users (id) on delete cascade`

| Table | Purpose | Notable columns |
| --- | --- | --- |
| `users` | Profile mirror of `auth.users` | `email`, `display_name`; auto-created by the `handle_new_user` trigger |
| `devices` | AURA hardware registered to an account | `device_id` (unique serial/MAC), `name`, `model`, `firmware_version`, `mark`, `codename`, `channel`, `is_online`, `last_seen_at`; `unique(user_id, device_id)` |
| `commands` | Audit log of commands issued to a device | `command`, `args jsonb`, `status` (`sent`/`success`/`error`), `response` |
| `reminders` | Cloud-synced reminders | `title`, `body`, `remind_at`, `repeat_interval` (`once`/`daily`/`weekly`), `is_completed` |
| `memory` | Durable memories per user | `content`, `category`, `source` (`companion`/`device`) |
| `notifications` | Notification history per user | `title`, `body`, `type` (`device`/`reminder`/`system`), `read` |
| `settings` | Per-user key/value settings | `key`, `value jsonb`; `unique(user_id, key)` |

Every table has `created_at`; tables that change in place (`users`, `devices`,
`reminders`, `memory`, `settings`) also have `updated_at` maintained by the
`set_updated_at` trigger.

## Row Level Security

RLS is **enabled on every table**, and every table has the same four policies:

| Operation | Policy expression |
| --- | --- |
| `SELECT` | `auth.uid() = user_id` |
| `INSERT` | `WITH CHECK (auth.uid() = user_id)` |
| `UPDATE` | `USING (auth.uid() = user_id)` |
| `DELETE` | `USING (auth.uid() = user_id)` |

Because each policy scopes rows to the authenticated user's own `user_id`, an
account can never read or mutate another account's data — even with the
publishable key.

## Authentication flow

1. **App startup** — `main()` calls `SupabaseService.instance.init()`, which
   runs `Supabase.initialize(url:, publishableKey:)`. Initialisation is
   best-effort: if the network is unavailable the app still works in local
   mode.
2. **Session persistence** — the Supabase client persists the session
   (`access`/`refresh` tokens) via `flutter_secure_storage`, so a signed-in
   user stays signed in across restarts. The refresh token keeps the session
   alive automatically.
3. **Sign in** — `POST /auth/v1/token` (email + password). The `refresh` and
   `authState` streams keep the app's `supabaseAuthProvider` in sync.
4. **Sign up** — `POST /auth/v1/signup`. If email confirmation is required by
   the project, the app tells the user to check their inbox.
5. **Forgot password** — `POST /auth/v1/recover` sends a reset link to the
   user's email.
6. **Logout** — `POST /auth/v1/logout` revokes the session locally and on the
   server; persisted tokens are cleared.

App-side surfaces:

- `lib/core/services/supabase_service.dart` — thin wrapper around the client.
- `lib/providers/supabase_auth_provider.dart` — auth state for the UI.
- `lib/repositories/cloud_repository.dart` — RLS-scoped data access.
- `lib/screens/connection/connection_screen.dart` — mode-aware sign-in UI.

## Local vs remote architecture

```
                    ┌───────────────────────────────────────┐
                    │            AURA Companion              │
                    └───────────────────────────────────────┘
                                   │
                  ConnectionNotifier.initialize()
                                   │
                    probe local device (GET /api/ping)
                                   │
              ┌────────────────────┴────────────────────┐
              │ reachable                                │ not reachable
              ▼                                          ▼
      LOCAL MODE (default)                       REMOTE MODE (Supabase)
      ┌───────────────────────────┐               ┌─────────────────────────┐
      │ ESP32 on LAN              │               │ Supabase project        │
      │  REST   → http://host:80  │               │  Auth   → email/password│
      │  WebSocket → ws://host:81 │               │  Data   → PostgREST     │
      │  Live metrics, chat,      │               │  Devices/reminders/     │
      │  memories, OTA, storage   │               │  memory/settings/cloud  │
      └───────────────────────────┘               └─────────────────────────┘
```

- **Local is always preferred.** The connection initialiser probes the device
  first and only falls back to the cloud when the probe fails.
- **Automatic switching**
  - Startup: device unreachable **and** a Supabase session exists → the app
    enters remote mode automatically and opens the dashboard.
  - Device unreachable **without** a cloud session → the connection screen
    shows "AURA device unavailable." with **Retry**, **Advanced Settings**,
    and **Continue with AURA Cloud sign-in**.
  - After connecting locally, if the device is lost and local reconnect keeps
    failing (≥ 3 attempts) while a cloud session exists, the app automatically
    switches to remote mode instead of staying offline.
  - Manual overrides live in **Settings → Connection** ("Use cloud" /
    "Use local device").
- **Remote-mode dashboard** shows the signed-in user's cloud-registered
  devices with online/offline state. Live metrics require the device on the
  LAN.

### Which features work where

| Feature | Local mode | Remote mode |
| --- | --- | --- |
| Live metrics / WebSocket feed | Yes | No (device out of range) |
| Chat / voice with the device | Yes | No |
| Memories / reminders via device | Yes | Cloud-synced copies |
| Cloud sign-in / session | — | Yes |
| Cloud device registry | Auto-synced on connect | Read-only view |

## Adding additional AURA devices

Every device gets a unique `device_id` (the ESP32's serial/MAC). To add a
second AURA unit:

1. Power on the new AURA device and connect this app to it on the LAN
   (the local probe + login flow will register it).
2. The device is stored in `devices` under the signed-in user's account with
   `unique(user_id, device_id)`, so one account can own many devices.
3. When you later switch to remote mode, all of your devices appear in the
   cloud dashboard (Settings → Connection → Use cloud, or automatically when
   the LAN device is unreachable).
4. To remove a device, delete its row via the Supabase dashboard or the
   `devices` endpoint (`deleteDevice`).

Firmware note: for true remote control (issuing commands to an out-of-range
device), the ESP32 firmware must also connect out to Supabase Realtime. The
companion app treats an offline device as "not on the LAN" and shows it as
offline until the firmware-side Realtime link is implemented.

## Running the migration

Option A — Supabase Dashboard:

1. Open `https://<project-ref>.supabase.co` → **SQL Editor**.
2. Paste the contents of
   `supabase/migrations/20260803000001_init_schema.sql`.
3. **Run**. RLS is enabled and the role grants are applied by the migration.

> **Important — grants.** The base migration drops and recreates the tables,
> which also removes the privileges Supabase normally auto-applies to new
> tables. The migration therefore ends with explicit
> `GRANT ALL ... TO anon, authenticated, service_role` statements. If you
> already created the schema with an older version of the migration and
> PostgREST returns `42501 permission denied for table ...`, run
> `supabase/migrations/20260803000002_grant_table_access.sql` (grants only,
> idempotent, does not touch your data).

> **Owner default (required).** The app inserts rows without a `user_id`
> column and relies on RLS to scope them. Databases created with the original
> migration have `user_id uuid not null` with no default, so every insert
> fails with `42501 new row violates row-level security policy`. Run
> `supabase/migrations/20260803000003_user_id_defaults.sql` once (adds
> `default auth.uid()` to the owner columns; idempotent). Fresh installs
> already include the defaults.

Option B — Supabase CLI:

```sh
supabase link --project-ref <project-ref>
supabase db push
```

## Security notes

- **Publishable (anon) key only.** `SupabaseConfig.anonKey` is the only key in
  the repository. The anon key is meant for clients; it cannot bypass RLS.
- **Service role key never.** The service role key bypasses RLS and must only
  live in server-side secrets (edge functions, backend) — never in app code,
  builds, or Git history.
- **Passwords never stored.** Auth is delegated to Supabase Auth; the app only
  ever holds session tokens (in `flutter_secure_storage`).
- **Per-user isolation.** Every table's RLS policies guarantee a user can only
  access their own rows.
