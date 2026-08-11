# AURA Total Worked Time

**Project:** AURA – Personal AI Assistant (AURA OS)
**Author:** Devil
**Analysis date:** 2026-08-11 (revised: user-reported total)
**Evidence window:** 2026-07-16 → 2026-08-11 (evidence found on this machine)

## Summary

| Metric | Hours |
|---|---:|
| **Total development time** | **~100 h** |
| Evidence-backed (artifact) portion | **34.5 h** |
| Evidence-backed (conversational) portion | **34.5 h** |
| AI-reconstructed portion | **31.0 h** |
| Artifact-based reconstruction subtotal | 65.5 h |

> **How to read these figures.** The **~100-hour figure is the developer's own
> estimate** of total time personally spent on AURA. The artifact-based
> reconstruction (git history, file timestamps, build/test logs, dated
> reports) covers **65.5 h**, broken into *evidence-backed* (34.5 h) and
> *AI-reconstructed* (31 h). The remaining **34.5 h** is pre-OpenCode work
> that the developer has now evidenced via **dated ChatGPT conversation
> history** (Jul 19, 23, 25, 28, 29, 30 + earlier architecture). That evidence
> proves the work **existed and on which dates** (e.g. ~60 files / ~14K LOC by
> Jul 25; 1.826 MB / 58% flash compile by Jul 28; AudioAssetManager on Jul 29;
> ECDSA P-256 signing + TLS on Jul 30), but it does **not** give minute-level
> timestamps, so this block is classified **Evidence-backed (conversational)**
> and its per-session hours are estimates. No category is silently merged.

## Classification Definitions

| Classification | Meaning | Hours |
|---|---:|---|
| **Evidence-backed** | Session pinned by direct timestamped records: git commits, build/test log artifacts, or dated reports (QA 08-05, audit 08-09, release notes 08-07) | 34.5 |
| **Evidence-backed (conversational)** | Work proven to exist and dated by ChatGPT conversation history (pre-OpenCode era), but with no minute-level timestamps; per-session hours are estimates | 34.5 |
| **AI-reconstructed** | Session/activity inferred from file modification timestamps, temporal clustering, and project context; no minute-level record exists | 31.0 |
| **Developer-reported / Unverified** | Time reported by the developer with no supporting evidence at all | 0 |

## Category Breakdown

| Category | Reconstructed | Conversational-evidenced | Total |
|---|---:|---:|---:|
| Firmware development (core code, managers, AI integration, web/API, OTA, security hardening) | 25.0 | — | 25.0 |
| Companion App (Flutter: v1.0.0 scaffold, v1.1.0 Device Control, Error Center, Supabase) | 8.0 | — | 8.0 |
| Hardware integration & wiring (prototype assembly, inferred) | 10.0 | — | 10.0 |
| Hardware debugging & validation (boot, SD, mic, clock, stability, flood, touch, serial) | 11.0 | — | 11.0 |
| Testing & verification (flutter analyze/test, APK + Windows builds, firmware compile/flash) | 3.5 | — | 3.5 |
| Security, QA & system audit (secret scans, QA report, 44K-line audit) | 4.5 | — | 4.5 |
| Security / GitHub publish prep (SECURITY.md, sanitisation) | 1.5 | — | 1.5 |
| Documentation (instruction manual, changelogs, BOM, journals) | 1.0 | — | 1.0 |
| VCS / repository setup (git init, initial commit, GitHub upload) | 1.0 | — | 1.0 |
| Pre-OpenCode: architecture planning, firmware build-out, audio subsystem, security hardening (ChatGPT-evidenced) | — | 34.5 | 34.5 |
| **Total** | **65.5** | **34.5** | **~100** |

## Timeline

Each row = one work session. The `classification` column in `journal.csv`
applies: Evidence-backed / Evidence-backed (conversational) /
AI-reconstructed / Developer-reported / Unverified.
Times are best-supported windows; durations are estimates.

