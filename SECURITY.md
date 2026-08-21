# Security Policy

AURA OS ("AURA") takes the security of its firmware, web portal, and companion
applications seriously. This document describes how security is handled for the
project, how to report a vulnerability, and what protections are built in.

---

## Supported Versions

AURA is currently released as version `1.0.0` (Mark III "Phoenix"),
Development channel.

| Version | Channel  | Supported          |
| ------- | -------- | ------------------ |
| 1.0.0   | Development | :white_check_mark: |
| < 1.0.0 | —        | :x:                |

The firmware targets the ESP32 family with the Arduino core (tested against
core `3.3.x`) and the `huge_app` partition scheme. Only the latest Development
channel build receives security fixes. Regressions and security patches are
backported on request for critical issues only.

---

## Reporting a Security Vulnerability

If you believe you have found a security vulnerability in AURA, **do not open a
public issue**.

Please report it privately:

1. Open a **private advisory** on GitHub:
   `https://github.com/DEV-ILv/Aura_v1/security/advisories/new`
2. Or email the maintainer (address listed on the repository profile).

Please include:

- A description of the vulnerability and the affected component (web portal,
  WebSocket, OTA, ESP-NOW, companion app, Supabase backend).
- Steps to reproduce, including the firmware version and hardware used.
- Impact assessment (what an attacker could achieve).
- A suggested fix, if you have one.

### Response timeline

| Step              | Target                 |
| ----------------- | ---------------------- |
| Acknowledgment    | 48 hours               |
| Triage + impact   | 5 business days        |
| Fix + release     | 30 days for Critical/High |

We follow a coordinated disclosure model: reporters are credited (unless they
request anonymity) once a fix is released.

---

## Authentication Architecture

### First-boot credentials

- `secrets.h` contains **no default password** in production builds. On first
  boot the firmware generates a strong random 32-hex-character admin password
  (`esp_random()`), stores it in the `auraauth` NVS namespace, and prints it to
  the Serial monitor.
- The login is flagged `must_change = true` until the password is replaced via
  `/api/auth/change-password` (minimum 8 characters, current password
  required).
- Existing user credentials are preserved across NVS layout migrations
  (`kAuthCredVersion = 3`); they are never reset to defaults.

### Development mode (local testing only)

A compile-time flag `AURA_DEVELOPMENT_MODE` selects credential behaviour:

- **`0` — production (default).** Random first-boot admin password, MAC-derived
  setup-AP password. No known credentials exist anywhere in the source tree.
- **`1` — development.** Well-known development credentials are used for local
  testing convenience:
  - Web Portal / REST: `Devil` / `Devil`
  - Setup AP (`AURA_Setup`): `DevilDevil`

The flag defaults to `0` in committed sources. Enable it locally by setting
`#define AURA_DEVELOPMENT_MODE 1` in the git-ignored `secrets.h` (or
`config.h`), and build the companion with
`--dart-define=AURA_DEVELOPMENT_MODE=true` to prefill the same credentials.
**A development build must never be shipped or published.** All authentication,
token validation, rate limiting, OTA verification, and WebSocket protection
remain active in both modes.

### REST API

| Endpoint                     | Auth required | Notes                                        |
| ---------------------------- | ------------- | -------------------------------------------- |
| `POST /api/auth/login`       | No            | Returns `{ token, expiresIn, must_change }`  |
| `GET  /api/auth/status`      | No           | Returns `{ authenticated, hasSession, must_change, expiresIn }` |
| `POST /api/auth/logout`      | Yes           | Revokes the session token                    |
| `POST /api/auth/change-password` | Yes       | Requires `current_password` + `new_password` |
| `GET  /api/status`, `/api/wifi`, `/api/settings`, `/api/performance`, … | Yes | 401 without `X-Auth-Token` |
| `POST /api/*`, `/wifi`, `/settings`, `/ota`, `/restart`, `/factory-reset` | Yes | 401 without `X-Auth-Token` |

- Every authenticated route calls `isAuthenticatedOrReject()` and returns
  `401 {"error": "unauthorized"}` when the token is missing or invalid.
- Session tokens are random (`esp_random()`), compared in constant time
  (`SecurityManager::CheckToken`), and expire after `kSessionTimeoutMs`.

### Login rate limiting

- Failed logins are tracked **per source IP** (5 attempts → 30 second lockout).
- The tracker is a bounded map (32 entries) so a single attacker cannot lock
  out other clients or exhaust device memory.
- Successful login resets the counter for that IP; failed attempts are written
  to the audit log.

### WebSocket (port 81)

- The browser WebSocket handshake is validated for **Origin** against the
  device's own IP, AP IP, and hostname (`.local`) to block cross-origin
  hijacking.
- On connect the server sends `{"type":"auth_required"}`; the client must
  reply `{"type":"auth","token":"<session-token>"}`.
- Tokens are validated with `SecurityManager::CheckToken`. An invalid or
  expired token is answered with `{"type":"error","message":"unauthorized"}`
  and the connection is dropped.
- Broadcast messages (`sendToAuthenticatedClients`) are only delivered to
  clients that completed the auth handshake.
- Before authentication the server only accepts `ping`, `auth_required`,
  `pong`, and handshake error messages.

### Companion application

- The Flutter companion stores the session token in
  `flutter_secure_storage` (not plain preferences) and attaches it as the
  `X-Auth-Token` header on every authenticated request.
- The web portal SPA keeps the token in `sessionStorage` (cleared when the
  tab closes) and never writes it to disk.
