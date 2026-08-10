# AURA — Complete User Instruction Manual

**Firmware:** AURA 1.0.0 · Mark III "Phoenix" · Development channel  
**Platform:** ESP32-WROOM-32 · Arduino-ESP32 core  
**Companion app:** AURA Companion 1.1.0 (Android / Windows)  
**Document version:** 1.0 — generated from the current codebase (source of truth)

---

## How to Read This Manual

This manual describes exactly what the **current** AURA firmware and companion app
actually do, verified from the source code. Three markers are used throughout:

| Marker | Meaning |
|---|---|
| Ready | Implemented and working in this build |
| Requires configuration | Code exists but needs a credential, TLS certificate, or hardware to produce results |
| Not currently available | Code path exists but is not wired up, or the hardware / API is absent |

**Source-of-truth rule.** Where this manual conflicts with any earlier README or
marketing material, the *current code* wins.

---

## Table of Contents

1. What AURA Is
2. Capability Summary
3. Hardware Overview
4. Power-On, Startup, Restart, Shut Down
5. Touch Sensor — Complete Guide
6. Microphone Guide
7. OLED Display Guide
8. LED Ring Guide (Status and Disco Mode are separate)
9. Voice Pipeline
10. Sarvam Speech-to-Text / Text-to-Speech
11. Gemini AI and the Offline Local AI Engine
12. The AURA Companion App
13. Local vs Remote Mode
14. Setup Mode (Wi-Fi Provisioning)
15. Privacy / Microphone Mute
16. Storage and the microSD Card
17. OTA Firmware Updates
18. Sensors
19. Companion App Tools
20. Status Reference (Master Table)
21. Troubleshooting
22. Safety
23. Developer / Advanced Section
24. FAQ

## 1. What AURA Is

AURA is a small, personal, **voice-first AI assistant** built on a single low-cost
ESP32 microcontroller. It sits on your desk, shows its mood through a miniature
OLED "face" and a 16-LED ring, listens through a microphone, and answers through a
speaker.

AURA handles the entire loop on one chip:

- **Hear** — an INMP441 I2S MEMS microphone captures your voice.
- **Think** — speech-to-text turns your words into text; a reasoning engine
  produces an answer. When the network is unavailable, a fully **offline Local AI
  Engine** answers instead.
- **Speak** — text-to-speech converts the answer into audio for an amplifier and
  speaker.
- **Show** — a 128×64 monochrome OLED renders the idle "face", a clock, and every
  state; the 16-LED ring uses a single distinct solid colour per state to
  communicate the same state at a glance.
- **Connect** — Wi-Fi, an embedded web configuration portal, and the AURA
  Companion app for phone and PC.

AURA also includes a device-side productivity suite: reminders, goals, habits, a
planner, study tools, daily briefings, a knowledge graph, documents, workspaces,
semantic search, decisions, predictions, and recommendations. These store their
data on the ESP32's on-chip filesystem and are managed through the web portal or
the companion app.

> **Platform note.** AURA runs on a very small microcontroller (≈3 MB app flash,
> ≈320 KB RAM, no PSRAM). That constraint, plus hardware and API availability, is
> why some features in this manual are marked *Requires configuration* or *Not
> currently available*.

---

## 2. Capability Summary

| Capability | Status | Notes |
|---|---|---|
| Touch interaction (single tap, double tap, 5 s hold) | **Ready** | See §5 |
| Microphone capture (INMP441, I2S, 16 kHz) | **Ready** | See §6 |
| Voice wake word (always-listening mic) | **Not currently available** | Wake-word code exists but is not wired; touch (push-to-talk) is the only input |
| Speech-to-Text (Sarvam AI, cloud) | **Requires configuration** | Needs a Sarvam API key and TLS root CA in `secrets.h` (empty by default) |
| Text-to-Speech (Sarvam AI, cloud) | **Requires configuration** | Same requirements as STT |
| Speaker / spoken reply (MAX98357A) | **Hardware-dependent** | Pins are defined, but the shipped build is compiled with the speaker **not present** |
| Gemini reasoning (cloud) | **Not currently available** | Model constant exists but the API endpoint is not wired in this build; the offline engine is used |
| Offline Local AI Engine | **Ready** | Fully offline, always available |
| Startup greeting | **Ready** | Configurable, ~4 s display |
| OLED display (SH1106, 128×64) | **Ready** | Face, clock, states |
| LED status ring (WS2812B, 16 LEDs) | **Ready** | See §8 |
| Disco Mode (party lights) | **Ready (app-only)** | Fully separate from status; see §8 |
| Privacy / mic mute | **Ready** | App + OLED + magenta ring |
| Wi-Fi STA + soft-AP + captive portal | **Ready** | See §14 |
| NTP time sync | **Ready** | Powers the idle clock |
| Web configuration portal | **Ready** | See §14, §23 |
| Embedded REST API (100+ endpoints) | **Ready** | See §23 |
| OTA firmware updates | **Ready** | Web upload; see §17 |
| microSD card storage | **Requires hardware** | Card is optional; on-chip SPIFFS is the default store |
| Sensors (BME280, BH1750, IMU, GPS, etc.) | **Not currently available** | No sensor modules exist in this firmware build |
| Battery gauge | **Not currently available** | No battery-sense code; the app shows a placeholder |
| Companion app | **Ready** | See §12 |

