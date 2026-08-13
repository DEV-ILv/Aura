# AURA Repository Security Audit

**Date:** 2026-08-13
**Scope:** Full repository scan of the AURA workspace before publishing to GitHub.
**Audit type:** Static secret scan (source, config, docs, git history, build artifacts, binaries, databases, images).
**Method:** Recursive file scan, git history inspection (`git log -p`, `git grep --all`, `git fsck`, `git stash`, `git reflog`), binary string extraction (`strings`-equivalent), APK inspection, remote-repository verification.
**File modification during scan:** None. No files were created, modified, rotated, or deleted during the scan.

---

## 1. Overall Status

### VERDICT: SAFE TO PUBLISH

The **tracked source tree and the complete git history** of `Aura_programs` are clean. **No real API key, password, token, private key, or credential was ever committed to git history.** The repository `https://github.com/DEV-ILv/Aura` is already public and currently contains only safe tracked files.

**However, real secrets exist on the local machine in git-ignored locations.** These must never be published, uploaded, or distributed:

| Local secret | Location | Why it exists | Status |
|---|---|---|---|
| Real Gemini API key + real Sarvam AI key | `Aura_programs/secrets.h` | Build-time credentials (git-ignored) | **[SECRET]** local only |
| Real ECDSA P-256 private key | `Aura_programs/keys/private.pem` + `private.d` | OTA signing (git-ignored) | **[SECRET]** local only |
| Real Gemini + Sarvam keys embedded in binaries | `Aura_programs/build/` (firmware `.bin`/`.elf`) | Compiled into firmware builds (git-ignored) | **[SECRET]** must not publish binaries |
| Real Supabase project URL | `aura_companion/release/artifacts/aura_companion_1.1.0+3_release.apk` | Embedded in APK at release-build time | **[SENSITIVE]** do not publish this artifact |

---

## 2. Secret Findings

| # | Location | Type | Classification | Details |
|---|---|---|---|---|
| S1 | `Aura_programs/secrets.h` | Real Gemini API key | **[SECRET]** | Real, active key. Git-ignored, never tracked. Value redacted. |
| S2 | `Aura_programs/secrets.h` | Real Sarvam AI API key | **[SECRET]** | Real, active key. Git-ignored, never tracked. Value redacted. |
| S3 | `Aura_programs/keys/private.pem` | ECDSA P-256 private key | **[SECRET]** | Real private key. Git-ignored (`keys/`), never tracked. |
| S4 | `Aura_programs/keys/private.d` | Private key scalar (64 hex chars) | **[SECRET]** | Real private key material. Git-ignored, never tracked. |
| S5 | `Aura_programs/build/**/*.bin` / `.elf` / `.merged.bin` | Firmware binaries | **[SECRET]** | Every firmware artifact embeds the real Gemini + Sarvam keys (`GEMINI=True SARVAM=True`). Git-ignored (`build/`). |
| S6 | `aura_companion/release/artifacts/aura_companion_1.1.0+3_release.apk` | Supabase project URL | **[SENSITIVE]** | Embedds real project URL (`https://*.supabase.co`). No anon key / JWT / `sbp_` token found inside. `+4` APK does NOT contain the URL. |

**No other secrets found.** No AWS/GCP/Azure keys, no GitHub tokens, no OAuth client secrets, no service-account JSON, no signing keystore (`.jks`/`.keystore`/`key.properties`) was found anywhere.

---

## 3. URL Findings

| URL | Context | Classification |
|---|---|---|
| `https://github.com/DEV-ILv/Aura` | Project repository (public) | **[PUBLIC]** |
| `https://github.com/DEV-ILv/Smart_buddy_prototype-1` | Prototype repo (public remote configured) | **[PUBLIC]** |
| `https://generativelanguage.googleapis.com` | Gemini API endpoint (`geminiai_agent.cpp`) | **[PUBLIC]** |
| `https://api.sarvam.ai` | Sarvam STT/TTS endpoint (`docs/sarvam_ai.md`, firmware) | **[PUBLIC]** |
| `https://dashboard.sarvam.ai` | Sarvam dashboard (documentation only) | **[PUBLIC]** |
| `https://bzkfxvgupkpduzduzvsi.supabase.co` (redacted) | Supabase project URL embedded in APK +3 | **[SENSITIVE]** |
| `https://fonts.googleapis.com`, `fonts.gstatic.com`, `developer.android.com`, `keepachangelog.com`, `semver.org` | Companion docs/assets | **[PUBLIC]** |
| `https://192.168.4.1`, `http://<LAN_IP>:80/81` | Device LAN (see IP section) | **[SENSITIVE]** |

**Remote repository verification:** GitHub API confirms the public repo does NOT contain `secrets.h` or `keys/private.pem` (both 404). Only the tracked, clean file set is published.

