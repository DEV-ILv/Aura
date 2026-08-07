#include "tts_provider.h"
#include "sarvam_tts.h"

TextToSpeechProvider* createTextToSpeechProvider(TTSProvider type) noexcept {
    switch (type) {
        case TTSProvider::SARVAM:
            // Sarvam AI TTS (active placeholder - not implemented yet).
            return &textToSpeech;
        case TTSProvider::ELEVENLABS:
        case TTSProvider::PIPER:
        default:
            // Future providers - not implemented yet.
            return nullptr;
    }
}