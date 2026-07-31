#ifndef AURA_GEMINI_CLIENT_H
#define AURA_GEMINI_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include "wifi_manager.h"
#include "storage_manager.h"
#include "config.h"
#include "logger.h"
#include "secrets.h"

enum class GeminiState : uint8_t {
    IDLE,
    CONNECTING,
    SENDING,
    WAITING,
    RECEIVING,
    COMPLETED,
    ERROR
};

enum class GeminiError : uint8_t {
    NONE,
    NETWORK,
    TIMEOUT,
    API_ERROR,
    JSON_ERROR,
    AUTHENTICATION,
    RATE_LIMIT,
    SAFETY,
    TLS_ERROR,
    CANCELLED,
    UNKNOWN
};

enum class CircuitBreakerState : uint8_t {
    CLOSED,
    OPEN,
    HALF_OPEN
};

struct Telemetry {
    unsigned long avgLatencyMs;
    unsigned long lastLatencyMs;
    unsigned long fastestLatencyMs;
    unsigned long slowestLatencyMs;
    uint32_t totalRequests;
    uint32_t successfulRequests;
    uint32_t failedRequests;
    uint32_t retryCount;
    float cacheHitRate;
    uint32_t cacheHits;
    uint32_t cacheMisses;
    uint32_t inputTokens;
    uint32_t outputTokens;
    uint32_t totalTokens;

    Telemetry() noexcept
        : avgLatencyMs(0), lastLatencyMs(0), fastestLatencyMs(UINT32_MAX),
          slowestLatencyMs(0), totalRequests(0), successfulRequests(0),
          failedRequests(0), retryCount(0), cacheHitRate(0.0f),
          cacheHits(0), cacheMisses(0), inputTokens(0), outputTokens(0),
          totalTokens(0) {}

    void clear() noexcept {
        avgLatencyMs = 0;
        lastLatencyMs = 0;
        fastestLatencyMs = UINT32_MAX;
        slowestLatencyMs = 0;
        totalRequests = 0;
        successfulRequests = 0;
        failedRequests = 0;
        retryCount = 0;
        cacheHitRate = 0.0f;
        cacheHits = 0;
        cacheMisses = 0;
        inputTokens = 0;
        outputTokens = 0;
        totalTokens = 0;
    }
};

struct GeminiResponse {
    String responseText;
    String finishReason;
    int promptTokens;
    int responseTokens;
    int totalTokens;
    unsigned long latencyMs;
    unsigned long timestamp;
    GeminiError error;

    GeminiResponse() noexcept
        : responseText(""), finishReason(""), promptTokens(0), responseTokens(0),
          totalTokens(0), latencyMs(0), timestamp(0), error(GeminiError::NONE) {}

    void clear() noexcept {
        responseText.clear();
        finishReason.clear();
        promptTokens = 0;
        responseTokens = 0;
        totalTokens = 0;
        latencyMs = 0;
        timestamp = 0;
        error = GeminiError::NONE;
    }
};

struct ConversationTurn {
    String role;
    String text;
    int estimatedTokens;

    ConversationTurn() noexcept : role(""), text(""), estimatedTokens(0) {}
    ConversationTurn(const String& r, const String& t, int tok = 0) noexcept
        : role(r), text(t), estimatedTokens(tok) {}
};

class GeminiClient {
public:
    GeminiClient() noexcept;
    ~GeminiClient() noexcept;

    GeminiClient(const GeminiClient&) = delete;
    GeminiClient& operator=(const GeminiClient&) = delete;
    GeminiClient(GeminiClient&&) = delete;
    GeminiClient& operator=(GeminiClient&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void run() noexcept;
    void update() noexcept;
    [[nodiscard]] bool sendPrompt(const String& prompt) noexcept;
    void cancelRequest() noexcept;
    void clearConversation() noexcept;
    void clearResponse() noexcept;
    void setSystemPrompt(const String& systemPrompt) noexcept;
    void setApiKey(const String& apiKey) noexcept;
    [[nodiscard]] bool hasApiKey() const noexcept;
    [[nodiscard]] String getMaskedApiKey() const noexcept;
    void setApiEndpoint(const String& endpoint) noexcept;
    void setTemperature(float temperature) noexcept;
    void setTopP(float topP) noexcept;
    void setTopK(int topK) noexcept;
    void setMaxTokens(int maxTokens) noexcept;
    void setTimeout(unsigned long timeoutMs) noexcept;
    void enableStreaming() noexcept;
    void disableStreaming() noexcept;
    void setRootCA(const String& caCert) noexcept;
    void setSafetySetting(const String& category, const String& threshold) noexcept;
    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] GeminiState getState() const noexcept;
    [[nodiscard]] GeminiError getError() const noexcept;
    [[nodiscard]] const GeminiResponse& getResponse() const noexcept;
    [[nodiscard]] const std::vector<ConversationTurn>& getConversation() const noexcept;
    void addConversationTurn(const String& role, const String& text) noexcept;

    void setCacheEnabled(bool enabled) noexcept;


    [[nodiscard]] const Telemetry& getTelemetry() const noexcept;
    [[nodiscard]] CircuitBreakerState getCircuitBreakerState() const noexcept;
    void setCircuitCooldown(unsigned long ms) noexcept;

private:
    // State machine handlers
    void handleConnecting() noexcept;
    void handleSending() noexcept;
    void handleWaiting() noexcept;
    void handleReceiving() noexcept;

    // Core helpers
    void changeState(GeminiState newState) noexcept;
    void setError(GeminiError error) noexcept;
    void resetRequest() noexcept;