---

## 3. Hardware Overview

The table lists the hardware actually supported by the current firmware, with pins
verified from `config.h`.

| Component | Function | Connection / pins | User-visible purpose |
|---|---|---|---|
| ESP32-WROOM-32 (38-pin) | Main MCU (Wi-Fi + BLE) | — | All processing and wireless |
| SH1106 OLED, 128×64 px | Display | I2C SDA=GPIO21, SCL=GPIO22, addr 0x3C | Face, clock, states |
| INMP441 I2S MEMS microphone | Voice input | BCLK=GPIO26, WS=GPIO25, DATA=GPIO34 | Captures your speech |
| MAX98357A I2S amp + speaker | Voice output | BCLK=GPIO27, LRC=GPIO14, DATA=GPIO12 | Speaks replies (hardware-dependent) |
| WS2812B 16-LED ring | Status lights | WS2812 on GPIO4 | Solid status colours |
| TTP223 capacitive touch | Input | GPIO13, active-high | Tap / double-tap / hold |
| microSD / SD module | Optional storage | CS=GPIO5, MOSI=GPIO23, MISO=GPIO19, SCK=GPIO18 | Optional data and asset storage |
| Wi-Fi + Bluetooth (integrated) | Networking | onboard | Connection, app, cloud |

> **Speaker note.** The firmware is compiled with `AURA_HW_SPEAKER_PRESENT = 0`.
> The I2S speaker stage is disabled and boot logs "Speaker output skipped (not
> connected)". On this unit you will **not** hear spoken replies until a MAX98357A
> (or equivalent) is connected and the build flag enabled. Microphone, OLED, LED
> ring, touch, Wi-Fi, and storage are unaffected.

> **Not in this firmware (parts-list only):** a 5" touchscreen, GPS, NFC, camera,
> RTC module, IMU, distance sensor, ambient-light sensor, temperature / humidity /
> pressure sensor. None of these have firmware support in the current build and
> are not documented here as working functions.

## 4. Power-On, Startup, Restart, Shut Down

### 4.1 What happens when you apply power

1. **Connect 5 V / USB-C power** (or a battery, if fitted).
2. The ESP32 boots; a serial console at **115 200 baud** shows progress if a
   computer is connected.
3. **System Manager** initialises the ~57 modules in dependency order — storage,
   display, Wi-Fi, UI framework, audio, LED ring, AuraSystem, conversation,
   memory, planners, OTA. This typically finishes in a few seconds.
4. **OLED:** boot screen — a small logo with a pulsing ring, the text "AURA", a
   progress bar, then **READY**.
5. **LED ring:** solid blue during boot, then the **idle** indication (solid
   blue with a subtle flicker).
6. **Network:** if saved Wi-Fi credentials exist, AURA joins that network (STA);
   otherwise it starts a soft-AP named `AURA_Setup` so you can provision it. On
   success it syncs time from NTP.
7. **READY.** The OLED shows the idle face (and clock once time is synced); the
   ring shows the solid-blue idle light.
8. Optional **startup greeting** is displayed for about 4 seconds (unless disabled).

### 4.2 What you should see

The OLED completes the boot animation and shows the face; the ring shows the idle
solid-blue light; the unit responds to a single tap. No permanent error text, no
reboot loops.

### 4.3 If it does not boot

- Check a stable **5 V** supply and a good USB-C cable.
- Unplug power, wait ~5 seconds, reconnect.
- Watch the serial console. Repeated `rst:` lines or "Guru Meditation" indicate a
  crash loop. The firmware counts failed boots and then enters **Safe Mode** —
  a reduced load state that lets you reach the web portal, review crash logs, and
  recover.
- If serial boots fine but the OLED stays blank, check OLED wiring / I2C address
  (§21).

