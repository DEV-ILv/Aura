# AURA Development Journal

Engineering journal for the AURA personal AI assistant firmware (ESP32). This file records the
audit/hardening phases completed through **Phase 10**. Companion record: `journal.csv` (session log
plus phase rows).

## Project Status

The AURA firmware compiles cleanly (0 errors / 0 warnings) under the `huge_app` partition scheme on
ESP32 core 3.3.11. Ten audit/hardening phases (1, 2, 3, 6, 7, 8, 9, 10) have been completed, covering
boot reliability, self-recovery, concurrency, heap safety, Wi-Fi lifecycle, audio/STT, storage and
long-run runtime behavior. Hardware verification for the phase audits remains **blocked** — the ESP32
was not connected — so all phase-level results are compile-verified only, unless explicitly stated
otherwise. No firmware source was modified while preparing this journal.

## Phase 1

Initial firmware/system implementation and stabilization.

## Phase 2

Self-recovery implementation.

- Centralized `HealthManager` (`health_manager.cpp/.h`).
- Recovery state machine: `detect -> reportFailure -> recover -> reportHealthy`.
- Bounded recovery attempts and cooldowns.
- Optional subsystems must not reboot the device.

## Phase 3

Boot reliability and persistent-state hardening.

Fixes:
- Safe-mode boot-loop handling.
- Centralized `SystemManager::requestRestart()`.
- Persistent restart reasons.
- Settings flushing before restart.
- Durable configuration version.
- Checked crash-counter writes.
- Fail-closed rejection of unsigned web OTA updates.
- All restart paths routed through `SystemManager`.
- Optional subsystem failures cannot cause reboot.

OTA limitation: the current `huge_app` partition has a single OTA slot, so OTA installation/rollback is
not functional in this build.

## Phase 6

Long-run runtime and concurrency hardening.

Fixes:
- SD remount watchdog pre-feed.
- Dirty-save retry throttling.
- ESP-NOW logging moved outside the spinlock.
- Loop-task stack high-water-mark monitoring.
- Mutex/deadlock audit.
- Watchdog audit.

Hardware verification remained blocked because the ESP32 was not connected.

## Phase 7

High-risk runtime hardening.

Fixes:
- ESP-NOW spinlock cleanup.
- Heap allocation and event publication moved outside critical sections.
- Software-restart boot-loop blind spot handling.
- Content-aware reserves for large JSON serializers.
- Heap-floor guards.
- Gemini response/body hard caps.

Build remained compile-clean.

## Phase 8

Concurrency, heap, display, audio/STT and Wi-Fi hardening.

- **P0:** EventBus publish/update race fixed using a FreeRTOS mutex.
- **P1:** ESP-NOW driver call removed from the spinlock; MemoryManager heap safety improved;
  STT short-burst LISTENING wedge fixed.
- **P2:** OLED runtime responsiveness detection and recovery added.
- **P3:** SH1106 comment corrected.
- **Wi-Fi:** ERROR state now retries STA before AP fallback.

## Phase 9

Wi-Fi lifecycle reliability audit.

Important fixes identified:
- External `reconnect()` could reset the retry budget and bypass provisioning/AP fallback.
- NTP synchronization was missing on the actual CONNECTED transition path.
- `m_connecting` could remain `true`.
- Re-provisioning while already connected could fail because the old STA connection pre-empted
  `WAITING_TO_CONNECT`.

Fixes applied:
- Hardened `reconnect()` state guards.
- Centralized `handleConnected()`.
- NTP sync now occurs on every actual connection transition.
- Provisioning guards prevent stale connection events from interrupting re-provisioning.
- Provisioning flag reset correctly during disconnect.

Wi-Fi state machine: `DISCONNECTED`, `CONNECTING`, `CONNECTED`, `ACCESS_POINT`,
`WAITING_TO_CONNECT`, `ERROR`.

Security:
- AP password resolution centralized.
- No Wi-Fi password logging.

Build: Phase 9 compiled successfully with 0 errors and 0 warnings.

## Phase 10

Production-hardening audit.

Areas audited: Audio/I2S, Mic/STT, TTS/playback, SD/storage, OLED/I2C, heap, FreeRTOS, long-run
runtime, power-loss behavior, HealthManager, watchdog.

Confirmed fixes:
1. **CONV-01** — LISTENING state timeout now explicitly cancels STT and stops the microphone before
   entering ERROR.
2. **CONV-02** — STT continuous utterances now have a 60-second maximum duration.
3. **CONV-03** — Starting a new STT recognition session tears down stale PROCESSING/streaming state.
4. **HEAP-01** — Gemini streamed response buffer now reserves the maximum response capacity once
   rather than repeatedly reallocating per SSE chunk.