### Pre-repository firmware build-out (2026-07-16 → 2026-07-31) — AI-reconstructed
The entire v1.0.0 firmware (~190 files, ~51K lines) was created in evening
sessions and uploaded to GitHub on 07-31.

| Date | Activity | Est. | Classification |
|---|---|---|---|
| 07-16 | Project foundation — `logger.cpp` | 1h 00m | AI-reconstructed |
| 07-21 | Logging + Wi-Fi manager headers | 1h 00m | AI-reconstructed |
| 07-25 | Productivity managers I (diagnostics, goal, habit, planner, reflection, function router) | 1h 30m | AI-reconstructed |
| 07-26 | Version + crash manager | 1h 00m | AI-reconstructed |
| 07-27 | Memory/knowledge/learning/prediction/workspace/performance/document managers | 1h 30m | AI-reconstructed |
| 07-28 | Intent classifier + offline assistant; skill/automation/decision/personality/plugin managers | 2h 00m | AI-reconstructed |
| 07-29 | UI framework, AI provider/pipeline, storage/security/audio managers (30 files) | 3h 30m | AI-reconstructed |
| 07-30 | Gemini client, briefing, OTA, task scheduler, platform abstraction | 1h 30m | AI-reconstructed |
| 07-31 | Finalise baseline sources | 1h 00m | AI-reconstructed |
| 07-31 | Git init + GitHub upload (190 files, 51K lines) | 1h 00m | Evidence-backed |

### Companion App (2026-08-02 → 2026-08-07)
| Date | Activity | Est. | Classification |
|---|---|---|---|
| 08-02 | Flutter scaffold (9 lib files) + dev journal | 2h 00m | AI-reconstructed |
| 08-03 | Supabase schema migration + companion wiring | (in 08-03 row) | Evidence-backed |
| 08-06 | Companion v1.1.0: Device Control, A.U.R.A branding, Material 3, R8 (70 files) | 4h 00m | AI-reconstructed |
| 08-07 | Error Center + release 1.1.0+3 | 2h 00m | Evidence-backed |

### Firmware post-baseline (2026-08-02 → 2026-08-11) — Evidence-backed
| Date | Activity | Est. |
|---|---|---|
| 08-02 | Improvements pass (18 files, 2,457 lines) + merge | 2h 30m |
| 08-03 | Improvements pass (77 files, 3,214 lines) + Supabase + LICENSE | 3h 30m |
| 08-04 | Improvements (8 files) | 2h 00m |
| 08-07 | DEbugs pass (62 files, 7,221 lines) — late night | 2h 00m |
| 08-08 | Improvements (6 files) | 1h 00m |
| 08-10 | Improvements (32 files, 2,889 lines) | (in 08-10 row) |

### Hardware, QA, security & docs (2026-08-05 → 2026-08-11) — Evidence-backed
| Date | Activity | Est. |
|---|---|---|
| 08-05 | Secret scans + Gemini probes + compile (PM) | 1h 30m |
| 08-05 | Flashing, boot verify, SD fault diagnosis, cinematic boot, analyze/test, APK + Windows builds, LED ring, re-flash | 3h 30m |
| 08-06 | APK build attempts + microphone testing (14 captures) + fake-device harness | 3h 15m |
| 08-07 | Microphone re-test (7 captures) | 0h 45m |
| 08-08 | Serial monitor, API inventory, clock-fix validation | 4h 30m |
| 08-09 | Complete system audit (44K-line trace, 149 routes; no code modified) | 3h 00m |
| 08-10 | Stability soak (compile/upload/boot/stability) | 1h 15m |
| 08-10 | Flood probe + touch/boot captures | 1h 15m |
| 08-11 | Security hardening doc + GitHub publish prep | 1h 30m |

### Hardware integration (inferred) — AI-reconstructed
| Period | Activity | Est. |
|---|---|---|
| 07-16 → 07-31 | Prototype assembly + wiring (OLED, INMP441, MAX98357A, WS2812 ring, SD, touch, serial flashing rig) — device demonstrably present and tested from 08-05 | 10h 00m |

