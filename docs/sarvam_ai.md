# Sarvam AI Integration — Architecture & Readiness Guide

> **Status: architecture-ready only. Sarvam AI is NOT implemented.**
>
> This document describes how AURA is prepared for Sarvam AI (Speech-to-Text
> and Text-to-Speech) so that adding the API key and implementing the calls
> later requires minimal changes. The former **Google Cloud** STT/TTS modules
> have been **removed**; Sarvam is now the ACTIVE provider but only ships as a
> placeholder, so the voice pipeline degrades gracefully until implemented.

---

## 1. Current state

| Concern | Provider |
|---|---|
| Reasoning engine | **Gemini** (`gemini_client.*`) — unchanged, remains the brain |
| Speech-to-Text (firmware) | **Sarvam AI** — `sarvam_stt.*` → `SarvamSpeechToText` (placeholder) |
| Text-to-Speech (firmware) | **Sarvam AI** — `sarvam_tts.*` → `SarvamTextToSpeech` (placeholder) |
| Companion on-device STT | platform `speech_to_text` plugin (device recogniser) |
| Companion on-device TTS | platform `flutter_tts` plugin (device engine) |

The firmware voice pipeline today: `INMP441 mic → AudioManager → Sarvam STT
(placeholder) → Gemini → Sarvam TTS (placeholder) → MAX98357 speaker`.

## 2. Planned architecture (Sarvam-ready)

Two provider interfaces make STT/TTS interchangeable:

- `SpeechToTextProvider` — `speech_provider.h`
  Implementations: `SarvamSpeechToText` (active placeholder), future: Deepgram, local
- `TextToSpeechProvider` — `tts_provider.h`
  Implementations: `SarvamTextToSpeech` (active placeholder), future: ElevenLabs, Piper

Provider selection is centralised behind factories so no provider-specific
logic leaks into the codebase:

```cpp
// speech_provider.cpp / tts_provider.cpp
SpeechToTextProvider* createSpeechToTextProvider(SpeechProvider type);
TextToSpeechProvider* createTextToSpeechProvider(TTSProvider type);
```

Selection enums live in `config.h`:

```cpp
enum class SpeechProvider { SARVAM, DEEPGRAM, LOCAL };
enum class TTSProvider    { SARVAM, ELEVENLABS, PIPER };

#define DEFAULT_SPEECH_PROVIDER SpeechProvider::SARVAM
#define DEFAULT_TTS_PROVIDER    TTSProvider::SARVAM
```

### Module map

| Module | Role | Status |
|---|---|---|
| `speech_provider.h/.cpp` | STT interface + factory | done |
| `tts_provider.h/.cpp` | TTS interface + factory | done |
| `sarvam_client.h/.cpp` | Sarvam API client shell | placeholder |
| `sarvam_stt.h/.cpp` | `SarvamSpeechToText` (ACTIVE provider) | placeholder |
| `sarvam_tts.h/.cpp` | `SarvamTextToSpeech` (ACTIVE provider) | placeholder |
| `config.h` | provider enums, defaults, `SARVAM_BASE_URL` | done |
| `secrets.h.example` | `SARVAM_API_KEY` placeholder | done |

The former `speech_to_text.*` / `text_to_speech.*` (Google) modules and the
`GOOGLE_STT_API_KEY` / `GOOGLE_TTS_API_KEY` secrets were **deleted**. All Sarvam
modules return "Not Implemented" (`false` / neutral defaults), so the default
build today degrades gracefully until the Sarvam integration is completed.

## 3. Required API key

Add your key to the **git-ignored** `secrets.h` (never commit real keys):

```cpp
// secrets.h (local, git-ignored) — NOT committed
constexpr char SARVAM_API_KEY[] = "your-sarvam-api-key";
```

`secrets.h.example` already ships the placeholder:

```cpp
constexpr char SARVAM_API_KEY[] = "";
```

Sarvam AI SDK keys are obtained from the Sarvam AI developer portal
(`https://dashboard.sarvam.ai`). Keep the key off the firmware image by
preferring the Web Portal > Settings runtime configuration path used for the
Gemini key today.