### 4.4 Restart and power off

ESP32 firmware has no software power-off; to power off simply remove power. To
restart without unplugging:

- **Web portal:** Restart button (or `POST /api/restart`).
- **Companion app:** Control → Restart device.

**Factory reset** wipes device storage (settings, Wi-Fi, memories, reminders,
goals, habits, secrets) back to defaults. Use `POST /api/factory-reset` or the app
Control → Factory reset (both confirm first). After a reset you must re-provision
Wi-Fi and sign in again.

> **Warnings**
> - Never remove power during an OTA update (§17).
> - A factory reset is **irreversible**.

---

## 5. Touch Sensor — Complete Guide

The touch pad (TTP223, active-high) is the only physical input. AURA polls it every
~20 ms and applies debounce and timing so taps, double taps, and holds are
distinguishable.

### 5.1 Gesture grammar

| Gesture | Timing | Result |
|---|---|---|
| **Single tap** | pressed ~50–400 ms, released; no second tap within 400 ms | Start a conversation — microphone ON (listening) |
| **Double tap** | two taps, second within 400 ms | Cancel / stop the active interaction → return to idle |
| **5-second hold** | press held 5000 ms without release | Enter Setup Mode (§14) — highest priority |
| Any other hold (>400 ms but <5 s) | press then release | **Ignored** — deliberately nothing |

Priority on a single press: **5 s hold > double tap > single tap**.

### 5.2 Behaviour in context

- **Idle:** single tap opens the mic and starts a conversation.
- **During listening / recording / processing / speaking:** single tap is ignored;
  **double tap cancels** the interaction and returns to IDLE.
- **Privacy / muted:** touch **never** opens the mic; the display shows
  "Microphone Muted".
- **Setup mode:** gestures are disabled except a further 5 s hold, which exits
  setup.
- **Auto-sleep:** any touch wakes AURA; a 5 s hold goes straight into setup.

In short, **"tap once to talk, tap twice to stop, hold five seconds for setup".**
There is no wake-word.

## 6. Microphone Guide

### 6.1 What the microphone is

The INMP441 digital microphone (I2S, 16 kHz, 16-bit mono) captures your voice so it
can be transcribed. By default it is **push-to-talk**:

- **MIC INITIALIZED** — the driver is created and pins configured at boot.
- **MIC ACTIVELY LISTENING / RECORDING** — only while a conversation is active
  (after a single tap).

The mic is **not always-on**. At boot the firmware stops recording and does not
enable voice-activity signals, so no ambient sound is captured until you tap.

### 6.2 How to activate it

**Single tap** the touch sensor. AURA opens the mic and the OLED/LED switch to the
listening state.

### 6.3 How to stop it

- **Double tap** anywhere stops the interaction and returns to IDLE.
- Otherwise the capture stops automatically once you stop speaking (silence end),
  then STT → AI → TTS complete the turn.

### 6.4 Knowing what the mic is doing

| Signal | Meaning |
|---|---|
| OLED label "Listening…" | Mic is capturing |
| LED ring solid cyan (then green while recording) | Mic is recording |
| Mic icon active | Mic active |
| Mic icon muted / OLED "Microphone Muted" | Mic is muted |
| LED **solid magenta** | Muted / privacy |

### 6.5 Privacy / mute

- Mute is controlled from the companion app or the web portal.
- While muted: recording is prevented, voice-activity detection is off, no audio is
  sent; the OLED shows the muted notice and the ring is solid magenta.
- Unmute restores voice behaviour.

### 6.6 Wake word — Not currently available

A wake-word handler exists in the conversation manager but nothing invokes it in
this build. **Push-to-talk via touch is the only input path.** Wake-word is
documented as not available.

### 6.7 If STT is unavailable

If speech-to-text cannot initialise (missing key/TLS or no network), the voice
turn degrades gracefully. **This is not a sign that the touch sensor is broken** —
always check the LED and OLED to separate a touch failure from an STT failure
(§21).

---

## 7. OLED Display Guide

The display is a **128×64 monochrome SH1106**. Colour is conveyed through the LED
ring, not the screen.

### 7.1 User-visible states

| Screen | Meaning | What to do |
|---|---|---|
| Boot animation + "AURA" + progress + "READY" | Starting up | Wait |
| Idle face + optional clock | Ready and idle | Tap once |
| "Listening…" | Mic is capturing | Speak |
| "Thinking…" | AI is processing | Wait |
| "Speaking…" | Playback running | Wait (needs speaker) |
| "AURA SETUP" + SSID | Setup / provisioning | Join the SSID (§14) |
| "Microphone Muted" | Mute active | Unmute from the app |
| Error / notice screen | A failure state | See §21 |
| OTA screen | Updating | Keep power |