---

## 4. IP / Network Findings

| Value | Context | Classification |
|---|---|---|
| `192.168.4.1` | Default ESP32 SoftAP address (standard, `AURA_Setup`) | **[PUBLIC]** standard default |
| `<LAN_IP>` | Developer's private LAN STA IP (ports 80/81 open) | **[SENSITIVE]** private network |
| `127.0.0.1` | Local unit-test binding | **[PUBLIC]** |
| `10.0.0.255` | Test constant only | **[PUBLIC]** |
| `<DEVICE_MAC>` (MAC) | Device MAC in `AURA_STARTUP_DIAGNOSTIC.md` (untracked) | **[SENSITIVE]** device identity |

**Note:** The private LAN IP and device MAC appear only in **untracked** diagnostic reports (`AURA_STARTUP_DIAGNOSTIC.md`, `AURA_WIFI_FIX_REPORT.md`). If these files are committed, redact the IP/MAC first (recommendation below).

---

## 5. Credential Files

| File | Tracked? | Contents | Classification |
|---|---|---|---|
| `secrets.h.example` | **Yes** | All API-key fields empty; only `AP_SSID` (len 10, documented `AURA_Setup`) and `WEB_USERNAME` (len 5, documented `Devil`) pre-filled. Placeholder template. | **[PLACEHOLDER]** safe |
| `secrets.h` (local) | No (ignored) | Real Gemini + Sarvam keys + dev-mode credentials | **[SECRET]** local only |
| `config.h` (committed) | Yes | Dev constants `AURA_DEV_WEB_USERNAME "Devil"`, `AURA_DEV_WEB_PASSWORD "Devil"`, `AURA_DEV_AP_PASSWORD "DevilDevil"` — compile-time only, gated by `AURA_DEVELOPMENT_MODE` (default **0** = production) | **[PLACEHOLDER]** known dev defaults; active only in dev builds |
| `aura_companion/lib/core/config/supabase_config.dart` | (companion not a repo) | `String.fromEnvironment('SUPABASE_URL')` / `('SUPABASE_ANON_KEY')` — dart-define injection, no committed values | **[SAFE]** |
| `aura_companion/lib/core/config/app_config.dart` | (companion not a repo) | Dev `kDevUsername`/`kDevPassword` (`Devil`/`Devil`), gated by dev mode | **[PLACEHOLDER]** |
| `aura_companion/lib/core/config/device_config.dart` | (companion not a repo) | `defaultHost = 192.168.4.1` (standard SoftAP) | **[SAFE]** |
| `.env.example` files (both projects) | **Yes** | Placeholders only | **[PLACEHOLDER]** safe |
| `aura_companion/supabase/migrations/*.sql` | (companion not a repo) | Schema/RLS only; comments state service-role key must never ship in app. No credentials. | **[SAFE]** |

---

## 6. Build Artifact Findings

| Artifact | Keys found | Classification |
|---|---|---|
| `build/flash_latest/*.bin`, `*.merged.bin`, `*.elf` | Real Gemini + Sarvam keys | **[SECRET]** |
| `build/release_cli/*.bin`, `*.elf` | Real Gemini + Sarvam keys | **[SECRET]** |
| `aura_companion_1.1.0+3_release.apk` | Real Supabase project URL; no anon key, no JWT, no Gemini/Sarvam | **[SENSITIVE]** |
| `aura_companion_1.1.0+4_release.apk` | No Supabase URL, no secrets | **[SAFE]** |
| `*.ino.bin`/elf under `build/` | Dev-mode strings (`DevilDevil`, `AURA_Setup`) + keys | **[SECRET]** |

**Rule:** `build/` is git-ignored, so these will not be pushed via git. **Never** attach these `.bin`/`.elf`/`.merged.bin` files to GitHub Releases or upload them anywhere — they contain live API keys. The APK `+3` contains a real Supabase URL — do not distribute that specific artifact; rebuild with injected config or use `+4`.

---

## 7. Git History Findings

| Check | Result |
|---|---|
| Real API key / password / token / private key in any commit | **NONE FOUND** |
| `secrets.h` in history | Committed in `5947492` (Initial commit) and `a8ad9ce`, but **all values were empty** (template only) |
| `keys/` in history | Never committed |
| `build/` artifacts in history | Never committed |
| Private-key/credential patterns (`git grep --all`: `AIza`, `sk_`, `ghp_`, `Bearer`, `JWT`, `BEGIN ... PRIVATE KEY`, etc.) | **No matches** |
| Deleted secret files | None (no secret file was ever tracked with real content) |
| Dangling objects (`git fsck`) | 1 blob = the old empty `secrets.h`; 1 tree — both benign |
| Stash / reflog | No stash entries; no secret references in reflog |
| `config.h` history | Only dev-mode compile-time constants (`Devil`/`DevilDevil`), never runtime secrets |

