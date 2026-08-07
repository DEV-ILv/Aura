#ifndef AURA_TTS_PROVIDER_H
#define AURA_TTS_PROVIDER_H

#include <Arduino.h>
#include "config.h"
#include "audio_manager.h"

// ============================================================================
// Text-to-Speech provider abstraction.
//
// TextToSpeechProvider is the interface every TTS backend implements:
//   * SarvamTextToSpeech   - Sarvam AI (active placeholder, see sarvam_tts.*)
//   * future: ElevenLabs, Piper (local)
//
// The former Google Cloud TTS implementation was removed in favour of Sarvam
// AI. Callers talk to the concrete instance; swapping providers is done through
// createTextToSpeechProvider() so no provider-specific logic leaks into the
// rest of the codebase.
// ============================================================================

/**
 * @enum TTSState
 * @brief Text-to-Speech client state machine states
 */
enum class TTSState : uint8_t {
    IDLE,           ///< Not processing
    CONNECTING,     ///< TLS handshake in progress
    SENDING,        ///< Transmitting request
    WAITING,        ///< Request sent, awaiting first response byte
    RECEIVING,      ///< Reading response body
    DECODING,       ///< Decoding base64 audio
    PLAYING,        ///< Streaming PCM to AudioManager
    COMPLETED,      ///< Playback finished
    ERROR           ///< Error occurred
};

/**
 * @enum TTSError
 * @brief Text-to-Speech error codes
 */
enum class TTSError : uint8_t {
    NONE,           ///< No error
    NETWORK,        ///< Network connectivity issue
    TIMEOUT,        ///< Request/response timeout
    API_ERROR,      ///< TTS API returned error
    JSON_ERROR,     ///< JSON serialization/deserialization error
    DECODE_ERROR,   ///< Base64 decode error
    AUDIO_ERROR,    ///< Audio playback error
    AUTHENTICATION, ///< API key invalid
    TLS_ERROR,      ///< TLS certificate validation failed
    UNKNOWN         ///< Unspecified error
};

/**
 * @struct TTSResponse
 * @brief TTS API response container
 */
struct TTSResponse {
    unsigned long durationMs;     ///< Audio duration (ms)
    uint32_t sampleRate;          ///< Sample rate (Hz)
    uint8_t channels;             ///< Number of channels
    uint8_t bitsPerSample;        ///< Bits per sample
    size_t audioSize;             ///< Decoded audio size (bytes)
    unsigned long latencyMs;      ///< End-to-end latency (ms)
    unsigned long timestamp;      ///< Completion timestamp
    TTSError error;               ///< Error code if failed

    TTSResponse() noexcept
        : durationMs(0), sampleRate(0), channels(1), bitsPerSample(16),
          audioSize(0), latencyMs(0), timestamp(0), error(TTSError::NONE) {}

    void clear() noexcept {
        durationMs = 0;
        sampleRate = 0;
        channels = 1;
        bitsPerSample = 16;
        audioSize = 0;
        latencyMs = 0;
        timestamp = 0;
        error = TTSError::NONE;
    }
};

/**
 * @struct TTSQueueItem
 * @brief Queued speech request
 */
struct TTSQueueItem {
    String text;
    String voice;
    String language;
    float speed;
    float pitch;
    uint8_t volume;
    bool priority;

    TTSQueueItem() noexcept : text(""), voice(""), language(""), speed(1.0f), pitch(0.0f), volume(100), priority(false) {}
    TTSQueueItem(const String& t, const String& v, const String& l, float s, float p, uint8_t vol, bool pri) noexcept
        : text(t), voice(v), language(l), speed(s), pitch(p), volume(vol), priority(pri) {}
};

/**
 * @class TextToSpeechProvider
 * @brief Interface implemented by every text-to-speech backend.
 *
 * Mirrors the historical TextToSpeech public API so providers can be swapped
 * without changing how conversation/reminder/pipeline code drives synthesis.
 */
class TextToSpeechProvider {
public:
    virtual ~TextToSpeechProvider() = default;

    /**
     * @brief Initialize TTS client
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
     * @brief Queue text for speech synthesis
     * @param text Text to speak
     * @param priority If true, insert at front of queue
     * @return true if queued successfully
     */
    virtual bool speak(const String& text, bool priority = false) noexcept = 0;

    virtual void stop() noexcept = 0;
    virtual void pause() noexcept = 0;
    virtual void resume() noexcept = 0;
    virtual void clearQueue() noexcept = 0;
    virtual void setVoice(const String& voice) noexcept = 0;
    virtual void setLanguage(const String& language) noexcept = 0;
    virtual void setSpeed(float speed) noexcept = 0;
    virtual void setPitch(float pitch) noexcept = 0;
    virtual void setVolume(uint8_t volume) noexcept = 0;
    virtual void setApiKey(const String& apiKey) noexcept = 0;
    virtual void setApiEndpoint(const String& endpoint) noexcept = 0;
    virtual void setRootCA(const String& caCert) noexcept = 0;
    virtual void enableStreaming() noexcept = 0;
    virtual void disableStreaming() noexcept = 0;
    virtual bool isBusy() const noexcept = 0;
    virtual bool isPlaying() const noexcept = 0;
    virtual bool isInitialized() const noexcept = 0;
    virtual TTSState getState() const noexcept = 0;
    virtual TTSError getError() const noexcept = 0;
    virtual const TTSResponse& getResponse() const noexcept = 0;
    virtual size_t getQueueSize() const noexcept = 0;

    /**
     * @brief Provider identity
     */
    virtual TTSProvider providerType() const noexcept = 0;
    virtual const char* providerName() const noexcept = 0;
};

/**
 * @brief Create a TTS provider for the given type.
 * @param type Requested provider
 * @return Provider instance, or nullptr if the provider is not implemented.
 * @note Default build uses TTSProvider::SARVAM (placeholder, not implemented).
 */
TextToSpeechProvider* createTextToSpeechProvider(TTSProvider type) noexcept;

#endif // AURA_TTS_PROVIDER_H