### 7.2 Mic icon and idle clock

- **Mic icon** (top right) reflects mic mode: idle, listening, or muted.
- **Idle clock:** while the face is genuinely idle, the top shows an `HH:MM` clock
  in **India Standard Time (IST, UTC+5:30)**. It appears only when the face
  expression is `IDLE`; any active state replaces it. Until NTP sync, it shows
  `--:--`.

---

## 8. LED Ring Guide (Status and Disco are separate)

There are **two independent LED systems**: the normal status indicator and Disco
Mode. Keep them separate.

### 8.1 Normal status LED (FINAL: solid colour per state)

Each status uses a distinct solid color across the entire LED ring. The ring
may have a subtle synchronized brightness flicker to give AURA a more
alive/futuristic appearance. The whole 16-LED ring always shares one brightness
level, so all LEDs stay in perfect sync while the intensity slightly flickers
(~85–100 % for normal states, ~92–100 % for quiet states, recomputed every
50–150 ms). The error state uses a deeper, faster flicker so it stands out.

| State | Solid colour |
|---|---|
| IDLE / READY | Solid **Blue** `#0080FF` |
| BOOT | Solid **Blue** `#0080FF` (matches IDLE identity) |
| LISTENING | Solid **Cyan** `#00FFFF` |
| RECORDING | Solid **Green** `#00FF40` |
| THINKING / PROCESSING | Solid **Yellow** `#FFFF00` |
| SPEAKING | Solid **White** `#FFFFFF` |
| SETUP | Solid **Purple** `#8000FF` |
| PRIVACY / Muted | Solid **Magenta** `#FF00AA` |
| OTA | Solid **Orange** `#FF8000` |
| Wi‑Fi connecting | Solid **Steel blue** `#4080E0` |
| Wi‑Fi connected | Solid **Spring green** `#00FF80` (transient → IDLE) |
| Happy | Solid **Gold** `#FFD700` |
| Success | Solid **Mint green** `#00FFAA` (transient → IDLE) |
| Reminder / notification | Solid **Amber** `#FFAA00` |
| Warning | Solid **Red-orange** `#FF4000` (error-style flicker) |
| Critical | Solid **Dark red** `#8A0000` |
| Offline | Solid **Slate** `#607085` |
| Sleep / Wake | Solid **Dim navy** `#404A60` / **Azure** `#00AFFF` |

Brightness is adjustable (default ~120). IDLE glows a quiet dim-blue presence
instead of turning off. Colour changes cross-fade crisply between SOLID states.
Privacy / muted states use solid **magenta** (not red).

### 8.2 Disco Mode (separate, app-only)

Disco Mode is a fun, optional **visual override**, turned on from the Companion app
(Tools → Disco Mode) or the portal API.

- Turns the ring into **party lights**: a rotating set of ~10 smooth colours
  (deliberately low-strobe, eye-friendly) at a configurable brightness.
- **Overrides** the normal status animation while enabled.
- **Emergency exception:** a critical error temporarily breaks through with a red
  alarm, then Disco resumes.
- **Not persisted:** Disco is OFF after every boot; enable it again from the app.
- Disco leaves the OLED and the microphone untouched.

Disco is not the normal status indicator and never claims to be.

## 9. Voice Pipeline

```
User → Touch (tap) → Microphone (INMP441) → Voice capture
     → PT (Sarvam, cloud) → Reasoning (Local AI by default; Gemini if configured)
     → TTS (Sarvam, cloud) → Speaker (MAX98357A, hardware-dependent)
```

| Stage | Component | Internet required? | Current state |
|---|---|---|---|
| Input | TTP223 touch | No | Ready |
| Capture | INMP441 I2S mix | No | Ready |
| STT | Sarvam STT (cloud) | Yes | Requires configuration |
| Reasoning | Local AI Engine (offline) | No | Ready (default) |
| Reasoning (optional) | Gemini (cloud) | Yes | Not currently available |
| TTS | Alternative TTS (cloud) | Yes | Requires configuration |
| Output | MAX98357A | No | Hardware-dependent |

**What this means today:** you can always interact by touch, and the assistant can
always answer thanks to the offline engine. For the audible voice conversation you
must supply the speech configuration and the speaker hardware; voice-output pipes
are included in the code, but the shipped build has the speaker off.

