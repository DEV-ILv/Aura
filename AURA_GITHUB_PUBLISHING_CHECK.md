# AURA GitHub Publishing Check

**Date:** 2026-08-13
**Auditor:** Automated security scan (post-cleanup re-verification)
**Scope:** `Aura_programs` (firmware git repo) + `aura_companion` (Flutter app) + local artifacts
**Action taken during this pass:** Cleanup only. **No commit, no push, no flash, no key rotation, no file deletion.**

---

## Status

**READY FOR GITHUB** — tracked source tree, git history, and diagnostic reports are clean. Local git-ignored secret material (API keys, private key, build binaries, APKs) remains local and is correctly excluded from version control.

---

## Changes Made

1. **Redacted network identifiers** in two diagnostic reports:
   - `AURA_STARTUP_DIAGNOSTIC.md` — device MAC and private LAN STA IP replaced with `<DEVICE_MAC>` / `<LAN_IP>` / `<DEVICE_IP>` placeholders.
   - `AURA_WIFI_FIX_REPORT.md` — private LAN STA IP replaced with `<DEVICE_IP>`.
   - `AURA_REPOSITORY_SECURITY_AUDIT.md` — same identifiers redacted in its own findings tables.
2. **Hardened `aura_companion/.gitignore`** — added `release/`, `*.apk`, `*.aab`, `*.pem`, `*.key`, `*.jks`, `*.keystore`, `key.properties`, `credentials/`, `secrets/`.
3. **Verified firmware build artifacts** — `build/`, `keys/`, `secrets.h` all confirmed git-ignored; **no binary, APK, key, or `secrets.h` is tracked**.
4. **Classified the Supabase URL** in the release APK as public client configuration (see below).

**No firmware behavior was modified.** `config.h` touch-timing profile is untouched (`TAP_MIN=60, TAP_MAX=450, DOUBLE_TAP=500, SETUP_HOLD=5000`). No API keys rotated. No files deleted.

---

## Secret Scan

Re-scanned all tracked files, untracked reports, companion source, and the full git history.

| Pattern class | Result |
|---|---|
| Google API keys (`AIza…`) | Not found |
| Sarvam keys (`sk_…`) | Not found |
| GitHub tokens (`ghp_`, `gho_`, `github_pat_`) | Not found |
| AWS keys (`AKIA…`) | Not found |
| Private key blocks (`-----BEGIN … PRIVATE KEY-----`) | Not found |
| JWTs (`eyJ…`) | Not found |
| OAuth secrets, bearer tokens | Not found |

**Git history:** `git grep --all` across every commit — no secret patterns. The only secret-adjacent file ever committed is `secrets.h` with **empty** values (template only). **No real API key, password, token, private key, or credential was ever committed.**

**Git diff (working tree):** secret scan of `git diff` — clean.

---

## Network Identifier Scan

| Identifier | Before | After | Classification |
|---|---|---|---|
| Private LAN STA IP | `192.168.1.107` (in 3 reports) | `<LAN_IP>` / `<DEVICE_IP>` | **[SENSITIVE]** redacted |
| Device MAC | `68:09:47:28:c9:3c` (1 report) | `<DEVICE_MAC>` | **[SENSITIVE]** redacted |
| SoftAP IP `192.168.4.1` | kept in docs/code | unchanged | **[PUBLIC]** standard ESP32 default (keep) |
| Example IP `192.168.1.100` | `web_portal.h` doc comment | unchanged | **[PUBLIC]** generic example (keep) |
| mDNS `aura-<mac>.local` | docs | unchanged | **[PUBLIC]** placeholder `mac` (keep) |

**No real private IP or MAC address remains in any tracked file or report.**

---

## Firmware Artifact Scan

- `build/flash_latest/*.bin|*.merged.bin|*.elf` — **git-ignored** (`.gitignore:41 build/`).
- `build/release_cli/*.bin|*.elf` — **git-ignored**.
- `keys/private.pem`, `keys/private.d` — **git-ignored** (`.gitignore:31 keys/`).
- `secrets.h` — **git-ignored** (`.gitignore:9 secrets.h`).
- Tracked files: **0** binaries, **0** `.bin`, `.elf`, `.map`, `.hex`, `.apk`, `.pem`, `.key` — verified via `git ls-files`.
- **Verdict:** no credential-bearing firmware binary is tracked. Build artifacts stay local.

---

## Companion APK Scan