Build: 0 errors, 0 warnings.

## Current Firmware Metrics

Latest verified metrics at the end of Phase 10:

| Metric | Value |
|---|---|
| Flash usage | 2,019,675 B / 3,145,728 B (64%) |
| RAM / globals | 122,976 B / 327,680 B (37%) |
| Compile status | 0 errors / 0 warnings |
| Partition scheme | `huge_app` |
| ESP32 core | 3.3.11 |

Reference: baseline after Phase 9 was Flash 2,019,459 B and RAM 122,976 B. The Phase 10 delta was
+216 B flash, +0 B RAM.

## Remaining Risks

The following risks were documented after Phase 10. None of them were resolved as part of this
journaling task; they remain open.

1. **Audio-01 — severity P1** — Mic-stall recovery may be ineffective because `recoverMicrophone()`
   currently calls `startRecording()`, which may return immediately if already recording.
   *Why it remains:* changing the I2S recovery approach is invasive and was intentionally gated
   behind hardware validation. *Hardware verification required:* Yes.
2. **Audio-02 — severity P2** — `calibrateNoiseFloor()` can potentially spin if the microphone is
   dead. *Why it remains:* a wall-clock timeout is a possible future fix but needs reproduction on
   hardware first. *Hardware verification required:* Yes.
3. **SD-01 — severity P1** — Boot temp-file recovery has an edge case where a valid original could
   potentially be replaced when a zero-byte temporary file exists. *Why it remains:* the recovery
   logic change needs power-loss testing. *Hardware verification required:* Yes.
4. **SD-02 — severity P1** — No SD orphan-temp recovery mechanism exists for interrupted writes.
   *Why it remains:* not implemented; needs power-loss/removal testing. *Hardware verification
   required:* Yes.
5. **OLED-02 — severity P3** — OLED refresh is approximately 30 FPS and can consume I2C bandwidth.
   *Why it remains:* optional optimization; gating full redraw during STT capture is proposed.
   *Hardware verification required:* No.
6. **WF-5 — severity P3** — Unreachable/no-credential ERROR retry branch. *Why it remains:*
   defensive branch, low impact. *Hardware verification required:* No.
7. **WF-6 — severity P3** — Cosmetic `WAITING_TO_CONNECT` state-string issue. *Why it remains:*
   cosmetic only, no functional impact. *Hardware verification required:* No.
8. **WF-7 — severity P2** — LOW_POWER Wi-Fi reconnect timing is a design question. *Why it remains:*
   design decision pending. *Hardware verification required:* Yes.
9. **WF-8 — severity P3** — Wi-Fi scanning can briefly disrupt the STA connection. *Why it remains:*
   platform behavior; optional mitigation. *Hardware verification required:* Yes.
10. **WF-9 — severity P3** — mDNS does not explicitly register a service and relies on
    library-managed hostname behavior. *Why it remains:* platform behavior, low impact.
    *Hardware verification required:* No.

## Hardware Verification Status

Hardware verification was **blocked** for the phase audits because the ESP32 was not connected.
Therefore:

- All Phase 6 through Phase 10 results are compile-verified only; no hardware tests were performed
  for these phases.
- No phase in this journal is marked as hardware-verified.
- Tests that were not actually performed remain marked as unverified. Nothing here should be read as
  a claim that hardware testing occurred.
- Earlier hands-on hardware sessions (flashing, microphone, SD-card fault diagnosis, touch probes,
  stability soak) are recorded separately in the session rows of `journal.csv` (August 2026) and are
  outside the scope of the phase-audit records.

## Next Recommended Work

Prioritized open work as of the Phase 10 state. **No fix is implemented by this journal task.**

**P0**
- None identified at the Phase 10 state.

**P1**
- Audio-01: make mic-stall recovery effective (requires hardware validation of the I2S path).
- SD-01: harden boot temp-file recovery so a valid original can never be replaced by a zero-byte
  temporary file.
- SD-02: add SD orphan-temp recovery for interrupted writes.

**P2**
- Audio-02: add a wall-clock timeout to `calibrateNoiseFloor()`.
- WF-7: decide and document LOW_POWER Wi-Fi reconnect timing.

**P3 / optional**
- WF-6: fix the cosmetic `WAITING_TO_CONNECT` state string.
- OLED-02: gate full OLED redraw during STT capture.
- WF-5: review the unreachable/no-credential ERROR retry branch.
- WF-8: evaluate Wi-Fi scan disruption mitigation.
- WF-9: consider explicit mDNS service registration.