## 10. Sarvam Speech-to-Text / Text-to-Speech

### 10.1 What they do

- **STT (Speech-to-Text):** streams the microphone capture (16 kHz WAV) to the
  Sarvam API over HTTPS and returns a transcript. Voice-activity detection
  determines where your speech ends.
- **TTS (Text-to-Speech):** takes the assistant's answer, requests Sarvam, and
  streams audio back (Base64 → PCM at 16 kHz) for playback.

Both are **cloud services**: they require an Internet connection and a valid
account key.

### 10.2 Configuration

Secrets are never committed. A template exists at `secrets.h.example`:

```
copy secrets.h.example secrets.h
# in secrets.h set:
#   SARVAM_API_KEY = "…"     (Sarvam account key)
#   SARVAM_ROOT_CA  = "…"    (TLS root certificate of the API host)
```

- Keys load from NVS at runtime; empty values make initialisation fail closed.
- **TLS is strict:** without a root CA certificate, STT/TTS cannot initialise.
- The language model expects Indian English and Hindi support (see `config.h`).

### 10.3 Failure behaviour

If STT or TTS cannot start (missing key/CA, offline), that stage is skipped and
reported clearly. **An STT failure is never a fault of the touch sensor.**

> Note: some architecture notes call these modules "placeholders"; in the current
> source they are **fully implemented** (WAV upload, TLS, streaming decode). They
> are gated purely on configuration and network.

---

## 11. Reasoning — Gemini (Cloud) and the Local AI Engine

### 11.1 Gemini (cloud)

- Requested model: **`gemini-3.5-flash-lite`** at Google's `generateContent`
  endpoint (see `config.h`).
- Needs a Google API key, TLS root CA, and Wi-Fi; uses a timeout and a circuit
  breaker.
- On any failure (no Wi-Fi, no key, no endpoint, no CA), callers fall back to the
  offline engine.

> **Setup status:** the model string is defined, but the API endpoint is **never
> wired into the client** in this build (`GeminiClient::setApiEndpoint` has no
> callers). Gemini is therefore **Not currently available**, and the assistant
> uses the Local AI Engine. This is offline-first behaviour by design.

### 11.2 Local AI Engine (offline)

The offline engine builds replies entirely on-device, using conversation context,
memories, goals, the planner, personality, time of day, and sentence generation
with a response cache. It is always available, works without any network or API
key, and can be enabled/disabled from the portal (`/api/offline_ai/*`).

## 12. The AURA Companion App

The companion app is a Flutter application for **Android** and **Windows**.

### 12.1 Install and first launch

1. Install / build the app, then launch **AURA Companion**.
2. A splash screen leads to a **sign-in card**, which depends on the connection
   mode:
   - **Local device reached** → device sign-in (username + password). The admin
     password is printed on the serial monitor at first boot (development builds
     prefill `Devil` / `Devil` for local testing only).
   - **Device unreachable** → a "device unavailable" message with a path to
     **cloud sign-in**.
   - **Remote (cloud) mode** → email/password sign-in with Sign Up and Forgot
     Password, via Supabase.

### 12.2 Home navigation

The app shows the main destinations as a rail (wide screens) or bottom bar
(phones):

| Tab | Purpose |
|---|---|
| **Dashboard** | Live device overview — connection badge, module status, resource gauges (CPU, storage, temperature, battery), metric grid (heap, Wi-Fi, uptime) |
| **Chat** | Text chat with the on-device assistant; replies are memory-aware |
| **Tools** | Launchpad for device tools (§19) |
| **Alerts** | Local notification settings and a test-alert button |
| **Settings** | App preferences, speech settings, and (hidden) developer settings |

### 12.3 Connecting the app to the device

- Default addresses: REST `http://<ip>:80`, WebSocket `ws://<ip>:81`; AP fallback
  address `192.168.4.1`; mDNS hostname `aura-<mac>.local`.
- The app automatically probes the device, validates the stored session, and
  routes you to the dashboard when possible.
- Networking details are intentionally hidden behind the **developer settings**
  screen.

### 12.4 Notifications

Alerts and reminders use **local notifications** inside the app (no cloud push
service). The Alerts screen offers: device alerts on/off, alert sound/vibration,
and reminder notifications. A **Test alert** button lets you verify delivery.

---

## 13. Local vs Remote Mode

| | **Local (device)** | **Remote (cloud)** |
|---|---|---|
| What it is | App talks directly to the device | App talks via Supabase |
| When | Device reachable on the LAN | Device out of range |
| Requirements | Same network, device powered | Supabase project + account |
| If unreachable | "Offline" badge | "Offline" / reconnecting |
| Sign-in | Device username/password | Email/password |

