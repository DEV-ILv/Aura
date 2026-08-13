# AURA — QUICK START

Getting started with AURA takes about one minute. Follow the steps in order.

---

## 1. Power it on

1. Connect a **5 V / USB-C power cable** (or battery, if fitted).
2. Wait for the boot animation: a small logo, "AURA", a progress bar.

## 2. Wait for READY

3. When boot finishes, the small OLED shows **READY**, then its idle "face".
4. The 16-LED ring settles into the **idle** indication (all 16 LEDs solid
   **blue** with a very subtle flicker).
5. The top of the screen may show the **time (HH:MM, IST)** once synced over Wi‑Fi.
   Until then it shows `--:--`.

## 3. Talk to AURA

6. **Tap once** on the touch pad — the microphone opens ("Listening…", LED shows
   all-16 solid **cyan**).
7. **Speak** your question in the direction of the microphone.
8. When you finish, AURA processes ("Thinking…") and replies.
9. Touch **double tap** to stop or cancel an interaction at any time.

## 4. First-time Wi-Fi (Setup)

10. If your home Wi-Fi is not yet configured, hold the touch pad for **5 seconds**.
11. The OLED shows **AURA SETUP** and the network to join.
12. Open your phone's Wi-Fi settings and join that network, then open
    **http://192.168.4.1** in a browser.
13. Enter your home Wi‑Fi SSID and password and save. AURA connects and leaves
    setup. (Hold 5 s again to exit setup manually.)

## 5. Companion app

- Install **AURA Companion** (Android or Windows).
- Keep the phone on the same Wi‑Fi as AURA, open the app, and sign in with the
  admin username/password (shown on the serial console at first boot).
- If the device isn't reachable, use the **cloud** (Supabase) sign-in.

---

## Gesture summary

| Gesture | Result |
|---|---|
| **Tap once** | Start talking (microphone on) |
| **Tap twice** | Stop / cancel; return to idle |
| **Hold 5 s** | Enter (or exit) Setup Mode |
| **Hold 15 s** | Clean system restart (passes through setup at 5 s) |

## Not available on this build

- Wake-by-word ("Hey AURA") — voice wake is not wired up; tap to talk.
- Spoken replies — the speaker is not connected in the current hardware/build.
- Sensors, battery %, GPS — none in this firmware.

*Everything you need to know — including LEDs, OLEDs, the app, tools, and a
troubleshooter — is in **AURA_Instruction_Manual.md**.*