- The release APK `aura_companion_1.1.0+3_release.apk` embeds a Supabase **project URL** only.
- **Classification: public client configuration.** A Supabase project URL is not a credential by itself; the Supabase anon key is designed to be embedded in clients, and data access is protected by Row Level Security (RLS), per the app's own `supabase_config.dart` and release notes.
- **Verified absent from the APK:** anon key JWT (`sbp_`/`eyJ…`), service-role key, `service_role` marker, any private-key material.
- **Mitigation:** the APKs live in `release/`, which is now git-ignored (`release/`, `*.apk`, `*.aab`). **No APK will be committed.** The APK is not published during this task.
- `supabase_config.dart` injects credentials via `--dart-define` (no values committed); `.env.example` is placeholder-only.

---

## Git History Scan

| Check | Result |
|---|---|
| Secret patterns across all commits | Clean |
| `secrets.h` in history | Only empty-value template (2 commits) |
| `keys/` in history | Never committed |
| `build/` artifacts in history | Never committed |
| Deleted secret files | None |
| Stash / reflog / fsck | Clean (1 dangling blob = empty secrets.h) |
| **Any real credential ever committed** | **No** |

---

## Gitignore Verification

**`Aura_programs/.gitignore`** (verified with `git check-ignore`):
- `secrets.h` ✓, `keys/` ✓, `build/` ✓, `.env*` ✓, `*.pem/*.key/*.jks` ✓, `*.bin/*.elf/*.map/*.hex/*.apk/*.aab` ✓.

**`aura_companion/.gitignore`** (hardened this session):
- Added: `release/`, `*.apk`, `*.aab`, `*.pem`, `*.key`, `*.jks`, `*.keystore`, `key.properties`, `credentials/`, `secrets/`.
- Already present: `.env`, `.env.*`, `*.env`, `!.env.example`, `service_account.json`, `/build/`, `/android/app/*/release`.
- Companion has **no `.git`** yet — these rules protect it when it becomes its own repo.

**Root `Arduino\.gitignore`** — already comprehensive (covers `.freebuff/`, secrets, keys, `build/`, binaries). Unchanged.

---

## Files Safe to Commit

```
M config.h                  (touch-timing constants only; secret scan clean)
M esp_now_manager.cpp       (Wi-Fi ownership fix)
M resilience_manager.cpp    (Wi-Fi ownership fix)
M system_manager.cpp        (Wi-Fi ownership fix)
M system_manager.h          (Wi-Fi ownership fix)
M wifi_manager.cpp          (Wi-Fi ownership fix)
M wifi_manager.h            (Wi-Fi ownership fix)
?? AURA_REPOSITORY_SECURITY_AUDIT.md   (findings report; identifiers redacted)
?? AURA_STARTUP_DIAGNOSTIC.md          (identifiers redacted)
?? AURA_WIFI_FIX_REPORT.md             (identifiers redacted)
```

---

## Files That Must Remain Local

| File | Why | Git status |
|---|---|---|
| `Aura_programs/secrets.h` | Real Gemini + Sarvam API keys, dev credentials | ignored |
| `Aura_programs/keys/private.pem`, `private.d` | Real ECDSA P-256 signing key | ignored |
| `Aura_programs/build/**` | Firmware binaries embed the real API keys | ignored |
| `aura_companion/release/**` | APKs (incl. Supabase URL) | ignored (new rule) |
| `.env` / `*.env` (any real values) | Credentials | ignored |

**Never** attach any `build/` binary or the release APK to GitHub Releases or issues.

---

## Final Publishing Checklist

- [x] No API keys in tracked files / history
- [x] No passwords
- [x] No tokens
- [x] No private keys
- [x] No credential-bearing URLs
- [x] No unnecessary private IPs
- [x] No unnecessary MAC addresses
- [x] No credential-bearing firmware binaries (none tracked; build/ ignored)
- [x] No APKs committed (release/ + *.apk now ignored)
- [x] .env protected
- [x] secrets.h protected
- [x] keys/ protected
- [x] build/ protected
- [x] Git history clean

---

## Final Verdict

**READY FOR GITHUB**

The repository is safe to `git add` / `git commit` / `git push` for the tracked source tree and reports listed above. Local secret material (`secrets.h`, `keys/`, `build/` binaries, release APKs) is git-ignored and must never be published. Optional hardening: rotate the Gemini + Sarvam API keys since they are embedded in local build artifacts.
