#ifndef AURA_SPEECH_TO_TEXT_H
#define AURA_SPEECH_TO_TEXT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "audio_manager.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "config.h"
#include "logger.h"
#include "secrets.h"

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
 * @class SpeechToText
 * @brief Single authority for speech recognition
 *
 * Manages:
 * - Speech recognition session lifecycle
 * - Audio buffering from AudioManager with VAD integration
 * - HTTPS communication with speech API (REST & streaming)
 * - Request/response handling with ArduinoJson
 * - Voice Activity Detection coordination
 * - Language and configuration management
 * - Timeout, retry, and error handling
 *
 * Non-blocking, production-quality speech recognition for ESP32.
 */
class SpeechToText {
public:
    /**
     * @brief Constructor
     */
    SpeechToText() noexcept;

    /**
     * @brief Destructor
     */
    ~SpeechToText() noexcept;

    // Delete copy semantics
    SpeechToText(const SpeechToText&) = delete;
    SpeechToText& operator=(const SpeechToText&) = delete;

    // Delete move semantics
    SpeechToText(SpeechToText&&) = delete;
    SpeechToText& operator=(SpeechToText&&) = delete;

    /**
     * @brief Initialize speech recognition module
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
     * @brief Start speech recognition session
     * @param mode Recognition mode (default: ONESHOT)
     * @return true if started successfully
     * @note Begins recording audio from AudioManager
     */
    [[nodiscard]] bool startRecognition(RecognitionMode mode = RecognitionMode::ONESHOT) noexcept;

    /**
     * @brief Stop recording and process accumulated audio
     * @return true if audio sent for processing
     * @note Transitions to PROCESSING state
     */
    [[nodiscard]] bool stopRecognition() noexcept;

    /**
     * @brief Cancel current recognition session
     * @note Aborts recording/processing, returns to IDLE
     */
    void cancelRecognition() noexcept;

    /**
     * @brief Process incoming audio data from AudioManager callback
     * @param audioData Pointer to PCM16 audio buffer
     * @param length Number of samples (not bytes)
     * @note Non-blocking, called from audio DMA interrupt context
     */
    void processAudio(const int16_t* audioData, size_t length) noexcept;

    /**
     * @brief Process audio chunk with timestamp (for streaming)
     * @param chunk AudioChunk containing data and metadata
     */
    void processAudioChunk(const AudioChunk& chunk) noexcept;

    /**
     * @brief Check if actively recording audio
     * @return true if in LISTENING state
     */
    [[nodiscard]] bool isRecognizing() const noexcept;

    /**
     * @brief Check if recognition is busy (recording or processing)
     * @return true if LISTENING or PROCESSING
     */
    [[nodiscard]] bool isBusy() const noexcept;

    /**
     * @brief Check if module is initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Get last recognition result
     * @return Const reference to SpeechResult
     * @note Valid after COMPLETED state or during LISTENING for partials
     */
    [[nodiscard]] const SpeechResult& getResult() const noexcept;

    /**
     * @brief Clear current recognition result
     */
    void clearResult() noexcept;

    /**
     * @brief Set recognition language
     * @param language Language code (e.g., "en-US", "es-ES")
     * @note Must be called before startRecognition()
     */
    void setLanguage(const String& language) noexcept;

    /**
     * @brief Set recognition timeout
     * @param timeoutMs Maximum duration in milliseconds (0 = no timeout)
     * @note Default: 30000ms
     */
    void setTimeout(unsigned long timeoutMs) noexcept;

    /**
     * @brief Set audio sample rate
     * @param sampleRate Sample rate in Hz (must match AudioManager)
     * @note Default: 16000 Hz
     */
    void setSampleRate(uint32_t sampleRate) noexcept;

    /**
     * @brief Set audio encoding format
     * @param format Format string (e.g., "pcm16", "opus")
     * @note Must be supported by speech API
     */
    void setAudioFormat(AudioFormat format) noexcept;

    /**
     * @brief Set recognition mode
     * @param mode RecognitionMode enum value
     */
    void setMode(RecognitionMode mode) noexcept;

    /**
     * @brief Get current speech state
     * @return Current SpeechState
     */
    [[nodiscard]] SpeechState getState() const noexcept;

    /**
     * @brief Get last error code
     * @return Current SpeechError
     */
    [[nodiscard]] SpeechError getError() const noexcept;

    /**
     * @brief Get current API connection state
     * @return Current ApiState
     */
    [[nodiscard]] ApiState getApiState() const noexcept;

