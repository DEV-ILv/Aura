# AURA OS — Documentation Index

This folder is the canonical documentation for **AURA OS** (firmware) and its
companion applications. The repo-root `README.md` remains the primary entry
point and gives a full project overview.

## Topic documents

| Document | Purpose |
| --- | --- |
| [HARDWARE.md](HARDWARE.md) | Hardware overview — current prototype vs planned (V2) hardware |
| [PARTS.md](PARTS.md) | Parts & BOM — current prototype parts and the planned V2 bill of materials |
| [WIRING.md](WIRING.md) | Verified wiring & pinout (mic, speaker, OLED, LED ring, touch, SD) |
| [SOFTWARE.md](SOFTWARE.md) | Software stack — firmware, web portal, companion app, AI providers, build tooling |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System architecture analysis (lifecycle, modules, data flow, known gaps) |
| [ota-signing.md](ota-signing.md) | OTA key generation, signing, verification, rotation, CI |
| [sarvam_ai.md](sarvam_ai.md) | Sarvam AI STT/TTS integration architecture & readiness |
| [aura-led-state-machine.md](aura-led-state-machine.md) | LED ring status system (solid-colour per state) and manual control |
| [companion-output.md](companion-output.md) | Companion app — assistant-response WebSocket spec |
| [companion-v2-device-control.md](companion-v2-device-control.md) | Companion V2 Device Control centre (endpoints, status) |

## Related documents (kept at repo root)

| Document | Purpose |
| --- | --- |
| [`../README.md`](../README.md) | Project overview, features, configuration, building |
| [`../AURA_Instruction_Manual.md`](../AURA_Instruction_Manual.md) | End-user instruction manual (operation, gestures, troubleshooting) |
| [`../AURA_QUICK_START.md`](../AURA_QUICK_START.md) | One-minute getting-started guide |
| [`../SECURITY.md`](../SECURITY.md) | Security policy, reporting, hardening overview |
| [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Setup, coding standards, contribution workflow |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Release history (canonical changelog) |
| [`../BOM.csv`](../BOM.csv) | Planned V2 bill of materials (machine-readable; see [PARTS.md](PARTS.md)) |
| [`../journal.csv`](../journal.csv) | Development work log (evidence-backed) |

## Companion app

The Flutter companion (`aura_companion/`, separate repository) has its own
[`README.md`](../../aura_companion/README.md) and
[`docs/supabase.md`](../../aura_companion/docs/supabase.md) for the Supabase
cloud integration.