- Both clients treat `401` from the device as a session-expiry event and
  return the user to the sign-in screen.

---

## OTA Signing

Firmware updates are authenticated end-to-end:

- Firmware images are signed with an **ECDSA P-256 (secp256r1)** private key.
- The device embeds only the matching **public key** as a DER
  SubjectPublicKeyInfo blob in `firmware_keys.h`.
- The signature is a **DER-encoded ECDSA (r ‖ s)** over the **SHA-256**
  digest of the firmware image.
- `OtaManager::verifyFirmwareSignature()` is **fail-closed**: a firmware image
  without a valid signature is rejected.
- The web-portal OTA upload verifies the signature from the `X-Signature`
  header or the `signature` form field before applying the update. A missing
  signature is logged as a warning; an invalid signature aborts the update
  with `400` and an audit event.

See [`docs/development/ota-signing.md`](docs/development/ota-signing.md) for key generation, signing,
verification, rotation, and CI workflows.

---

## Secret Management

- `secrets.h` is **git-ignored** and never committed. A template
  (`secrets.h.example`) is committed; copy it to `secrets.h` before building.
- Runtime secrets (admin password, API keys) are stored in NVS and can be
  configured through the web portal without recompiling.
- Signing keys live in `keys/`, which is **git-ignored**:
  - `private.d` / `private.pem` — NEVER commit.
  - `public.h` — safe to commit; it is only the public key.
- The Supabase **service role key** must never be embedded in the companion
  app. Only the publishable (anon) key is client-visible, and data is
  protected by Row Level Security (RLS).
- **Firmware build artifacts embed secrets.** Compiled images under
  `build/` (`*.bin`, `*.merged.bin`, `*.elf`) contain whatever API keys were
  present in `secrets.h` at compile time, and they are extractable with a
  simple `strings` scan. `build/` is git-ignored — **never publish, share, or
  distribute build artifacts**. For firmware meant for distribution, build
  with placeholder `secrets.h` values and provision real keys at runtime via
  Web Portal → Settings.
- The companion app takes cloud credentials via `--dart-define`
  (`SUPABASE_URL`, `SUPABASE_ANON_KEY`) at build time; no secrets are
  hard-coded in the source tree. `.env.example` and `secrets.h.example` are
  placeholder-only templates and are safe to commit.

### Credential rotation & exposure policy

- Rotate an API key or the OTA signing key **only when a real secret was
  actually exposed outside this machine** — published in a repository, shared
  as a file, uploaded to a public service, or embedded in a distributed
  binary.
- A leak confined to your own machine (e.g. a local `build/` artifact) does
  **not** require rotation; ignore or delete the artifact instead.
- To rotate the OTA signing key, follow `docs/ota-signing.md` §6
  (key rotation). To rotate an API key, revoke it in the provider console and
  update `secrets.h` (or the runtime setting via Web Portal → Settings).

---

## Transport Security

- The web portal and WebSocket run over **plain HTTP on the LAN**. AURA does
  not (yet) implement TLS; do not expose the device or companion endpoints
  directly to the public internet.
- The companion's cloud (Supabase) path always uses HTTPS.

---

## ESP-NOW

- Peer encryption is enabled (`peerInfo.encrypt = true`) with a Primary
  Master Key (PMK) set via `esp_now_set_pmk()`.
- Privileged message types (text, commands, OTA) are only processed from
  **paired** nodes; unpaired traffic is logged and ignored.

---

## Best Practices for Users & Deployers

1. Change the admin password immediately after first login (the device forces
   this on first boot).
2. Never ship a development build: ensure `AURA_DEVELOPMENT_MODE` is `0`
   (production) before any release, and verify with the build-mode banner
   printed to Serial on boot.
3. Keep the firmware signing private key offline and rotate it only if it was
   actually exposed outside this machine (see rotation policy above).
4. Use unique, strong Wi-Fi and admin passwords; never reuse `secrets.h`
   values across devices.
5. Do not forward device ports (80/81) to the public internet.
6. Update firmware only from sources you trust; prefer signed OTA images.
7. Keep the Supabase project locked down: enable RLS on all tables, use the
   anon key only in the client, and store the service role key in a server
   environment.
8. Never publish `build/` output or distribute firmware images compiled with
   real `secrets.h` values — API keys are embedded in the binary and
   extractable with `strings`. Build distributable images with placeholder
   secrets and provision keys at runtime.

---

## Known Limitations

- **No TLS on the LAN**: REST and WebSocket traffic is unencrypted on the
  local network. Mitigation: keep the device on a trusted network and restrict
  port exposure.
- **Per-IP rate limiting is memory-bounded** (32 IPs): a distributed attack
  could still trigger lockouts, but cannot bypass authentication.
- **Web portal session tokens** live in `sessionStorage` (SPA) or secure
  storage (companion) and are not persisted server-side beyond the in-memory
  token — a device reboot invalidates all sessions.
- **OTA signing is advisory for unsigned uploads**: unsigned uploads are
  logged and rejected by `OtaManager`, but the web-portal upload path logs a
  warning rather than hard-failing when no signature is present. Enforce
  signing in your CI.
- **Supabase email flows** are subject to the provider's quota
  (`over_email_send_rate_limit`); this is an operational limit, not a
  security issue.

---

## Reporting

Security advisories: `https://github.com/DEV-ILv/Aura_v1/security/advisories`
License: [Apache License 2.0](LICENSE).