- **Local is always preferred.** The app falls back to remote when the direct
  connection is unavailable.
- The connection badge in the app title shows `Cloud` (remote) or `Connected`
  (local), plus transient `Connecting` / `Testing` / `Reconnecting` states.
- If the device itself is offline, neither mode can talk to it; the **Local AI
  still answers** on the device, and touch interaction still works.

---

## 14. Setup Mode (Wi-Fi Provisioning)

Enter Setup by holding the touch pad for **5 seconds**.

1. **Touch and hold 5 s.** AURA enters Setup Mode.
2. **OLED:** shows **AURA SETUP** plus the SSID to join.
3. **LED:** Solid **Purple** (setup indication; magenta when muted during setup).
4. On your phone, join the Wi-Fi network shown on the OLED. The SSID is
   `AURA_Setup` (from `Secrets::AP_SSID`); the password is generated at runtime and
   printed to the serial console unless a placeholder in `secrets.h` is set.
5. Open a browser to **http://192.168.4.1** (or `http://aura-<mac>.local`) — the
   captive portal config page.
6. Enter your home Wi-Fi SSID and password, then save. AURA connects to that
   network and exits setup.
7. To exit setup manually, hold the pad for 5 seconds again.

While in setup the microphone is disabled and touch input is restricted; this
prevents accidental conversations during configuration.

## 15. Privacy / Microphone Mute

- **Enable mute** from the Companion app (Control) or the Web portal.
- While muted:
  - Mic capture and voice-activity detection stop; no audio is sent.
- OLED shows **Microphone Muted** and the mic icon shows a muted state.
  - The LED ring becomes **solid magenta**.
  - The touch pad does **not** open the microphone.
- **Unmute** from the app/portal restores voice behaviour (LED returns to idle).

Terminology:
- **MIC OFF** — normal, not currently capturing.
- **MIC MUTED** — intentionally silenced (privacy).
- **MIC INITIALIZED** — driver ready, not capturing (default at boot).

---

## 16. Storage and the microSD Card

### 16.1 On-chip storage (default)

The firmware uses **SPIFFS** (on-chip filesystem, ~896 KB partition) as the
primary store. It keeps:

- Conversations (JSON) — auto-promoted to memories
- Memories with revisions
- Reminders, goals, habits, planner data
- Knowledge graph
- Log files (rotating) and crash logs
- Backup / export files

### 16.2 microSD (optional)

A microSD card (SPI pins above) is optional. If present, it gives extra capacity
for data, audio assets, and logs. If **absent**, SPIFFS keeps every feature
working — the SD is entirely optional.

- Check status: `GET /api/sd/diagnostics` in the portal.
- If the card is not mounted, check the SPI wiring and format the card as FAT32.

---

## 17. OTA Firmware Updates

- **What:** updating AURA's firmware over Wi-Fi from the app or the web portal,
  without USB.
- **How:** Portal `/ota` (browser upload) or app Tools → Firmware → pick a `.bin`;
  live progress is shown.
- **Before you start:** use a stable, mains-powered USB supply. Do **not** unplug
  during the update.
- **During:** the portal streams the file and verifies SHA‑256 (and, when signed,
  an ECDSA signature) before applying it.
- **Failure:** an invalid image is rejected and the current firmware stays
  bootable; simply retry.
- **Power loss** during the flash can leave the app invalid — recover by flashing
  via Arduino IDE over USB serial.

---

## 18. Sensors

The firmware does **not** include environmental sensor modules. It does include a
**Health Monitor** that samples system metrics:

| Metric | What it detects | Alert |
|---|---|---|
| Heap (free RAM) | Low memory | Low-heap warning |
| Wi-Fi RSSI | Weak connection | Weak-signal warning |
| SD presence | Card mounted? | Missing-card warning |

These are operating-system health metrics (shown in the app dashboard), not
temperature/pressure/light sensors. Sensors from the parts list (BME280, BH1750,
etc.) are **not currently supported**.

---

## 19. Companion App Tools

The **Tools** tab contains:

| Tool | Purpose | How to use |
|---|---|---|
| **Control** | Direct device control | Restart, Factory reset, brightness, volume, gain |
| **Disco Mode** | Party lights | On/Off toggle + brightness slider |
| **Memory** | Browse stored memories | Search, category filter, pin, archive/restore |
| **Reminders** | Schedule reminders | Set title/date/time (local notifications) |
| **Firmware (OTA)** | Update firmware | Pick `.bin`, upload, watch progress |
| **SD Card** | Browse / upload / delete files | Root path, upload, download, delete |
| **Monitor** | System diagnostics | Live CPU/heap/uptime telemetry |