    // Networking
    bool connectToApi() noexcept;
    void disconnectFromApi() noexcept;
    bool sendRequest(const String& body) noexcept;
    bool parseHttpHeaders() noexcept;
    void readAvailableData() noexcept;
    bool isBodyComplete() noexcept;

    // Request/response processing
    String buildRequest() noexcept;
    bool parseResponse(const String& response) noexcept;
    bool parseStreamingChunk(const String& chunk) noexcept;
    int parseStatusCode(const String& line) const noexcept;
    bool handleHttpResponse(int statusCode) noexcept;

    // Streaming
    void processSseEvents() noexcept;

    // Retry / backoff
    bool shouldRetry() const noexcept;
    void applyBackoff() noexcept;
    bool isBackoffComplete() const noexcept;
    unsigned long currentBackoffDelay() const noexcept;

    // Timeouts
    void checkTimeouts() noexcept;
    bool isConnectionTimedOut() const noexcept;
    bool isRequestTimedOut() const noexcept;

    // Conversation management
    void trimConversation() noexcept;

    // Token estimation
    int estimateTokens(const String& text) const noexcept;

    // Prompt loading
    void loadPrompts() noexcept;
    bool tryLoadPrompt(const char* path, String& output) const noexcept;

    // Response cache
    uint32_t hashString(const String& str) const noexcept;
    String buildCacheKey(const String& prompt) const noexcept;
    bool lookupCache(const String& key, String& value) const noexcept;
    void storeCache(const String& key, const String& value) noexcept;

    // Circuit breaker
    void recordSuccess() noexcept;
    void recordFailure() noexcept;

    // API key management
    bool loadApiKeyFromNVS() noexcept;
    bool saveApiKeyToNVS() noexcept;
    static String maskApiKey(const String& key) noexcept;

    // Persistent conversation history
    void saveConversationHistory() noexcept;
    void loadConversationHistory() noexcept;

    // Automatic prompt reload
    void checkPromptReload() noexcept;

    // Offline fallback
    String getOfflineResponse(const String& prompt) const noexcept;

    // State
    bool m_initialized;
    GeminiState m_currentState;
    GeminiError m_lastError;
    GeminiResponse m_response;

    // Configuration
    String m_apiKey;
    String m_endpoint;
    String m_systemPrompt;
    String m_rootCA;
    float m_temperature;
    float m_topP;
    int m_topK;
    int m_maxTokens;
    unsigned long m_timeoutMs;
    bool m_streaming;

    // Connection
    WiFiClientSecure m_client;

    // Request metadata
    unsigned long m_requestStartTime;
    unsigned long m_connectionStartTime;
    unsigned long m_lastActivityTime;
    String m_requestBuffer;
    String m_lastPrompt;

    // HTTP response parsing
    bool m_headersParsed;
    int m_contentLength;
    String m_headerLine;
    bool m_chunkedEncoding;
    bool m_bodyComplete;
    int m_httpStatusCode;
    String m_responseBuffer;

    // Streaming
    String m_streamingBuffer;
    bool m_streamComplete;
    String m_safetySettings;

    // Conversation
    std::vector<ConversationTurn> m_conversation;
    int m_contextTokens;

    // Retry
    uint8_t m_retryCount;
    unsigned long m_retryStartTime;

    // Cache
    bool m_cacheEnabled;
    static constexpr uint8_t kCacheCapacity = GEMINI_CACHE_SIZE;
    uint32_t m_cacheHashes[kCacheCapacity];
    String m_cacheResponses[kCacheCapacity];
    unsigned long m_cacheTimestamps[kCacheCapacity];
    uint8_t m_cacheCount;

    // Circuit breaker
    CircuitBreakerState m_circuitState;
    uint8_t m_circuitFailures;
    unsigned long m_circuitOpenTime;
    unsigned long m_circuitCooldownMs;

    // Persistent history
    bool m_historyEnabled;
    unsigned long m_lastHistorySaveTime;

    // Auto prompt reload
    bool m_promptLoadedFromFile;
    unsigned long m_lastPromptReloadCheck;
    uint32_t m_systemPromptVersion;

    // NVS
    Preferences m_preferences;
    static constexpr const char* kNvsNamespace = "aurasec";

    // Telemetry
    Telemetry m_telemetry;

    // Platform constants
    static constexpr unsigned long kDefaultTimeoutMs = GEMINI_TIMEOUT_MS;
    static constexpr float kDefaultTemperature = static_cast<float>(GEMINI_TEMPERATURE);
    static constexpr float kDefaultTopP = static_cast<float>(GEMINI_TOP_P);
    static constexpr int kDefaultTopK = GEMINI_TOP_K;
    static constexpr int kDefaultMaxTokens = GEMINI_MAX_TOKENS;
    static constexpr unsigned long kConnectionTimeoutMs = GEMINI_CONNECT_TIMEOUT_MS;
    static constexpr unsigned long kRetryBaseDelayMs = GEMINI_RETRY_BASE_DELAY_MS;
    static constexpr unsigned long kRetryMaxDelayMs = GEMINI_RETRY_MAX_DELAY_MS;
    static constexpr uint8_t kMaxRetries = GEMINI_RETRY_MAX;
    static constexpr int kMaxContextTokens = GEMINI_CONVERSATION_MAX_TOKENS;
    static constexpr size_t kMaxConversationTurns = 20;
    static constexpr uint32_t kBufferReserveSize = GEMINI_BUFFER_SIZE;
    static constexpr uint8_t kCircuitThreshold = 3;
    static constexpr unsigned long kCircuitCooldownMs = 30000UL;
    static constexpr unsigned long kPromptReloadIntervalMs = 5000UL;
};

extern GeminiClient geminiClient;

#endif