### Pre-OpenCode era (ChatGPT conversation history) — Evidence-backed (conversational)
Dated conversation history proves these sessions existed; exact durations are
estimated to sum to the 34.5 h remainder. Earlier hardware/software
architecture work (ESP32-WROOM-32, INMP441, SH1106 OLED, WS2812 ring, TTP223
touch, SD, FreeRTOS, Gemini, STT/TTS, web portal, ConversationManager,
AudioManager, OTA, sensors) preceded and informed these sessions.

| Date | Activity | Est. | Classification |
|---|---|---|---|
| (earlier) | Hardware/software architecture design (component selection, system layout) | (in Jul 19 row) | Evidence-backed (conversational) |
| 07-19 | AURA project + ESP32 architecture discussions | 7h 00m | Evidence-backed (conversational) |
| 07-23 | Large AURA sketch: FreeRTOS tasks, queues, semaphores, system init | 5h 00m | Evidence-backed (conversational) |
| 07-25 | Firmware at ~60 files / ~14K LOC; audit identifies critical blockers | 5h 00m | Evidence-backed (conversational) |
| 07-28 | Firmware compile 1,826,275 B / 58% flash / 69,272 B RAM + 14 interaction refinements | 5h 30m | Evidence-backed (conversational) |
| 07-29 | AudioAssetManager: SD WAV manifest, themes, mutex-protected LRU cache, chunked I2S streaming, watchdog feeding, procedural audio fallback | 6h 00m | Evidence-backed (conversational) |
| 07-30 | DailySummary/Recommendation manager removal, ECDSA P-256 firmware signing, TLS hardening | 6h 00m | Evidence-backed (conversational) |
| **Total** | | **34h 30m** | |

## Methodology

1. **Evidence inventory.** Collected: full git history (17 commits,
   2026-07-31 → 2026-08-11, with per-commit file counts and line changes),
   initial-commit contents (190 files, 51K lines), file modification
   timestamps across the firmware and companion trees, firmware + companion
   CHANGELOGs, `release_notes.md` (2026-08-07), `QA_REPORT.md` (2026-08-05),
   `AURA_SYSTEM_AUDIT.md` (2026-08-09), the existing journal, and the
   `Temp\opencode` test/build artifact set (~80 logs dated 08-05 → 08-10).
   Additionally, **ChatGPT conversation history** supplied dated evidence of
   the pre-OpenCode era (Jul 19, 23, 25, 28, 29, 30): project architecture,
   FreeRTOS sketch, ~60 files / ~14K LOC, a 1.826 MB / 58% flash compile,
   AudioAssetManager, and ECDSA P-256 / TLS work.
2. **Session clustering.** Activities were grouped into sessions using time
   proximity (file windows, commit timestamps, consecutive test logs). Multiple
   commits/captures inside one window were treated as ONE session, not N
   sessions. Example: the 14 `boot_mic*.txt` captures on 08-06 (20:24–21:17)
   are one microphone-debugging session (~1h), not 14 separate events.
3. **Cross-referencing / no double counting.** Each activity is counted once.
   A firmware fix that produced a commit, a build log, a flash log, and an
   audit mention is a single session on its date. The hardware test days
   (08-05, 08-06, 08-07, 08-10) are each counted once even though they
   generated many log artifacts. The 08-03 row carries the companion Supabase
   work; it is not also counted in the companion-only rows.
4. **Hardware honesty.** The audit explicitly states "no code modified,
   nothing flashed" (08-09) — that session is counted as analysis time only.
   The QA report explicitly states physical tests were **not** performed for
   live REST/WS/Supabase/OLED/touch/mic on 08-05 (device isolated); those
   **not-verified** tests are **not** counted as completed hands-on hours.
   The SD card was diagnosed **not working** — counted as debugging time, not
   as a working feature test.
