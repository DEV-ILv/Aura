# Contributing to AURA OS

Thanks for your interest in contributing to AURA! This guide explains how to
set up the repository, how to write and review code, and how to build and test
the firmware and companion applications.

By participating, you agree to abide by this contribution workflow and to
license your contributions under the [Apache License 2.0](LICENSE).

---

## Repository Setup

### Prerequisites

- **Git** (any recent version)
- **Arduino CLI** (`arduino-cli`) with the ESP32 core (tested: core `3.3.x`)
- **Flutter SDK** (stable channel) for the companion app
- **PowerShell 5.1+** (Windows) or **Python 3.8+** (macOS/Linux) for the
  firmware-signing tooling

### Clone

```bash
git clone https://github.com/DEV-ILv/Aura_v1.git
cd Aura_v1
```

### Firmware

1. Install the ESP32 board support:
   ```bash
   arduino-cli core update-index
   arduino-cli core install esp32:esp32
   ```
2. Prepare the local secrets file (never commit it):
   ```bash
   copy secrets.h.example secrets.h    # Windows
   cp secrets.h.example secrets.h      # macOS / Linux
   ```
3. Generate a signing keypair for OTA (optional but recommended):
   ```bash
   powershell -ExecutionPolicy Bypass -File tools/generate_keypair.ps1   # Windows
   python tools/generate_keypair.py                                      # macOS / Linux
   ```
   The tool writes `keys/` (git-ignored). Copy `keys/public.h` content into
   `firmware_keys.h`.

### Build modes (development vs production)

`AURA_DEVELOPMENT_MODE` selects credential behaviour:

- **`0` — production (default):** the device generates a strong random admin
  password on first boot and stores it in NVS; the setup-AP password is
  MAC-derived. Committed sources default to `0`.
- **`1` — development (local testing only):** well-known credentials
  (`Devil` / `Devil`, setup AP `AURA_Setup` / `DevilDevil`) are used for
  convenience. Enable it in your git-ignored `secrets.h`:
  ```cpp
  #define AURA_DEVELOPMENT_MODE 1
  ```

Never merge a build that has `AURA_DEVELOPMENT_MODE = 1`.

### Companion app

```bash
cd aura_companion
flutter pub get
```

---

## Coding Standards

- **C++ (firmware)**: C++17, ESP32 Arduino core conventions.
  - Follow the existing module layout: a `.h`/`.cpp` pair per subsystem.
  - Use the project's `Logger` / `AuditEventType` facilities; do not add ad-hoc
    `Serial.print` logging.
  - Keep functions `noexcept` where they are called from ISR or tight loops.
  - Match the existing naming style (`snake_case` members, `kCamelCase`
    constants, `PascalCase` types/functions).
  - No hardcoded secrets, API keys, or default passwords anywhere.
- **Dart (companion)**: follow `analysis_options.yaml` (Flutter lints).
  - Use Riverpod providers for state; keep repositories transport-agnostic.
  - Never store credentials in `SharedPreferences`; use
    `flutter_secure_storage`.
- **Web portal SPA**: vanilla ES6, no build step. Keep the existing
  `data/js/` modular layout and the project's `api.js`/`app.js` auth plumbing.
- Never commit generated files, build output, or secrets (see `.gitignore`).

---

## Branch Naming

Use descriptive, lower-case branch names with a type prefix:

| Prefix      | Purpose                          | Example                    |
| ----------- | -------------------------------- | -------------------------- |
| `feat/`     | New feature                      | `feat/ota-rollback`        |
| `fix/`      | Bug fix                          | `fix/websocket-reconnect`  |
| `security/` | Security hardening               | `security/auth-rate-limit` |
| `docs/`     | Documentation                    | `docs/ota-signing`         |
| `refactor/` | Non-functional refactor          | `refactor/wifi-manager`    |

Always branch from an up-to-date `main`:
```bash
git checkout main && git pull
git checkout -b feat/my-feature
```

---

## Commit Message Format

Use conventional commit messages:

```
<type>(<scope>): <short summary>

<body explaining the why, not the what>

<optional footer: BREAKING CHANGE / references>
```

- **Types**: `feat`, `fix`, `security`, `docs`, `refactor`, `test`, `build`,
  `chore`.
- **Scope**: the affected module, e.g. `web_portal`, `ota`, `esp_now`,
  `companion`, `spa`.
- Keep the summary under 72 characters, imperative mood
  ("add", "fix", "remove" — not "added", "fixes").

Examples:

```
security(web_portal): require X-Auth-Token on /api/wifi

fix(ota): fail closed when firmware signature is missing

docs(companion): document --dart-define for Supabase credentials
```

---

## Pull Request Process

1. Create a feature branch (see above).
2. Make focused, reviewable changes. Each PR should solve **one** problem.
3. Keep the CHANGELOG up to date (`## [Unreleased]` section) for user-facing
   changes.
4. Run the checks below locally before pushing.
5. Open a PR against `main`. Use a clear title and link any related issue.
6. Address review feedback; re-request review when done.
7. A maintainer merges the PR. Squash-and-merge is preferred to keep history
   clean.

---

## Code Review Guidelines

Reviewers should check for:

- **Security**: no secrets, no bypassable auth, constant-time token compare,
  rate limiting, fail-closed defaults.
- **Correctness**: boundary/off-by-one errors, buffer sizes, integer overflow,
  ESP32 heap constraints.
- **Regressions**: WebSocket/REST auth contract changes must stay in sync with
  the SPA (`data/js/`) and the companion (`aura_companion/lib/`).
- **Style**: matches the project conventions; no dead code or debug leftovers.
- **Tests**: behavior changes are covered where feasible (companion unit tests
  run under `flutter test`).

Be kind and specific: praise what works, and give concrete, actionable
suggestions.

---

## Running Tests

### Companion app

```bash
cd aura_companion
flutter analyze      # zero issues expected
flutter test         # all unit tests pass
```

### Firmware

The firmware has no automated unit-test harness yet; verification is via
compile + on-device smoke tests:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" Aura_programs
```

A successful build must report **0 errors and 0 warnings**.

---

## Building Firmware

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" Aura_programs
```

Expected baseline (V1.0.0): ~61% flash (1,939,775 / 3,145,728 B) and ~24% RAM
(79,384 / 327,680 B).

### Signing an OTA image

See [`docs/development/ota-signing.md`](docs/development/ota-signing.md) for key generation and
signing steps, including the Windows PowerShell and Python tooling.

---

## Building the Flutter App

```bash
cd aura_companion
flutter pub get
flutter analyze
flutter test
flutter build apk --release     # Android
flutter build windows            # Windows desktop
```

### Supabase cloud credentials

The companion reads Supabase credentials from Dart compile-time defines; never
commit real values. Provide them at build/run time:

```bash
flutter run --dart-define=SUPABASE_URL=https://<project-ref>.supabase.co \
            --dart-define=SUPABASE_ANON_KEY=<your-publishable-anon-key>
```

See `aura_companion/.env.example` for the full list.

---

## AI-Assisted Development

This project is developed with heavy use of AI coding assistants (e.g.
ChatGPT, OpenCode, Gemini). The bulk of the firmware and companion code was
generated with AI assistance; hardware wiring, testing, flashing, and
validation are human-led. Contributions may likewise use AI tools, but please
follow the same standards as any other contribution — review the output,
keep the codebase consistent, and never introduce secrets.

## Code of Conduct

Be respectful and constructive. Harassment, trolling, and abuse will result in
removal from the project. We are all here to make a better open-source AURA.