    /**
     * @brief Set API endpoint URL
     * @param endpoint Speech API endpoint
     * @note Should be called before initialize()
     */
    void setApiEndpoint(const String& endpoint) noexcept;

    /**
     * @brief Set API authentication key
     * @param apiKey Speech API key
     * @note Should be called before initialize()
     */
    void setApiKey(const String& apiKey) noexcept;

    /**
     * @brief Set custom root CA certificate
     * @param caCert PEM-formatted certificate
     * @note For HTTPS verification
     */
    void setRootCA(const String& caCert) noexcept;

    /**
     * @brief Enable/disable partial results during recognition
     * @param enable True to receive interim results
     */
    void setPartialResults(bool enable) noexcept;

    /**
     * @brief Enable/disable profanity filter
     * @param enable True to filter profanity
     */
    void setProfanityFilter(bool enable) noexcept;

    /**
     * @brief Set maximum alternatives to return
     * @param max Number of alternatives (1-10)
     */
    void setMaxAlternatives(uint8_t max) noexcept;

    /**
     * @brief Get audio buffer statistics
     * @param[out] usedBytes Current buffer usage
     * @param[out] capacityBytes Total buffer capacity
     */
    void getBufferStats(size_t& usedBytes, size_t& capacityBytes) const noexcept;

    /**
     * @brief Check if buffer is near capacity
     * @return true if buffer > 90% full
     */
    [[nodiscard]] bool isBufferNearFull() const noexcept;

    /**
     * @brief Force flush audio buffer to API (streaming mode)
     * @return true if flush initiated
     */
    [[nodiscard]] bool flushBuffer() noexcept;

    [[nodiscard]] bool sendAudio() noexcept;

private:
    // State management
    void changeState(SpeechState newState) noexcept;
    void setError(SpeechError error) noexcept;
    void resetSession() noexcept;

    // Buffer management
    bool ensureCapacity(size_t additionalBytes) noexcept;
    void appendAudio(const int16_t* data, size_t samples) noexcept;
    void resetBuffer() noexcept;

    // Request/Response
    String buildRequestBody() noexcept;
    bool parseResponse(const String& response) noexcept;
    bool parseStreamingResponse(const String& chunk) noexcept;

    // Network
    bool connectToApi() noexcept;
    void disconnectApi() noexcept;
    bool sendRequest(const String& body) noexcept;
    String readResponse() noexcept;
    bool handleHttpResponse(int statusCode, const String& body) noexcept;

    // Timeouts
    void checkTimeouts() noexcept;
    bool isRecognitionTimeout() const noexcept;
    bool isRequestTimeout() const noexcept;

    // Member variables
    bool m_initialized;
    SpeechState m_currentState;
    SpeechError m_lastError;
    ApiState m_apiState;
    RecognitionMode m_mode;
    SpeechResult m_result;

    // Audio buffer (fixed capacity, no fragmentation)
    static constexpr size_t kMaxBufferSize = 65536;  // 64KB max
    static constexpr size_t kDefaultBufferSize = 32768; // 32KB default
    int16_t* m_audioBuffer;
    size_t m_bufferCapacity;
    size_t m_bufferLength;
    bool m_bufferOverflow;

    // Configuration
    char m_language[16];           // Fixed buffer for language code
    AudioFormat m_audioFormat;        // Fixed buffer for format
    uint32_t m_sampleRate;
    unsigned long m_timeoutMs;
    char m_apiEndpoint[128];       // Fixed buffer for endpoint
    char m_apiKey[256];             // Fixed buffer for API key
    char m_rootCA[2048];           // Fixed buffer for CA cert
    bool m_partialResultsEnabled;
    bool m_profanityFilter;
    uint8_t m_maxAlternatives;

    // Timing
    unsigned long m_sessionStartTime;
    unsigned long m_lastAudioTime;
    unsigned long m_requestStartTime;

    // Network
    WiFiClientSecure m_client;

    // Constants
    static constexpr unsigned long kDefaultTimeoutMs = 30000UL;
    static constexpr uint32_t kDefaultSampleRate = 16000;
    static constexpr const char* kDefaultLanguage = "en-US";
    static constexpr const char* kDefaultAudioFormat = "pcm16";
    static constexpr unsigned long kRequestTimeoutMs = 15000UL;
    static constexpr unsigned long kConnectionTimeoutMs = 10000UL;
};

/**
 * @brief Global speech-to-text instance
 */
extern SpeechToText speechToText;

#endif // AURA_SPEECH_TO_TEXT_H