## 4. STT flow (planned)

```
Touch / wake word
  → AudioManager captures INMP441 PCM16 (16 kHz mono)
  → SarvamSpeechToText buffers samples (processAudio / processAudioChunk)
  → stopRecognition() → SarvamClient::transcribeAudio(pcm, len, &transcript)
      POST {SARVAM_BASE_URL}/v1/speech-to-text  (Authorization: Bearer SARVAM_API_KEY)
  → SpeechResult { transcript, confidence, error }
  → ConversationManager consumes SpeechResult exactly as today
```

Interfaces already expose everything ConversationManager needs
(`startRecognition`, `stopRecognition`, `processAudio`, `getResult`,
`getError`, …), so no caller changes are required when the Sarvam STT module is
filled in.

## 5. TTS flow (planned)

```
ConversationManager / ReminderManager / StartupGreetingManager
  → textToSpeech.speak(text, priority)
  → SarvamTextToSpeech → SarvamClient::synthesizeSpeech(text, &pcmBase64)
      POST {SARVAM_BASE_URL}/v1/text-to-speech  (Authorization: Bearer SARVAM_API_KEY)
  → decode base64 → PCM16
  → AudioManager.play(...)  → MAX98357 speaker
```

The TTS queueing/playback contract (`speak`, `stop`, `pause`, `resume`,
`isBusy`, `getState`) is already defined on `TextToSpeechProvider`, so
reminders and greetings keep working unchanged.

## 6. Gemini integration

Gemini is **not touched**. `GeminiClient` remains the reasoning engine:

```
Sarvam STT
  → transcript
  → Gemini generateContent(...)  →  reply text
  → Sarvam TTS  →  audio
```

`ai_pipeline.*` already treats STT / Gemini / TTS as separate stages; the
providers slot into the `SPEECH_TO_TEXT` and `TEXT_TO_SPEECH` pipeline stages
without pipeline changes.

## 7. Expected request flow (end-to-end)

1. User speaks → mic → audio buffer (provider-agnostic).
2. `SpeechToTextProvider::stopRecognition()` → provider request → transcript.
3. Transcript → `ConversationManager` → `GeminiClient` → reply text.
4. Reply text → `TextToSpeechProvider::speak(...)` → provider request → PCM.
5. PCM → `AudioManager` → speaker. Queue/priority handled by the provider.

## 8. Work remaining to complete Sarvam support

1. **Implement `SarvamClient`** (`sarvam_client.cpp`):
   - TLS transport (`WiFiClientSecure` + root CA).
   - `transcribeAudio()` → Sarvam STT REST call (ArduinoJson request/response).
   - `synthesizeSpeech()` → Sarvam TTS REST call.
2. **Implement `SarvamSpeechToText`** (`sarvam_stt.cpp`):
   - Buffer management, `startRecognition`/`stopRecognition`, response parsing
     into `SpeechResult`, error/timeout handling (mirror the former Google STT
     behaviour for reference).
3. **Implement `SarvamTextToSpeech`** (`sarvam_tts.cpp`):
   - Queueing, synthesis request, base64 decode, `AudioManager` playback.
4. **Wire the key at runtime**: expose `SARVAM_API_KEY` in Web Portal > Settings
   and load it via `SarvamClient::setApiKey(...)` during `system_manager`
   initialisation.
5. **Update this document** with the real request/response payloads once they
   are verified against the live API.

## 9. Compatibility notes

- The Google STT/TTS modules, `GOOGLE_STT_URL`/`GOOGLE_TTS_URL` config and the
  `GOOGLE_STT_API_KEY`/`GOOGLE_TTS_API_KEY` secrets have been removed; the
  `SpeechProvider::GOOGLE` / `TTSProvider::GOOGLE` enum values were dropped.
- The companion app exposes informational `Speech provider` / `TTS provider`
  selections in Settings (Voice section); they persist locally and do **not**
  change behaviour until the firmware implements Sarvam.
- Never commit `secrets.h`, `keys/private.pem`, or any real API key.