Device-control tools (Control, Disco, SD, OTA) are available only when the app is
connected in **local** mode; the app hides them when the device is unreachable or
the app is in remote mode.

## 20. Status Reference — Master Table

| State | OLED | LED ring | Microphone | App | Meaning |
|---|---|---|---|---|---|
| IDLE | Face (+clock when synced) | Solid Blue (subtle flicker) | OFF | Connected | Ready |
| LISTENING | "Listening…" | Solid Cyan | ON | Mic active | Capturing |
| RECORDING | "Recording…" | Solid Green | ON | — | Recording for STT |
| THINKING / PROCESSING | "Thinking…" | Solid Yellow | OFF | Working | AI processing |
| SPEAKING | "Speaking…" | Solid White | OFF | — | Playback (needs speaker) |
| MUTED | "Microphone Muted" | Solid Magenta | OFF (muted) | Muted | Privacy |
| SETUP | "AURA SETUP" + SSID | Solid Purple | OFF (blocked) | — | Provisioning |
| ERROR | Error / notice | Solid Red (deep fast flicker) | — | Error | Failure |
| OTA | Update screen | Solid Orange | — | Updating | Firmware update |
| DISCO (extra) | Unchanged | Party animation | — | Disco toggle | Visual override |

App-only entries: **Disco** (controlled only from the app/portal; OLED unchanged).

---

## 21. Troubleshooting

| Symptom | Likely causes | Check | Solution |
|---|---|---|---|
| AURA won't boot | Power/bad cable, crash loop | Serial output, LED behaviour | Stable 5 V; re-flash over USB; Safe Mode appears after repeated crashes |
| OLED blank | Display wiring / I2C address / power | Address 0x3C, serial logs | Reconnect/verify address; re-flash if needed |
| OLED animations stop | Watchdog / hang | Serial log | Reboot; re-flash if persistent |
| LED ring off | LED power rail, GPIO4 wiring, brightness 0 | Solid boot blue | Power the ring correctly; verify pin |
| Touch not responding | Sensor wiring / ground / muted state | Try repeat taps; watch LED | Check wiring; not related to STT |
| Mic never starts | Continuous-listening setting, muted | App settings, LED magenta | Keep push-to-talk; unmute; single tap |
| Mic won't activate | Privacy/mute active | LED magenta, OLED muted | Unmute from app; retry tap |
| Mic has no audio | Speaker not connected (hardware) | — | Expected until a speaker is fitted |
| STT unavailable | Key/CA not set, no network | Serial "STT init failed" | Configure Sarvam key + root CA; not a touch fault |
| TTS unavailable | Same as STT + speaker | — | Configure key/CA; add speaker |
| Gemini unavailable | Key/endpoint/network | — | Expected now; local engine answers |
| Wi-Fi unavailable | Power / interference | `/api/wifi` | Re-scan and reconnect in the portal |
| App cannot connect | IP changed / different network | Developer → address | Update IP; same network/VLAN |
| App shows Offline | Device offline or remote mode | Badge state | Reconnect local; re-check Wi-Fi |
| Cloud login fails | Supabase not configured | `.env`, supabase docs | Configure project; correct account |
| SD card not seen | Format/wiring/power | `/api/sd/diagnostics` | Reformat FAT32, verify SPI pins |
| OTA fails | Network / wrong file / signature | Hash check | Stable power; retry; signed firmware |
| Setup opens by itself | Accidental 5 s hold | Press timing | Hold 5 s again to exit |
| Restarts / watchdog loop | Crash loop, low memory | Serial + crash log | Safe Mode, disable modules, re-flash |
| Low memory | Too many features | Heap metric | Disable unused modules; free heap |

---

## 22. Safety

### Power and charging
- Use a quality **5 V / 2 A** USB-C supply. Do not exceed rated current.
- The parts list uses a **5000 mAh Li-ion cell**. Charge it only through a
  dedicated charger (BQ24074-class). Never charge a bare cell directly.
- Keep the cell inside the enclosure and protect against over-charge,
  over-discharge, and high temperature.

### Wiring and heat
- The ESP32 and WS2812 ring can get warm; keep the enclosure ventilated and away
  from fabrics/plastics.
- Wire only per the verified pinout in §3; avoid loose strands shorting pins.
- The MAX98357A can be loud — use an appropriate speaker and volume.

