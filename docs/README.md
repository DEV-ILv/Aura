# AURA OS — Documentation Index

This folder is the canonical documentation for **AURA OS** (firmware) and its
companion applications. The repo-root `README.md` remains the primary entry
point and gives a full project overview.

## Topic documents

| Document | Purpose |
| --- | --- |
| [architecture/HARDWARE.md](architecture/HARDWARE.md) | Hardware overview — current prototype vs planned (V2) hardware |
| [architecture/SOFTWARE.md](architecture/SOFTWARE.md) | Software stack — firmware, web portal, companion app, AI providers, build tooling |
| [architecture/ARCHITECTURE.md](architecture/ARCHITECTURE.md) | System architecture analysis (lifecycle, modules, data flow, known gaps) |
| [architecture/sarvam_ai.md](architecture/sarvam_ai.md) | Sarvam AI STT/TTS integration architecture & readiness |
| [architecture/aura-led-state-machine.md](architecture/aura-led-state-machine.md) | LED ring status system (solid-colour per state) and manual control |
| [development/ota-signing.md](development/ota-signing.md) | OTA key generation, signing, verification, rotation, CI |
| [development/companion-output.md](development/companion-output.md) | Companion app — assistant-response WebSocket spec |
| [development/companion-v2-device-control.md](development/companion-v2-device-control.md) | Companion V2 Device Control centre (endpoints, status) |
| [testing/WIRING.md](testing/WIRING.md) | Verified wiring & pinout (mic, speaker, OLED, LED ring, touch, SD) |
| [development/PARTS.md](development/PARTS.md) | Parts & BOM — current prototype parts and the planned V2 bill of materials |

## Related documents (kept at repo root)

| Document | Purpose |
| --- | --- |
| [`../README.md`](../README.md) | Project overview, features, configuration, building |
| [`../AURA_Instruction_Manual.md`](../AURA_Instruction_Manual.md) | End-user instruction manual (operation, gestures, troubleshooting) |
| [`../AURA_QUICK_START.md`](../AURA_QUICK_START.md) | One-minute getting-started guide |
| [`../SECURITY.md`](../SECURITY.md) | Security policy, reporting, hardening overview |
| [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Setup, coding standards, contribution workflow |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Release history (canonical changelog) |
| [`../hardware/BOM.csv`](../hardware/BOM.csv) | Planned V2 bill of materials (machine-readable; see [development/PARTS.md](development/PARTS.md)) |
| [`../companion/README.md`](../companion/README.md) | Companion app overview & build instructions |

## Companion app

The Flutter companion (`companion/`) has its own
[`README.md`](../companion/README.md) and
[`docs/supabase.md`](../companion/docs/supabase.md) for the Supabase
cloud integration.