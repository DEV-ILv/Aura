#ifndef AURA_SPEECH_PROVIDER_H
#define AURA_SPEECH_PROVIDER_H

#include <Arduino.h>
#include "config.h"
#include "audio_manager.h"

// ============================================================================
// Speech-to-Text provider abstraction.
//
// SpeechToTextProvider is the interface every STT backend implements:
//   * SarvamSpeechToText   - Sarvam AI (active placeholder, see sarvam_stt.*)
//   * future: Deepgram, Whisper, local VAD-based engines
//
// The former Google Cloud STT implementation was removed in favour of Sarvam
// AI. Callers talk to the concrete instance; swapping providers is done through
// createSpeechToTextProvider() so no provider-specific logic leaks into the
// rest of the codebase.
// ============================================================================

/**
 * @enum SpeechState
 * @brief Speech recognition session states
 */
enum class SpeechState : uint8_t {
    IDLE,           ///< Not recording or processing
    LISTENING,      ///< Actively recording audio from microphone
    PROCESSING,     ///< Audio sent to API, awaiting response
    COMPLETED,      ///< Recognition complete, final result available
    ERROR           ///< Error occurred during recognition
};

/**
 * @enum SpeechError
 * @brief Speech recognition error codes
 */
enum class SpeechError : uint8_t {
    NONE,           ///< No error
    NETWORK,        ///< Network connectivity issue
    TIMEOUT,        ///< Recognition timeout expired
    API_ERROR,      ///< Speech API returned error response
    INVALID_AUDIO,  ///< Audio format/sample rate unsupported
    BUFFER_OVERFLOW,///< Audio buffer exceeded capacity
    UNAUTHORIZED,   ///< API key invalid or missing
    TLS_ERROR,      ///< TLS certificate validation failed
    UNKNOWN         ///< Unspecified error
};

/**
 * @enum RecognitionMode
 * @brief Speech recognition operating modes
 */
enum class RecognitionMode : uint8_t {
    ONESHOT,        ///< Single utterance, stop on silence
    CONTINUOUS,     ///< Continuous streaming recognition
    VOICE_COMMAND   ///< Short command recognition with VAD
};

/**
 * @enum ApiState
 * @brief HTTPS API connection states
 */
enum class ApiState : uint8_t {
    DISCONNECTED,   ///< No active connection
    CONNECTING,     ///< TLS handshake in progress
    CONNECTED,      ///< Connection established
    SENDING,        ///< Transmitting request
    RECEIVING,      ///< Reading response
    ERROR           ///< Connection error
};

/**
 * @struct SpeechResult
 * @brief Speech recognition result container
 */
struct SpeechResult {
    String transcript;          ///< Full recognized transcript
    String partial;             ///< Partial/interim result
    float confidence;           ///< Confidence score (0.0 - 1.0)
    bool isFinal;               ///< True if final result
    SpeechError error;          ///< Error code if failed
    unsigned long durationMs;   ///< Audio duration processed (ms)
    unsigned long timestamp;    ///< Unix timestamp of completion
    uint32_t audioLength;       ///< Bytes of audio processed

    SpeechResult() noexcept
        : transcript(""), partial(""), confidence(0.0f), isFinal(false),
          error(SpeechError::NONE), durationMs(0), timestamp(0), audioLength(0) {}

    void clear() noexcept {
        transcript.clear();
        partial.clear();
        confidence = 0.0f;
        isFinal = false;
        error = SpeechError::NONE;
        durationMs = 0;
        timestamp = 0;
        audioLength = 0;
    }
};

/**
 * @struct AudioChunk
 * @brief Audio buffer chunk for streaming
 */
struct AudioChunk {
    const int16_t* data;        ///< Pointer to PCM samples
    size_t length;              ///< Number of samples
    unsigned long timestamp;    ///< Capture timestamp (ms)

    AudioChunk() noexcept : data(nullptr), length(0), timestamp(0) {}
    AudioChunk(const int16_t* d, size_t l, unsigned long t) noexcept
        : data(d), length(l), timestamp(t) {}
};

/**
 * @class SpeechToTextProvider
 * @brief Interface implemented by every speech-to-text backend.
 *
 * Mirrors the historical SpeechToText public API so providers can be swapped
 * without changing how conversation/pipeline code drives recognition.
 */
class SpeechToTextProvider {
public:
    virtual ~SpeechToTextProvider() = default;

    /**
     * @brief Initialize the provider
     * @return true if initialization successful
     */
    virtual bool initialize() noexcept = 0;

    /**
     * @brief Main update loop - process state machine and timeouts
     */
    virtual void run() noexcept = 0;

    /**
     * @brief Alias for run() for scheduler compatibility
     */
    virtual void update() noexcept = 0;

    /**
     * @brief Start speech recognition session
     */
    virtual bool startRecognition(RecognitionMode mode = RecognitionMode::ONESHOT) noexcept = 0;

    /**
     * @brief Stop recording and process accumulated audio
     */
    virtual bool stopRecognition() noexcept = 0;

    /**
     * @brief Cancel current recognition session
     */
    virtual void cancelRecognition() noexcept = 0;

    /**
     * @brief Process incoming audio data from AudioManager callback
     */
    virtual void processAudio(const int16_t* audioData, size_t length) noexcept = 0;

    /**
     * @brief Process audio chunk with timestamp (for streaming)
     */
    virtual void processAudioChunk(const AudioChunk& chunk) noexcept = 0;

    virtual bool isRecognizing() const noexcept = 0;
    virtual bool isBusy() const noexcept = 0;
    virtual bool isInitialized() const noexcept = 0;
    virtual const SpeechResult& getResult() const noexcept = 0;
    virtual void clearResult() noexcept = 0;
    virtual void setLanguage(const String& language) noexcept = 0;
    virtual void setTimeout(unsigned long timeoutMs) noexcept = 0;
    virtual void setSampleRate(uint32_t sampleRate) noexcept = 0;
    virtual void setAudioFormat(AudioFormat format) noexcept = 0;
    virtual void setMode(RecognitionMode mode) noexcept = 0;
    virtual SpeechState getState() const noexcept = 0;
    virtual SpeechError getError() const noexcept = 0;
    virtual ApiState getApiState() const noexcept = 0;
    virtual void setApiEndpoint(const String& endpoint) noexcept = 0;
    virtual void setApiKey(const String& apiKey) noexcept = 0;
    virtual void setRootCA(const String& caCert) noexcept = 0;
    virtual void setPartialResults(bool enable) noexcept = 0;
    virtual void setProfanityFilter(bool enable) noexcept = 0;
    virtual void setMaxAlternatives(uint8_t max) noexcept = 0;
    virtual void getBufferStats(size_t& usedBytes, size_t& capacityBytes) const noexcept = 0;
    virtual bool isBufferNearFull() const noexcept = 0;
    virtual bool flushBuffer() noexcept = 0;
    virtual bool sendAudio() noexcept = 0;

    /**
     * @brief Provider identity
     */
    virtual SpeechProvider providerType() const noexcept = 0;
    virtual const char* providerName() const noexcept = 0;
};

/**
 * @brief Create an STT provider for the given type.
 * @param type Requested provider
 * @return Provider instance, or nullptr if the provider is not implemented.
 * @note Default build uses SpeechProvider::SARVAM (placeholder, not implemented).
 */
SpeechToTextProvider* createSpeechToTextProvider(SpeechProvider type) noexcept;

#endif // AURA_SPEECH_PROVIDER_H