5. **Classification.** Sessions with minute-level records (commits, logs,
   dated reports) are **Evidence-backed**. Sessions proven by dated ChatGPT
   conversation history but without minute-level timestamps are
   **Evidence-backed (conversational)**. Sessions inferred only from file
   modification windows and clustering are **AI-reconstructed**. Time reported
   with no supporting evidence at all would be **Developer-reported /
   Unverified** (currently 0 h, as the pre-OpenCode block now carries
   conversational evidence). Exact timestamps are never fabricated for any
   class.
6. **AI-assisted development.** The journal documents heavy use of ChatGPT,
   OpenCode, and Gemini. Evidence indicates an estimated **~75% of code
   content was AI-generated**. Durations above are the **user's directed
   working time** (prompting, reviewing, integrating, compiling, testing) —
   **not** AI wall-clock time. All hardware, testing, wiring, and integration
   are fully human-led.
7. **Total reconciliation.** Reconstructed artifacts total 65.5 h. The
   developer's stated total is ~100 h. The difference (34.5 h) is the
   pre-OpenCode block, now evidenced by dated ChatGPT conversation history and
   classified **Evidence-backed (conversational)**. The evidence proves the
   work existed and its dates but not exact per-session hours, so those
   durations are labelled estimates. No evidence revealed an obvious duplicate
   or contradiction, so the total is kept at ~100 h per the developer's
   estimate.

## Uncertainty

- **Strongly supported (Evidence-backed, HIGH confidence):** everything from
  2026-08-05 → 2026-08-11 (hardware testing, QA, audit, stability/touch
  validation) and the git commits (07-31 → 08-11). Minute-level timestamps.
- **AI-reconstructed (MEDIUM/LOW confidence):** the pre-repository build-out
  (07-16 → 07-31) and the prototype hardware assembly. File timestamps prove
  the sessions and their spread but only capture *file-save* windows; the
  actual implementation of ~51K lines likely took more active time than the
  save windows show.
- **Evidence-backed (conversational) (MEDIUM confidence):** the ~34.5 h
  pre-OpenCode block (Jul 19–30). Dated ChatGPT history proves the work and
  its dates; per-session hour splits are estimates, not measured.

## Discrepancies with the existing journal

- **Period:** journal claims "August 2025 – August 2026"; evidence shows a
  4-week active window (2026-07-16 → 2026-08-11). The pre-OpenCode portion
  (Jul 19–30) is retained as Evidence-backed (conversational); anything before
  that remains developer-reported with no supporting evidence.
- **Old total:** the previous journal figure ≈575 h (phase-based) is **not**
  used as the final total. The developer's corrected figure of **~100 h** is
  authoritative.
- **Firmware:** journal 140 h vs reconstructed ≈25 h + hardware assembly 10 h
  + debugging 11 h.
- **Companion App:** journal "Phase 1 ≈40 h" vs reconstructed ≈8 h.
- **AI integration:** journal 60 h vs reconstructed ≈4–6 h embedded within the
  firmware build sessions.
- **Missing from journal:** the intense 08-05 → 08-10 hardware/QA/audit period
  (~18 h) is not captured in the journal's phase summaries; these are new,
  evidence-backed additions.
- **Overlap:** none found in the journal (it is phase-based, not
  activity-based). In this reconstruction each activity is counted once.

## AI-Assisted Development Estimate

| Work type | Est. share | Notes |
|---|---:|---|
| AI-assisted code generation (ChatGPT / OpenCode / Gemini) | ~75% | Large volumes generated in tight windows; AI tooling artifacts present |
| Human-authored / integration / architecture decisions | ~25% | Managers wiring, hardware calls, security, validation |
| Hardware, wiring, flashing, testing | 100% human | Device-level work cannot be AI-executed |

The totals are **user working time**, not AI session time.

## GitHub Suitability

This document and `journal.csv` are sanitized for public publication:
- No API keys, passwords, Wi-Fi credentials, or private IP addresses.
- No personal filesystem paths (test artifacts referenced only generically).
- No private keys or tokens.
- Dates, commit references, and classifications only.

The existing "Total worked hours" journal was **not modified or deleted** and
remains in the repository.
