#ifndef AURA_SARVAM_STT_H
#define AURA_SARVAM_STT_H

#include <Arduino.h>
#include "speech_provider.h"
#include "sarvam_client.h"

// ============================================================================
// Sarvam AI Speech-to-Text provider (production implementation).
//
// Flow: On startRecognition() AURA enables mic capture via AudioManager and
// streams 16 kHz mono PCM to Sarvam using Transfer-Encoding: chunked through a
// small static ring (no large heap utterance buffer). A lightweight VAD on the
// captured samples starts the upload once speech is detected and ends it after
// a quiet period (or the cap). On end, the caller (ConversationManager) sees
// isBusy() go false; the final transcript is reported through getResult().
//
// Network I/O is async via SarvamClient (https, streaming, no retry once the
// stream has started). No caller-facing interface changed; the
// speech_provider.h contract is fully honored.
// ============================================================================

class SarvamSpeechToText : public SpeechToTextProvider {
public:
    SarvamSpeechToText() noexcept = default;
    ~SarvamSpeechToText() noexcept override;

    bool initialize() noexcept override;
    void run() noexcept override;
    void update() noexcept override;
    bool startRecognition(RecognitionMode mode = RecognitionMode::ONESHOT) noexcept override;
    bool stopRecognition() noexcept override;
    void cancelRecognition() noexcept override;
    void processAudio(const int16_t* audioData, size_t length) noexcept override;
    void processAudioChunk(const AudioChunk& chunk) noexcept override;

    bool isRecognizing() const noexcept override;
    bool isBusy() const noexcept override;
    bool isInitialized() const noexcept override;
    const SpeechResult& getResult() const noexcept override;
    void clearResult() noexcept override;

    /**
     * @brief Clear the latched error state after a failure has been handled.
     * @note Used by the conversation error recovery so the pipeline can leave
     *       the ERROR state instead of being trapped by a stale error latch.
     */
    void clearError() noexcept;

    void setLanguage(const String& language) noexcept override;
    void setTimeout(unsigned long timeoutMs) noexcept override;
    void setSampleRate(uint32_t sampleRate) noexcept override;
    void setAudioFormat(AudioFormat format) noexcept override;
    void setMode(RecognitionMode mode) noexcept override;
    SpeechState getState() const noexcept override;
    SpeechError getError() const noexcept override;
    ApiState getApiState() const noexcept override;
    void setApiEndpoint(const String& endpoint) noexcept override;
    void setApiKey(const String& apiKey) noexcept override;
    void setRootCA(const String& caCert) noexcept override;
    void setPartialResults(bool enable) noexcept override;
    void setProfanityFilter(bool enable) noexcept override;
    void setMaxAlternatives(uint8_t max) noexcept override;
    void getBufferStats(size_t& usedBytes, size_t& capacityBytes) const noexcept override;
    bool isBufferNearFull() const noexcept override;
    bool flushBuffer() noexcept override;
    bool sendAudio() noexcept override;

    SpeechProvider providerType() const noexcept override { return SpeechProvider::SARVAM; }
    const char* providerName() const noexcept override { return "Sarvam STT"; }

private:
    enum class CapturePhase : uint8_t { NONE, LISTENING, UPLOADING };
    void captureMic() noexcept;
    void finalizeCapture(bool requestTranscription) noexcept;
    void restoreMic() noexcept;
    void setState(SpeechState s) noexcept;
    void mapHttpError() noexcept;
    void beginStream() noexcept;
    void feedClientRing() noexcept;
    void ringPut(const uint8_t* data, size_t len) noexcept;
    float frameEnergy(const uint8_t* buf, size_t bytes) const noexcept;

    SpeechResult m_result;
    SpeechState m_state{SpeechState::IDLE};
    SpeechError m_error{SpeechError::NONE};
    ApiState m_apiState{ApiState::DISCONNECTED};
    RecognitionMode m_mode{RecognitionMode::ONESHOT};
    CapturePhase m_capturePhase{CapturePhase::NONE};

    bool m_initialized{false};
    String m_apiKey;
    String m_endpoint;
    String m_rootCA;
    String m_language{SARVAM_LANGUAGE};
    unsigned long m_timeoutMs{SARVAM_TIMEOUT_MS};
    uint32_t m_sampleRate{SARVAM_SAMPLE_RATE};
    AudioFormat m_format{AudioFormat::PCM16_MONO};
    uint8_t m_maxAlternatives{1};
    bool m_partialResults{false};
    bool m_profanityFilter{false};

    // --- registration / VAD
    unsigned long m_sessionStart{0};
    unsigned long m_lastVoiceMs{0};
    bool m_sawVoice{false};
    size_t m_pcmLen{0};          // total PCM captured this session (statistics)
    bool m_streaming{false};     // chunked upload active
    SpeechError m_pendingError{SpeechError::NONE};

    // --- dynamic ring: pre-roll + bytes waiting to be handed to SarvamClient
    static constexpr size_t kPcmRingCap = 8192;  // ~256 ms at 16 kHz mono
    uint8_t* m_pcmRing{nullptr};
    size_t m_ringHead{0};
    size_t m_ringTail{0};
    size_t m_ringCount{0};
};

extern SarvamSpeechToText speechToText;

#endif // AURA_SARVAM_STT_H