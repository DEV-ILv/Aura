# AURA LED Ring — State Machine

The WS2812B aura ring (`LedRing`, `led_ring.cpp`) is driven from a single
`AuraMood` state (`aura_mood.h`) so the ring can never drift apart from the
OLED face. This document describes the *idle power policy* and the *manual
control session* added for the Device Control page.

## Idle power policy

**The ring is only lit for meaningful events and is kept completely OFF while
the system is idle.**

| Mood / event             | Ring behaviour                                            |
|--------------------------|-----------------------------------------------------------|
| `IDLE`                   | **OFF** (no breathing, no sparkle, no residual glow)      |
| `SLEEP`                  | **OFF**                                                   |
| `OFFLINE`                | **OFF**                                                   |
| `BOOT` (startup)         | single-LED fill / glow, then OFF when boot completes      |
| `LISTENING`              | deep-blue VU wave (mic level)                             |
| `THINKING` / `PROCESSING`| cyan energy flow / pulse                                  |
| `SPEAKING`               | blue-white speech pulse                                   |
| `HAPPY` / `SUCCESS`      | gold pulse / green wave → returns to OFF                  |
| `REMINDER` (notification)| golden ripple                                             |
| `WARNING` / `ERROR` / `CRITICAL` | orange / red / dark-red slow pulse                |
| `OTA`                    | purple rotating energy (progress %)                       |
| `WAKE`                   | expanding blue pulse → returns to OFF                     |
| `WIFI_CONNECTING`        | sweeping segment (brief)                                  |
| `WIFI_CONNECTED`         | pulse → returns to OFF                                    |

Events enter and leave through the normal `AuraSystem::setMood` transitions
(e.g. `listen()` → `enterIdle()`). Because `IDLE` renders nothing, **any event
that completes automatically switches the ring back to OFF** — no extra
timeout or polling is needed.

### Implementation

- Quiet states are classified by `LedRing::isQuietMood()` (`IDLE`, `SLEEP`,
  `OFFLINE`). In automatic mode `update()` short-circuits to `clearRing()`
  when the rendered mood is quiet.
- The buffer is blacked-out once and `FastLED.show()` is skipped while the
  ring is already black, keeping idle CPU and current draw minimal.
- Transient renderers (`SUCCESS`, `WAKE`, `WIFI_CONNECTED`) call
  `setMood(AuraMood::IDLE)` + `clearRing()` when their duration elapses, so
  the ring returns to OFF immediately after the brief animation.

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
  ring returns to **OFF**.

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

## Power

With the ring fully OFF at idle, idle current from the WS2812B string drops to
the LED quiescent level (near 0 mA) instead of the previous ~20 % idle
breathing plus sparkle. Brightness is still eased per mood during active
events via `easeBrightness()`.