**Explicit statement:** **No real API key, password, token, private key, or credential of any kind was found in the git history of this repository.** The only secret-adjacent file ever committed was `secrets.h` with all empty values.

---

## 8. `.gitignore` Review

**`Aura_programs/.gitignore` — adequate.** Correctly ignores:
- `secrets.h` / `secrets.*.h`
- `.env*` (except `.env.example`)
- `keys/`, `*.pem`, `*.key`, `*.p12`, `*.pfx`, `*.jks`, `*.keystore`, `key.properties`
- `build/`, `*.bin`, `*.elf`, `*.map`, `*.hex`, `*.log`
- `credentials.json`, service-account files, etc.

Verified with `git check-ignore`: `secrets.h`, `keys/`, `build/` all correctly ignored.

**Root `C:\Users\somas\Documents\Arduino\.gitignore` — adequate** (same categories + `*.apk`/`*.aab` + `.freebuff/`). Covers monorepo-initialization case.

**`aura_companion/.gitignore` — needs a small hardening before the companion is ever published** (see R2):

---

## 9. Files Requiring Attention

| # | File | Line/Region | Type | Classification | Why | Recommended action |
|---|---|---|---|---|---|---|
| A1 | `secrets.h` (local) | whole file | API keys | **[SECRET]** | Real Gemini + Sarvam keys | Keep git-ignored. Never commit, never share, never upload. Optional: rotate keys since they're embedded in local builds. |
| A2 | `keys/private.pem`, `keys/private.d` | whole files | Private key | **[SECRET]** | Real ECDSA P-256 signing key | Keep git-ignored. Never publish. Store offline/backed up. |
| A3 | `build/` binaries | firmware artifacts | Embedded keys | **[SECRET]** | Real API keys baked into binaries | Never attach to Releases/PRs. Rebuild fresh if a binary must be shared. |
| A4 | `aura_companion/release/artifacts/aura_companion_1.1.0+3_release.apk` | embedded URL | Supabase URL | **[SENSITIVE]** | Real project endpoint | Do not distribute. Use `+4` or rebuild with `--dart-define`. |
| A5 | `AURA_STARTUP_DIAGNOSTIC.md` (untracked) | MAC + LAN IP rows | Network info | **[SENSITIVE]** | Private IP + device MAC | If committing, redact IP/MAC or keep untracked. |
| A6 | `AURA_WIFI_FIX_REPORT.md` (untracked) | various | Network info | **[SENSITIVE]** | Private LAN IP | If committing, redact IP/MAC or keep untracked. |
| A7 | `config.h` (committed) | dev credential constants | Dev creds | **[PLACEHOLDER]** | Known dev defaults, compile-time only, off in production | Acceptable. Do not set `AURA_DEVELOPMENT_MODE 1` in committed files. |
| A8 | `secrets.h.example` (committed) | template | Placeholders | **[PLACEHOLDER]** | API keys empty; only documented `AURA_Setup` SSID + `Devil` username | Acceptable as-is. |

---

## 10. GitHub Publishing Checklist

Before pushing, confirm:

- [x] `git status` shows only intended source files (7 modified `.cpp/.h` files + optional redacted MD reports).
- [x] `secrets.h`, `keys/`, `build/`, `.env` are all git-ignored (`git check-ignore` verified).
- [x] No `*.bin`, `*.elf`, `*.apk`, `*.jks`, `*.pem` is tracked (verified: 249 tracked files, source/docs only).
- [x] Git history contains no real secrets (verified via `git grep --all` + `git log -p` + `git fsck`).
- [x] Remote `https://github.com/DEV-ILv/Aura` already public, confirmed clean (no `secrets.h`, no `keys/`).
- [x] Diagnostic reports either redacted (IP/MAC) or kept out of the commit.
- [ ] **Do not** attach any local firmware binary or the APK `+3` to releases.
- [ ] Optional: rotate Gemini + Sarvam keys (they are embedded in local `build/` artifacts).
- [ ] Before publishing `aura_companion` (new repo): add `release/` and `*.apk`/`*.aab` to its `.gitignore`; never commit `supabase_config` real values (use dart-define).

---

## 11. Final Statement

- **Verdict: SAFE TO PUBLISH** for the tracked source tree and git history of `Aura_programs`.
- **Explicit:** No real API key, password, token, private key, or credential was ever present in this repository's git history. The only related commit was the `secrets.h` template with empty values.
- **Caution:** Real secrets live only in local git-ignored files (`secrets.h`, `keys/`, `build/` binaries) and one local APK artifact (`+3`, Supabase URL). These must never be published. Treat local build artifacts as containing live credentials.