### Firmware
- Never remove power during an OTA update.
- Prefer a stable serial connection when re-flashing.

### Liquids and shorts
- Keep AURA away from all liquids. If wet, disconnect power immediately, dry
  fully, and inspect before reuse.
- Never insert metal objects into the USB port or touch test pads.

---

## 23. Developer / Advanced Section

### Architecture
A single Arduino (ESP32) sketch built from ~57 `*Manager` modules, orchestrated by
`SystemManager` in dependency order. A 30-second watchdog plus per-module boot
progress keeps the system observable and self-recovering (Safe Mode after repeated
crashes).

- `config.h` — identity, pins, model strings, hardware flags
  (`AURA_HW_SPEAKER_PRESENT`, `AURA_DEVELOPMENT_MODE`), timing constants.
- `secrets.h` — all keys/credentials; **gitignored; never commit**.
- `version.h` — semantic version (1.0.0 · Mark III "Phoenix").

### REST / WebSocket layer
- `web_portal.cpp` — `ESPAsyncWebServer` on port **80** + WebSocket on port **81**;
  ~100 API routes: `/api/status`, `/api/dashboard/*`, `/api/memories/*`,
  `/api/goals`, `/api/habits`, `/api/planner/*`, `/api/reminders/*`,
  `/api/skills*`, `/api/automations`, `/api/documents/*`, `/api/workspaces*`,
  `/api/plugins`, `/api/espnow/*`, `/api/companion/*`, `/api/diagnostics`,
  `/api/offline_ai/*`, and more.
- REST requires the `X-Auth-Token` header; WebSocket `/ws` carries live status and
  assistant responses.
- Static UI files live in `data/` (uploaded to SPIFFS).

### Storage
- SPIFFS is the default store; `StorageManager` exposes the memory/conversation/
  audio/reminder/backup/log paths with rotation.

### Voice pipeline internals
- `ConversationManager` states: IDLE → LISTENING → PROCESSING → SPEAKING →
  COMPLETED / ERROR, with interruption and cancel.
- STT/TTS go through `SpeechToTextProvider` / `TextToSpeechProvider` interfaces;
  Sarvam is the implemented provider.

### Constraints
- ~4 MB flash (3 MB app + ~896 KB SPIFFS), no PSRAM (~320 KB RAM). Memory is
  intentionally tight; large optional features can be disabled to free heap.
- Single OTA app slot limits update size to ~3 MB.

---

## 24. FAQ

1. **How do I wake AURA?** Single tap. Voice wake is not available.
2. **How do I turn the microphone off?** Double tap, or mute from the app.
3. **How do I know AURA is listening?** OLED "Listening…", LED solid cyan, mic
   icon active.
4. **What does the green LED mean?** Recording state (solid green). Solid cyan
   is listening, solid yellow is thinking, solid white is speaking.
5. **What does a solid magenta LED mean?** Muted / privacy. Solid red means an
   error (or warning).
6. **How do I enter setup?** Hold the touch pad for 5 seconds.
7. **Does AURA need Wi-Fi?** For cloud features and NTP yes; the local AI works
   offline.
8. **Can AURA work without the cloud?** Yes — offline Local AI engine.
9. **What happens if STT fails?** It fails closed; the touch sensor is not broken.
10. **How do I use Disco Mode?** App → Tools → Disco Mode.
11. **How do I update AURA?** App → Tools → Firmware, or the portal `/ota`.
12. **How do I connect the app?** Same network + device login, or cloud sign-in.
13. **Why is the microphone not responding?** Check mute/privacy (LED magenta) and try
    a single tap.
14. **Why is the OLED blank?** Check wiring/address; use the serial console.
15. **What does the mic icon mean?** Idle, listening, or muted.
16. **My Wi-Fi changed — what do I do?** Re-enter via setup (5 s hold).
17. **Where is my data stored?** On-chip SPIFFS; SD if mounted.
18. **What does factory reset do?** Erases settings, memories, Wi-Fi.
19. **Are notifications real-time?** No — local notifications while the app runs.
20. **Do I need the SD card?** No — SPIFFS always works; SD is optional.
21. **What is Safe Mode?** Recovery state after repeated crashes.
22. **When do I see the clock?** When idle and NTP-synced (IST).
23. **Can I control LED brightness?** Yes — Control + settings.
24. **Can I read logs?** Portal log viewer / developer export.
25. **Is the mic always on?** No — push-to-talk by touch is the default.

---

*End of document.*



