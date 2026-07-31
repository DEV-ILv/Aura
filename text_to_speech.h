#ifndef AURA_TEXT_TO_SPEECH_H
#define AURA_TEXT_TO_SPEECH_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <vector>
#include "audio_manager.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "config.h"
#include "logger.h"
#include "secrets.h"

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
 * @class TextToSpeech
 * @brief Single authority for text-to-speech synthesis
 *
 * Manages:
 * - HTTPS communication with TTS API
 * - JSON request/response handling
 * - Base64 audio decoding
 * - PCM audio buffering and streaming
 * - Voice, language, speed, pitch, volume control
 * - Playback queue with priority support
 * - Timeout and error handling
 *
 * Non-blocking, production-quality TTS for ESP32.
 */
class TextToSpeech {
public:
    /**
     * @brief Constructor
     */
    TextToSpeech() noexcept;

    /**
     * @brief Destructor
     */
    ~TextToSpeech() noexcept;

    // Delete copy semantics
    TextToSpeech(const TextToSpeech&) = delete;
    TextToSpeech& operator=(const TextToSpeech&) = delete;

    // Delete move semantics
    TextToSpeech(TextToSpeech&&) = delete;
    TextToSpeech& operator=(TextToSpeech&&) = delete;

    /**
     * @brief Initialize TTS client
     * @return true if initialization successful
     * @note Must be called once during setup() after WiFi connected
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Main update loop - process state machine and timeouts
     * @note Call regularly from loop(), non-blocking
     */
    void run() noexcept;

    /**
     * @brief Alias for run() for scheduler compatibility
     */
    void update() noexcept;

    /**
     * @brief Queue text for speech synthesis
     * @param text Text to speak
     * @param priority If true, insert at front of queue
     * @return true if queued successfully
     */
    [[nodiscard]] bool speak(const String& text, bool priority = false) noexcept;

    /**
     * @brief Stop current playback and clear queue
     */
    void stop() noexcept;

    /**
     * @brief Pause current playback
     */
    void pause() noexcept;

    /**
     * @brief Resume paused playback
     */
    void resume() noexcept;

    /**
     * @brief Clear speech queue
     */
    void clearQueue() noexcept;

    /**
     * @brief Set voice name
     * @param voice Voice identifier (e.g., "en-US-Standard-A")
     */
    void setVoice(const String& voice) noexcept;

    /**
     * @brief Set language code
     * @param language Language code (e.g., "en-US")
     */
    void setLanguage(const String& language) noexcept;

    /**
     * @brief Set speaking rate
     * @param speed Speed factor (0.25 - 4.0)
     */
    void setSpeed(float speed) noexcept;

    /**
     * @brief Set speaking pitch
     * @param pitch Pitch adjustment (-20.0 - 20.0 semitones)
     */
    void setPitch(float pitch) noexcept;

    /**
     * @brief Set playback volume
     * @param volume Volume (0-100)
     */
    void setVolume(uint8_t volume) noexcept;

    /**
     * @brief Set API authentication key
     * @param apiKey TTS API key
     * @note Should be called before initialize()
     */
    void setApiKey(const String& apiKey) noexcept;

    /**
     * @brief Set API endpoint URL
     * @param endpoint TTS API endpoint
     * @note Should be called before initialize()
     */
    void setApiEndpoint(const String& endpoint) noexcept;

    /**
     * @brief Set custom root CA certificate
     * @param caCert PEM-formatted certificate
     * @note For HTTPS verification
     */
    void setRootCA(const String& caCert) noexcept;

    /**
     * @brief Enable streaming playback mode
     */
    void enableStreaming() noexcept;

    /**
     * @brief Disable streaming playback mode
     */
    void disableStreaming() noexcept;

    /**
     * @brief Check if TTS is busy (processing or playing)
     * @return true if not IDLE
     */
    [[nodiscard]] bool isBusy() const noexcept;

    /**
     * @brief Check if currently playing audio
     * @return true if in PLAYING state
     */
    [[nodiscard]] bool isPlaying() const noexcept;

    /**
     * @brief Check if module is initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Get current state
     * @return Current TTSState
     */
    [[nodiscard]] TTSState getState() const noexcept;

    /**
     * @brief Get last error code
     * @return Current TTSError
     */
    [[nodiscard]] TTSError getError() const noexcept;

    /**
     * @brief Get last response metadata
     * @return Const reference to TTSResponse
     */
    [[nodiscard]] const TTSResponse& getResponse() const noexcept;

    /**
     * @brief Get queue size
     * @return Number of items in queue
     */
    [[nodiscard]] size_t getQueueSize() const noexcept;

private:
    // State management
    void changeState(TTSState newState) noexcept;
    void setError(TTSError error) noexcept;
    void resetRequest() noexcept;

    // Connection management
    bool connectToApi() noexcept;
    void disconnectFromApi() noexcept;

    // Request/Response
    String buildRequest() noexcept;
    bool sendRequest(const String& body) noexcept;
    String readResponse() noexcept;
    bool parseResponse(const String& response) noexcept;

    // Audio processing
    bool decodeAudio(const String& base64Audio) noexcept;
    bool playAudio() noexcept;
    void stopPlayback() noexcept;

    // Queue management
    void processQueue() noexcept;
    void advanceQueue() noexcept;

    // Timeouts
    void checkTimeouts() noexcept;
    bool isConnectionTimeout() const noexcept;
    bool isRequestTimeout() const noexcept;
    bool isPlaybackTimeout() const noexcept;

    // Member variables
    bool m_initialized;
    TTSState m_currentState;
    TTSError m_lastError;
    TTSResponse m_response;

    // Configuration
    char m_apiKey[128];
    char m_endpoint[128];
    char m_voice[64];
    char m_language[16];
    char m_rootCA[2048];
    float m_speed;
    float m_pitch;
    uint8_t m_volume;
    bool m_streaming;

    // Audio buffer
    static constexpr size_t kMaxAudioBuffer = 65536; // 64KB
    uint8_t* m_audioBuffer;
    size_t m_audioBufferSize;
    size_t m_audioBufferCapacity;
    size_t m_playbackOffset;

    // Playback queue
    std::vector<TTSQueueItem> m_queue;
    static constexpr size_t kMaxQueueSize = 20;

    // Timing
    unsigned long m_requestStartTime;
    unsigned long m_connectionStartTime;
    unsigned long m_playbackStartTime;

    // Network
    WiFiClientSecure m_client;

    // HTTP parsing
    bool m_headersParsed;
    int m_contentLength;
    bool m_chunkedEncoding;
    String m_responseBuffer;

    // Paused state
    bool m_paused;
    size_t m_pausedOffset;

    // Constants
    static constexpr unsigned long kDefaultTimeoutMs = 30000UL;
    static constexpr unsigned long kConnectionTimeoutMs = 10000UL;
    static constexpr unsigned long kPlaybackTimeoutMs = 300000UL; // 5 minutes
    static constexpr float kDefaultSpeed = 0.9f;
    static constexpr float kDefaultPitch = -2.0f;
    static constexpr uint8_t kDefaultVolume = 80;
    static constexpr const char* kDefaultVoice = "en-GB-Neural2-B";
    static constexpr const char* kDefaultLanguage = "en-GB";
};

/**
 * @brief Global TTS instance
 */
extern TextToSpeech textToSpeech;

#endif // AURA_TEXT_TO_SPEECH_H