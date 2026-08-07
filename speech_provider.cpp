#include "speech_provider.h"
#include "sarvam_stt.h"

SpeechToTextProvider* createSpeechToTextProvider(SpeechProvider type) noexcept {
    switch (type) {
        case SpeechProvider::SARVAM:
            // Sarvam AI STT (active placeholder - not implemented yet).
            return &speechToText;
        case SpeechProvider::DEEPGRAM:
        case SpeechProvider::LOCAL:
        default:
            // Future providers - not implemented yet.
            return nullptr;
    }
}