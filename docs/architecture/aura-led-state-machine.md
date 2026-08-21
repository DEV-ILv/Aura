# AURA LED Ring — State Machine

The WS2812B aura ring (`LedRing`, `led_ring.cpp`) is driven from a single
`AuraMood` state (`aura_mood.h`) so the ring can never drift apart from the
OLED face. This document describes the **final solid-colour status system**,
the *idle power policy* and the *manual control session* added for the Device
Control page.

## Normal status path (FINAL: SOLID colour per state)

The 16-LED ring is **ONE indicator**. Every AURA state paints **ALL 16 LEDs in
a single, distinct SOLID colour**. There is NO per-LED movement, chase, comet,
tail, pulse, breathing, or rotation in the normal status path. The only
variation is a subtle **whole-ring synchronised brightness flicker**.

| Mood / event        | Ring behaviour (SOLID, all 16 LEDs)              |
|---------------------|--------------------------------------------------|
| `IDLE` / `READY`    | Solid **Blue** `#0080FF`, subtle flicker         |
| `BOOT` (startup)    | Solid **Blue** `#0080FF` (matches IDLE identity) |
| `LISTENING`         | Solid **Cyan** `#00FFFF`                         |
| `RECORDING`         | Solid **Green** `#00FF40`                        |
| `THINKING` / `PROCESSING` | Solid **Yellow** `#FFFF00`                |
| `SPEAKING`          | Solid **White** `#FFFFFF`                        |
| `SETUP`             | Solid **Purple** `#8000FF`                       |
| `PRIVACY` / Muted   | Solid **Magenta** `#FF00AA`                      |
| `ERROR`             | Solid **Red** `#FF0000` (deeper, faster flicker) |
| `OTA`               | Solid **Orange** `#FF8000`                       |
| `WIFI_CONNECTING`   | Solid **Steel blue** `#4080E0`                   |
| `WIFI_CONNECTED`    | Solid **Spring green** `#00FF80` → returns to IDLE |
| `HAPPY`             | Solid **Gold** `#FFD700`                         |
| `SUCCESS`           | Solid **Mint green** `#00FFAA` → returns to IDLE |
| `REMINDER` (notification) | Solid **Amber** `#FFAA00`                  |
| `WARNING`           | Solid **Red-orange** `#FF4000` (error-style flicker) |
| `CRITICAL`          | Solid **Dark red** `#8A0000`                     |
| `OFFLINE`           | Solid **Slate** `#607085`                        |
| `SLEEP`             | Solid **Dim navy** `#404A60`                     |
| `WAKE`              | Solid **Azure** `#00AFFF` → returns to IDLE      |

### Synchronised brightness flicker

The whole ring shares ONE brightness level; individual LEDs NEVER change
independently. The level performs a small random walk (not a sine), recomputed
every 50–150 ms, so the ring reads as subtle electronic energy rather than a
flash or a breathing pulse.

- Normal states: random walk clamped to ~86–100 % of the mood brightness.
- Quiet states (`IDLE`, `PRIVACY`, `OFFLINE`, `SLEEP`): ~92–100 % (very subtle).
- `ERROR` / `WARNING` / `CRITICAL`: deeper, quicker flicker (~55–100 %, 35–90 ms
  recompute, larger step) so emergencies stand out.
- The walk state (`m_flickerLevel`, `m_flickerTarget`, `m_flickerNext`) is reset
  to full brightness on every `setMood()` so a fresh solid colour starts clean.

### Idle power policy

The ring is OFF for `SLEEP` and `OFFLINE` (power save). Normal `IDLE` shows a
quiet, dim **solid blue** presence (passive-listening glow) instead of being
fully dark. Transient moods (`SUCCESS`, `WAKE`, `WIFI_CONNECTED`) automatically
return to `IDLE` when their duration elapses.

### Implementation

- Solid status rendering is centralised in
  `LedRing::renderSolidStatus(color, lo%, hi%, stepMax, minMs, maxMs)` which
  fills `m_frame[LED_COUNT]` with one scaled colour; `update()` then applies a
  short `kCrossFade` blend so state changes ease between two solid colours
  without any movement.
- Quiet states are classified by `LedRing::isQuietMood()` (`SLEEP`, `OFFLINE`).
  In automatic mode `update()` short-circuits to `clearRing()` for those.
- The buffer is blacked-out once and `FastLED.show()` is skipped while the ring
  is already black, keeping idle CPU and current draw minimal.
- Transient renderers (`SUCCESS`, `WAKE`, `WIFI_CONNECTED`) call
  `setMood(AuraMood::IDLE)` when their duration elapses.
- `isSequentialAnimation()` is retained as a marker only and always returns
  `false` — no status state uses a sequential/moving animation any more.
- Disco Mode is fully separate and unaffected (see below).

## Manual control session (Device Control page)

The page can turn the ring on, change colour/animation, and run LED tests via
`POST /api/led/control`. These commands open a **temporary manual session**
that overrides the automatic behaviour.

- `beginManualControl()` — opens (or refreshes) a session. On first entry it
  snapshots the current system mood so colour/brightness changes are
  immediately visible.
- `setManualMood(mood)` — sets the animation shown during the session (drives
  the **ring only**, never the OLED face, so a test cannot desync the face).
- `endManualControl()` — restores automatic behaviour.
- Auto-expiry: the session times out after `kManualControlTimeoutMs` (60 s) of
  no further commands. `update()` checks this with `millis()` — **event-driven,
  no polling loops, no `delay()`-based timers, no blocking**.
- When the session ends, automatic behaviour resumes: if the system is idle the
  ring returns to the solid-blue idle.

### Endpoint semantics (`handleApiLedControl`)

| Field       | Effect                                                       |
|-------------|--------------------------------------------------------------|
| `enabled:false` | ends the manual session and turns the ring off (persisted) |
| `enabled:true`  | turns the ring on and starts a manual session             |
| `brightness`    | sets global brightness and refreshes the manual session    |
| `mood`          | sets the manual animation and refreshes the session        |
| `r`/`g`/`b`     | sets the theme colour and refreshes the session            |
| `manual:false`  | applies the setting without opening a manual session       |

`GET /api/led/control` now reports the manual mood while a session is active
(`manual:true`), so the page reflects what the ring is actually showing.

## Disco Mode (separate, app-only)

Disco Mode remains a fun, optional **visual override** enabled only from the
Companion app or the portal API. It is independent from the status path above:
while enabled it runs a rotating set of ~10 smooth (low-strobe) colour
animations at the disco brightness. Emergency moods (`ERROR`, `CRITICAL`, `OTA`,
`SETUP`, `PRIVACY`) always outrank it and pause it; when the emergency clears,
Disco resumes if it is still enabled. The enabled flag is not persisted — Disco
is OFF after every reboot.

## Power

With `SLEEP`/`OFFLINE` fully OFF and IDLE at a dim solid blue, idle current from
the WS2812B string stays low. Brightness is still eased per mood during active
events via `easeBrightness()`.
