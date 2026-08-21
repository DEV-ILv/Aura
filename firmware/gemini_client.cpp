#include "gemini_client.h"
#include <esp_task_wdt.h>

namespace {

constexpr const char* kTag = "GeminiClient";

constexpr const char* kDefaultSystemPrompt =
    "You are AURA, an AI desktop assistant. Be concise, helpful, and friendly.";

constexpr const char* kApiHostPlaceholder = "generativelanguage.googleapis.com";

constexpr const char* kConversationFile = "/conversation.json";

constexpr size_t kMaxLoadedHistoryTurns = 10;

struct OfflineResponse {
    const char* keyword;
    const char* response;
};

constexpr OfflineResponse kOfflineResponses[] = {
    {"hello",                 "Hello! I am AURA. I am currently offline, but I am here to assist you as soon as my connection is restored."},
    {"hi",                    "Hi there! I am currently offline. Please check your internet connection and try again."},
    {"who are you",           "I am AURA, your AI desktop assistant. I am currently offline and unable to access my full capabilities."},
    {"what can you do",       "I can answer questions, help with tasks, and control your devices. Right now I am offline, so my responses are limited."},
    {"help",                  "I am currently offline. Please ensure your device is connected to the internet and try again."},
    {"thank",                 "You are welcome! I will be happy to help more once I am back online."},
    {"goodbye",               "Goodbye! Stay connected so I can assist you when I am back online."},
    {"time",                  "I cannot check the current time right now as I am offline."},
    {"weather",               "I cannot check the weather right now as I am offline."},
    {"offline",               "Yes, I am currently offline. I will resume normal operation once the connection is restored."},
};

constexpr size_t kOfflineCount = sizeof(kOfflineResponses) / sizeof(kOfflineResponses[0]);

int charToInt(char c) noexcept {
    int v = static_cast<int>(c);
    return (v >= 48 && v <= 57) ? v - 48 : 0;
}

} // anonymous namespace

GeminiClient geminiClient;

GeminiClient::GeminiClient() noexcept
    : m_initialized(false)
    , m_currentState(GeminiState::IDLE)
    , m_lastError(GeminiError::NONE)
    , m_response()
    , m_temperature(kDefaultTemperature)
    , m_topP(kDefaultTopP)
    , m_topK(kDefaultTopK)
    , m_maxTokens(kDefaultMaxTokens)
    , m_timeoutMs(kDefaultTimeoutMs)
    , m_streaming(false)

    , m_requestStartTime(0)
    , m_connectionStartTime(0)
    , m_lastActivityTime(0)
    , m_headersParsed(false)
    , m_contentLength(-1)
    , m_headerLine()
    , m_chunkedEncoding(false)
    , m_bodyComplete(false)
    , m_httpStatusCode(0)
    , m_streamComplete(false)
    , m_contextTokens(0)
    , m_retryCount(0)
    , m_retryStartTime(0)
    , m_cacheEnabled(false)
    , m_cacheCount(0)
    , m_circuitState(CircuitBreakerState::CLOSED)
    , m_circuitFailures(0)
    , m_circuitOpenTime(0)
    , m_circuitCooldownMs(kCircuitCooldownMs)
    , m_historyEnabled(false)
    , m_lastHistorySaveTime(0)
    , m_promptLoadedFromFile(false)
    , m_lastPromptReloadCheck(0)
    , m_systemPromptVersion(0)
    , m_apiKey(Secrets::GEMINI_API_KEY) {
}

GeminiClient::~GeminiClient() noexcept {
    disconnectFromApi();
}

// ============================================================================
// Initialization
// ============================================================================

bool GeminiClient::initialize() noexcept {
    if (m_initialized) {
        return true;
    }

    if (!wifiManager.isConnected()) {
        LOG_ERROR(kTag, "WiFi not connected");
        setError(GeminiError::NETWORK);
        return false;
    }

    if (m_apiKey.isEmpty()) {
        LOG_ERROR(kTag, "API key not set");
        setError(GeminiError::AUTHENTICATION);
        return false;
    }

    if (m_endpoint.isEmpty()) {
        LOG_ERROR(kTag, "API endpoint not set");
        setError(GeminiError::API_ERROR);
        return false;
    }

    if (!m_rootCA.isEmpty()) {
        m_client.setCACert(m_rootCA.c_str());
    } else {
        LOG_ERROR(kTag, "No root CA certificate configured — TLS verification required");
        setError(GeminiError::TLS_ERROR);
        return false;
    }

    m_client.setTimeout(kConnectionTimeoutMs / 1000UL);

    // Load API key from NVS; fall back to compiled-in default
    if (!loadApiKeyFromNVS()) {
        if (!m_apiKey.isEmpty()) {
            saveApiKeyToNVS();
        }
    }
    if (!m_apiKey.isEmpty()) {
        LOG_INFO(kTag, "API key: %s", maskApiKey(m_apiKey).c_str());
    }

    loadPrompts();

    m_historyEnabled = storageManager.isSDMounted();
    if (m_historyEnabled) {
        loadConversationHistory();
    }

    m_initialized = true;
    m_lastError = GeminiError::NONE;
    LOG_INFO(kTag, "Initialized");
    return true;
}

// ============================================================================
// Prompt loading (SPIFFS -> SD -> built-in fallback)
// ============================================================================

bool GeminiClient::tryLoadPrompt(const char* path, String& output) const noexcept {
    return storageManager.readFile(path, output, StorageType::SPIFFS) == StorageStatus::SUCCESS
           && !output.isEmpty();
}

void GeminiClient::loadPrompts() noexcept {
    String content;

    if (tryLoadPrompt("/prompts/system.txt", content)) {
        m_systemPrompt = content;
        m_promptLoadedFromFile = true;
        ++m_systemPromptVersion;
        LOG_INFO(kTag, "Loaded system prompt from /prompts/system.txt");
        return;
    }

    if (storageManager.isSDMounted()) {
        content.clear();
        if (storageManager.readFile("/prompts/system.txt", content, StorageType::SD_CARD)
            == StorageStatus::SUCCESS && !content.isEmpty()) {
            m_systemPrompt = content;
            m_promptLoadedFromFile = true;
            ++m_systemPromptVersion;
            LOG_INFO(kTag, "Loaded system prompt from SD card");
            return;
        }
    }

    m_systemPrompt = kDefaultSystemPrompt;
    m_promptLoadedFromFile = false;
    LOG_INFO(kTag, "Using built-in system prompt");
}

// ============================================================================
// Automatic prompt reload
// ============================================================================

void GeminiClient::checkPromptReload() noexcept {
    if (!m_promptLoadedFromFile) {
        return;
    }

    String content;
    bool found = false;

    if (tryLoadPrompt("/prompts/system.txt", content)) {
        found = true;
    } else if (storageManager.isSDMounted()) {
        content.clear();
        if (storageManager.readFile("/prompts/system.txt", content, StorageType::SD_CARD)
            == StorageStatus::SUCCESS && !content.isEmpty()) {
            found = true;
        }
    }

    if (found) {
        if (content != m_systemPrompt) {
            m_systemPrompt = content;
            ++m_systemPromptVersion;
            LOG_INFO(kTag, "System prompt reloaded (version %lu)", m_systemPromptVersion);
        }
    } else {
        m_systemPrompt = kDefaultSystemPrompt;
        m_promptLoadedFromFile = false;
        LOG_INFO(kTag, "Prompt file unavailable, using built-in fallback");
    }
}

// ============================================================================
// State machine – entry
// ============================================================================

void GeminiClient::run() noexcept {
    if (!m_initialized) {
        return;
    }

    // Feed the task watchdog at the start of each cycle (loop task)
    esp_task_wdt_reset();

    checkTimeouts();

    const unsigned long now = millis();
    if (now - m_lastPromptReloadCheck >= kPromptReloadIntervalMs) {
        m_lastPromptReloadCheck = now;
        checkPromptReload();
    }

    switch (m_currentState) {
        case GeminiState::CONNECTING: handleConnecting(); break;
        case GeminiState::SENDING:    handleSending();    break;
        case GeminiState::WAITING:    handleWaiting();    break;
        case GeminiState::RECEIVING:  handleReceiving();  break;
        case GeminiState::COMPLETED:
            if (!m_streaming) {
                changeState(GeminiState::IDLE);
            }
            break;
        default:
            break;
    }
}

void GeminiClient::update() noexcept {
    run();
}

// ============================================================================
// State machine – handlers
// ============================================================================

void GeminiClient::handleConnecting() noexcept {
    if (m_retryCount > 0 && !isBackoffComplete()) {
        return;
    }

    if (connectToApi()) {
        m_retryCount = 0;
        m_retryStartTime = 0;
        changeState(GeminiState::SENDING);
        return;
    }

    if (shouldRetry()) {
        applyBackoff();
        LOG_WARN(kTag, "Connection failed, retry %d/%d in %lu ms",
            static_cast<int>(m_retryCount), static_cast<int>(kMaxRetries),
            currentBackoffDelay());
        return;
    }

    setError(GeminiError::NETWORK);
    changeState(GeminiState::ERROR);
}

void GeminiClient::handleSending() noexcept {
    if (sendRequest(buildRequest())) {
        m_retryCount = 0;
        m_retryStartTime = 0;
        changeState(GeminiState::WAITING);
        m_requestStartTime = millis();
        LOG_DEBUG(kTag, "Request sent (%u bytes)", m_requestBuffer.length());
        return;
    }

    if (shouldRetry()) {
        applyBackoff();
        disconnectFromApi();
        changeState(GeminiState::CONNECTING);
        LOG_WARN(kTag, "Send failed, retry %d/%d",
            static_cast<int>(m_retryCount), static_cast<int>(kMaxRetries));
        return;
    }

    disconnectFromApi();
    setError(GeminiError::NETWORK);
    changeState(GeminiState::ERROR);
}

void GeminiClient::handleWaiting() noexcept {
    esp_task_wdt_reset();
    if (!parseHttpHeaders()) {
        if (!m_client.connected() && !m_client.available()) {
            if (shouldRetry()) {
                applyBackoff();
                changeState(GeminiState::CONNECTING);
                LOG_WARN(kTag, "Connection lost before headers, retry %d/%d",
                    static_cast<int>(m_retryCount), static_cast<int>(kMaxRetries));
            } else {
                setError(GeminiError::NETWORK);
                changeState(GeminiState::ERROR);
            }
        }
        return;
    }

    if (m_httpStatusCode >= 400) {
        readAvailableData();
        handleHttpResponse(m_httpStatusCode);
        if (shouldRetry()) {
            applyBackoff();
            disconnectFromApi();
            changeState(GeminiState::CONNECTING);
            return;
        }
        changeState(GeminiState::ERROR);
        return;
    }

    changeState(GeminiState::RECEIVING);

    if (m_streaming) {
        m_streamingBuffer.clear();
        m_streamComplete = false;
    }
}

void GeminiClient::handleReceiving() noexcept {
    esp_task_wdt_reset();
    if (m_client.connected() || m_client.available()) {
        readAvailableData();
    }

    if (m_streaming) {
        processSseEvents();

        if (m_streamComplete || (!m_client.connected() && !m_client.available())) {
            if (m_lastError == GeminiError::NONE) {
                recordSuccess();
                saveConversationHistory();
            }
            changeState(GeminiState::COMPLETED);
            LOG_INFO(kTag, "Stream complete");
        }
        return;
    }

    if (!isBodyComplete()) {
        return;
    }

    if (parseResponse(m_responseBuffer)) {
        recordSuccess();
        saveConversationHistory();
        m_responseBuffer.clear();
        changeState(GeminiState::COMPLETED);
    } else if (m_lastError == GeminiError::SAFETY) {
        changeState(GeminiState::ERROR);
    } else if (shouldRetry()) {
        applyBackoff();
        disconnectFromApi();
        m_responseBuffer.clear();
        changeState(GeminiState::CONNECTING);
    } else {
        changeState(GeminiState::ERROR);
    }
}

// ============================================================================
// Public API – lifecycle
// ============================================================================

bool GeminiClient::sendPrompt(const String& prompt) noexcept {
    if (!m_initialized) {
        setError(GeminiError::UNKNOWN);
        return false;
    }

    if (isBusy()) {
        LOG_WARN(kTag, "Already busy (state=%d)", static_cast<int>(m_currentState));
        return false;
    }

    if (!wifiManager.isConnected()) {
        LOG_ERROR(kTag, "WiFi disconnected");
        setError(GeminiError::NETWORK);
        return false;
    }

    if (prompt.isEmpty()) {
        LOG_ERROR(kTag, "Empty prompt rejected");
        setError(GeminiError::API_ERROR);
        return false;
    }

    m_telemetry.totalRequests++;

    if (m_circuitState == CircuitBreakerState::OPEN) {
        const unsigned long cooldownElapsed = millis() - m_circuitOpenTime;
        if (cooldownElapsed >= m_circuitCooldownMs) {
            m_circuitState = CircuitBreakerState::HALF_OPEN;
            LOG_INFO(kTag, "Circuit half-open, allowing test request");
        } else {
            m_response.responseText = getOfflineResponse(prompt);
            m_response.latencyMs = 0;
            m_response.timestamp = millis();
            m_response.error = GeminiError::NONE;
            addConversationTurn("user", prompt);
            addConversationTurn("model", m_response.responseText);
            m_telemetry.successfulRequests++;
            changeState(GeminiState::COMPLETED);
            LOG_INFO(kTag, "Offline fallback response for: %s", prompt.c_str());
            return true;
        }
    }

    const String cacheKey = buildCacheKey(prompt);
    if (m_cacheEnabled && m_conversation.empty()) {
        String cached;
        if (lookupCache(cacheKey, cached)) {
            m_response.responseText = cached;
            m_response.latencyMs = 0;
            m_response.timestamp = millis();
            addConversationTurn("user", prompt);
            addConversationTurn("model", cached);
            m_telemetry.cacheHits++;
            m_telemetry.cacheHitRate = static_cast<float>(m_telemetry.cacheHits) /
                static_cast<float>(m_telemetry.cacheHits + m_telemetry.cacheMisses + 1);
            m_telemetry.successfulRequests++;
            changeState(GeminiState::COMPLETED);
            LOG_INFO(kTag, "Cache hit (%d chars)", prompt.length());
            return true;
        }
        m_telemetry.cacheMisses++;
        m_telemetry.cacheHitRate = static_cast<float>(m_telemetry.cacheHits) /
            static_cast<float>(m_telemetry.cacheHits + m_telemetry.cacheMisses);
    }

    m_lastPrompt = cacheKey;
    addConversationTurn("user", prompt);
    trimConversation();

    resetRequest();
    changeState(GeminiState::CONNECTING);
    m_connectionStartTime = millis();
    m_retryCount = 0;
    m_retryStartTime = 0;

    LOG_INFO(kTag, "Sending prompt (%d chars, ~%d context tokens)",
        prompt.length(), m_contextTokens);
    return true;
}

void GeminiClient::cancelRequest() noexcept {
    if (m_currentState == GeminiState::IDLE && m_retryCount == 0) {
        return;
    }

    disconnectFromApi();
    m_response.clear();
    m_responseBuffer.clear();
    m_streamingBuffer.clear();
    m_streamParseDoc.clear();
    m_retryCount = 0;
    m_retryStartTime = 0;
    m_streamComplete = false;
    m_bodyComplete = false;
    m_headersParsed = false;
    changeState(GeminiState::IDLE);
    LOG_INFO(kTag, "Request cancelled");
}

void GeminiClient::clearConversation() noexcept {
    m_conversation.clear();
    m_contextTokens = 0;
    LOG_DEBUG(kTag, "Conversation cleared");
}

void GeminiClient::clearResponse() noexcept {
    m_response.clear();
    m_lastError = GeminiError::NONE;
    m_responseTruncated = false;
}

// ============================================================================
// Public API – configuration setters
// ============================================================================

void GeminiClient::setSystemPrompt(const String& systemPrompt) noexcept {
    m_systemPrompt = systemPrompt;
    m_promptLoadedFromFile = false;
    ++m_systemPromptVersion;
}

void GeminiClient::setApiKey(const String& apiKey) noexcept {
    m_apiKey = apiKey;
    saveApiKeyToNVS();
    LOG_INFO(kTag, "API key updated: %s", maskApiKey(m_apiKey).c_str());
}

bool GeminiClient::loadApiKeyFromNVS() noexcept {
    m_preferences.begin(kNvsNamespace, true);
    String stored = m_preferences.getString("gemini_key", "");
    m_preferences.end();
    if (!stored.isEmpty()) {
        m_apiKey = stored;
        return true;
    }
    return false;
}

bool GeminiClient::saveApiKeyToNVS() noexcept {
    if (m_apiKey.isEmpty()) return false;
    m_preferences.begin(kNvsNamespace, false);
    bool ok = m_preferences.putString("gemini_key", m_apiKey);
    m_preferences.end();
    return ok;
}

bool GeminiClient::hasApiKey() const noexcept {
    return !m_apiKey.isEmpty();
}

String GeminiClient::getMaskedApiKey() const noexcept {
    return maskApiKey(m_apiKey);
}

String GeminiClient::maskApiKey(const String& key) noexcept {
    if (key.length() <= 8) {
        return String(key.length(), '*');
    }
    return key.substring(0, 4) + "****" + key.substring(key.length() - 3);
}

void GeminiClient::setApiEndpoint(const String& endpoint) noexcept {
    m_endpoint = endpoint;
}

void GeminiClient::setTemperature(float temperature) noexcept {
    m_temperature = (temperature < 0.0f) ? 0.0f : (temperature > 2.0f ? 2.0f : temperature);
}

void GeminiClient::setTopP(float topP) noexcept {
    m_topP = (topP < 0.0f) ? 0.0f : (topP > 1.0f ? 1.0f : topP);
}

void GeminiClient::setTopK(int topK) noexcept {
    m_topK = (topK < 1) ? 1 : (topK > 100 ? 100 : topK);
}

void GeminiClient::setMaxTokens(int maxTokens) noexcept {
    m_maxTokens = (maxTokens < 1) ? 1 : (maxTokens > 8192 ? 8192 : maxTokens);
}

void GeminiClient::setTimeout(unsigned long timeoutMs) noexcept {
    m_timeoutMs = timeoutMs;
}

void GeminiClient::enableStreaming() noexcept {
    m_streaming = true;
}

void GeminiClient::disableStreaming() noexcept {
    m_streaming = false;
}

void GeminiClient::setRootCA(const String& caCert) noexcept {
    m_rootCA = caCert;
}

void GeminiClient::setSafetySetting(const String& category, const String& threshold) noexcept {
    if (!m_safetySettings.isEmpty()) {
        m_safetySettings += ',';
    }
    m_safetySettings += "{\"category\":\"";
    m_safetySettings += category;
    m_safetySettings += "\",\"threshold\":\"";
    m_safetySettings += threshold;
    m_safetySettings += "\"}";
}

void GeminiClient::setCacheEnabled(bool enabled) noexcept {
    m_cacheEnabled = enabled;
    if (!enabled) {
        m_cacheCount = 0;
    }
}

void GeminiClient::setCircuitCooldown(unsigned long ms) noexcept {
    m_circuitCooldownMs = ms;
}

// ============================================================================
// Public API – getters
// ============================================================================

GeminiState GeminiClient::getState() const noexcept {
    return m_currentState;
}

GeminiError GeminiClient::getError() const noexcept {
    return m_lastError;
}

const GeminiResponse& GeminiClient::getResponse() const noexcept {
    return m_response;
}

const std::vector<ConversationTurn>& GeminiClient::getConversation() const noexcept {
    return m_conversation;
}

bool GeminiClient::isBusy() const noexcept {
    return m_currentState != GeminiState::IDLE && m_currentState != GeminiState::COMPLETED;
}

bool GeminiClient::isInitialized() const noexcept {
    return m_initialized;
}

const Telemetry& GeminiClient::getTelemetry() const noexcept {
    return m_telemetry;
}

CircuitBreakerState GeminiClient::getCircuitBreakerState() const noexcept {
    return m_circuitState;
}

// ============================================================================
// Conversation management
// ============================================================================

void GeminiClient::addConversationTurn(const String& role, const String& text) noexcept {
    if (role != "user" && role != "model") {
        return;
    }
    if (text.isEmpty()) {
        return;
    }

    while (m_conversation.size() >= kMaxConversationTurns) {
        m_contextTokens -= m_conversation.front().estimatedTokens;
        m_conversation.erase(m_conversation.begin());
    }

    const int tokens = estimateTokens(text);
    m_conversation.emplace_back(role, text, tokens);
    m_contextTokens += tokens;

    LOG_DEBUG(kTag, "Added %s turn (~%d tokens, total ~%d)",
        role.c_str(), tokens, m_contextTokens);
}

void GeminiClient::trimConversation() noexcept {
    while (m_contextTokens > kMaxContextTokens && m_conversation.size() > 1) {
        m_contextTokens -= m_conversation.front().estimatedTokens;
        m_conversation.erase(m_conversation.begin());
        LOG_DEBUG(kTag, "Trimmed one turn, context now ~%d tokens", m_contextTokens);
    }
}

// ============================================================================
// Persistent conversation history
// ============================================================================

void GeminiClient::saveConversationHistory() noexcept {
    if (!m_historyEnabled || m_conversation.empty()) {
        return;
    }

    JsonDocument doc;
    doc["version"] = 1;
    doc["timestamp"] = millis();

    JsonArray turns = doc["turns"].to<JsonArray>();
    for (const auto& turn : m_conversation) {
        JsonObject t = turns.add<JsonObject>();
        t["role"] = turn.role;
        t["text"] = turn.text;
        t["tokens"] = turn.estimatedTokens;
    }

    String json;
    serializeJson(doc, json);

    if (json.isEmpty()) {
        LOG_WARN(kTag, "Failed to serialize conversation history");
        return;
    }

    const StorageStatus status = storageManager.writeFile(
        kConversationFile, json, StorageType::SD_CARD);

    if (status == StorageStatus::SUCCESS) {
        m_lastHistorySaveTime = millis();
        LOG_DEBUG(kTag, "Conversation history saved (%u bytes)", json.length());
    } else {
        LOG_WARN(kTag, "Failed to save conversation history (%d)", static_cast<int>(status));
    }
}

void GeminiClient::loadConversationHistory() noexcept {
    if (!m_historyEnabled) {
        return;
    }

    String json;
    const StorageStatus status = storageManager.readFile(
        kConversationFile, json, StorageType::SD_CARD);

    if (status != StorageStatus::SUCCESS || json.isEmpty()) {
        LOG_DEBUG(kTag, "No conversation history to load");
        return;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json);
    if (err) {
        LOG_WARN(kTag, "Failed to parse conversation history: %s", err.c_str());
        return;
    }

    JsonArray turns = doc["turns"].as<JsonArray>();
    if (turns.size() == 0) {
        return;
    }

    size_t loaded = 0;
    size_t start = (turns.size() > kMaxLoadedHistoryTurns)
        ? turns.size() - kMaxLoadedHistoryTurns
        : 0;

    for (size_t i = start; i < turns.size(); ++i) {
        const JsonObject t = turns[i];
        const String role = t["role"] | "";
        const String text = t["text"] | "";
        const int tokens = t["tokens"] | 0;

        if (role.isEmpty() || text.isEmpty()) {
            continue;
        }

        m_conversation.emplace_back(role, text, tokens);
        m_contextTokens += tokens;
        ++loaded;
    }

    LOG_INFO(kTag, "Loaded %u conversation turns from history", loaded);
}

// ============================================================================
// Offline fallback
// ============================================================================

String GeminiClient::getOfflineResponse(const String& prompt) const noexcept {
    String lower = prompt;
    lower.toLowerCase();

    for (size_t i = 0; i < kOfflineCount; ++i) {
        if (lower.indexOf(kOfflineResponses[i].keyword) >= 0) {
            return String(kOfflineResponses[i].response);
        }
    }

    return String("I am AURA, your AI desktop assistant. I am currently offline and unable to process your request. Please check your internet connection and try again.");
}

// ============================================================================
// State machine helpers
// ============================================================================

void GeminiClient::changeState(GeminiState newState) noexcept {
    if (m_currentState == newState) {
        return;
    }

    static constexpr bool kValid[7][7] = {
        {0, 1, 0, 0, 0, 0, 1},
        {0, 0, 1, 0, 0, 0, 1},
        {0, 0, 0, 1, 0, 0, 1},
        {0, 0, 0, 0, 1, 1, 1},
        {0, 0, 0, 0, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0},
    };

    const auto u = static_cast<uint8_t>(m_currentState);
    const auto v = static_cast<uint8_t>(newState);

    if (!kValid[u][v]) {
        LOG_WARN(kTag, "Invalid state transition %d -> %d",
            static_cast<int>(m_currentState), static_cast<int>(newState));
        return;
    }

    LOG_DEBUG(kTag, "State %d -> %d", static_cast<int>(m_currentState), static_cast<int>(newState));
    m_currentState = newState;
}

void GeminiClient::setError(GeminiError error) noexcept {
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    m_response.error = error;

    if (error != GeminiError::CANCELLED) {
        LOG_ERROR(kTag, "Error %d", static_cast<int>(error));
        if (m_currentState != GeminiState::IDLE) {
            recordFailure();
        }
    }
}

void GeminiClient::resetRequest() noexcept {
    m_response.clear();
    m_lastError = GeminiError::NONE;
    m_requestBuffer.clear();
    m_responseBuffer.clear();
    m_streamingBuffer.clear();
    m_headersParsed = false;
    m_contentLength = -1;
    m_headerLine.clear();
    m_chunkedEncoding = false;
    m_chunkRemaining = 0;
    m_chunkLine.clear();
    m_bodyComplete = false;
    m_httpStatusCode = 0;
    m_streamComplete = false;
    m_responseTruncated = false;
}

// ============================================================================
// Circuit breaker
// ============================================================================

void GeminiClient::recordSuccess() noexcept {
    if (m_circuitState != CircuitBreakerState::CLOSED) {
        LOG_INFO(kTag, "Circuit closed (was %s)",
            m_circuitState == CircuitBreakerState::HALF_OPEN ? "half-open" : "open");
    }
    m_circuitState = CircuitBreakerState::CLOSED;
    m_circuitFailures = 0;

    m_telemetry.successfulRequests++;
    m_telemetry.lastLatencyMs = m_response.latencyMs;
    if (m_response.latencyMs < m_telemetry.fastestLatencyMs) {
        m_telemetry.fastestLatencyMs = m_response.latencyMs;
    }
    if (m_response.latencyMs > m_telemetry.slowestLatencyMs) {
        m_telemetry.slowestLatencyMs = m_response.latencyMs;
    }
    if (m_telemetry.successfulRequests > 0) {
        m_telemetry.avgLatencyMs = (
            (m_telemetry.avgLatencyMs * (m_telemetry.successfulRequests - 1)) +
            m_response.latencyMs
        ) / m_telemetry.successfulRequests;
    }
    m_telemetry.inputTokens += m_response.promptTokens;
    m_telemetry.outputTokens += m_response.responseTokens;
    m_telemetry.totalTokens += m_response.totalTokens;
}

void GeminiClient::recordFailure() noexcept {
    if (m_currentState == GeminiState::IDLE) {
        return;
    }

    ++m_circuitFailures;
    m_telemetry.failedRequests++;

    if (m_circuitFailures >= kCircuitThreshold && m_circuitState != CircuitBreakerState::OPEN) {
        m_circuitState = CircuitBreakerState::OPEN;
        m_circuitOpenTime = millis();
        LOG_WARN(kTag, "Circuit opened (%d consecutive failures)", m_circuitFailures);
    }
}

// ============================================================================
// Networking
// ============================================================================

bool GeminiClient::connectToApi() noexcept {
    if (m_client.connected()) {
        return true;
    }

    String host = m_endpoint;
    int port = 443;

    const int protoPos = host.indexOf("://");
    if (protoPos >= 0) {
        host = host.substring(protoPos + 3);
    }

    const int pathPos = host.indexOf('/');
    if (pathPos >= 0) {
        host = host.substring(0, pathPos);
    }

    const int colonPos = host.indexOf(':');
    if (colonPos >= 0) {
        port = host.substring(colonPos + 1).toInt();
        host = host.substring(0, colonPos);
    }

    m_client.setTimeout(kConnectionTimeoutMs / 1000UL);

    const unsigned long t0 = millis();
    if (!m_client.connect(host.c_str(), port)) {
        LOG_ERROR(kTag, "TLS connect failed to %s:%d", host.c_str(), port);
        setError(GeminiError::TLS_ERROR);
        return false;
    }
    m_lastActivityTime = millis();

    LOG_INFO(kTag, "Connected (%lu ms)", m_lastActivityTime - t0);
    return true;
}

void GeminiClient::disconnectFromApi() noexcept {
    if (m_client.connected()) {
        m_client.stop();
        LOG_DEBUG(kTag, "Disconnected");
    }
}

String GeminiClient::buildRequest() noexcept {
    m_requestBuffer.clear();

    JsonDocument doc;

    if (!m_systemPrompt.isEmpty()) {
        doc["systemInstruction"]["parts"][0]["text"] = m_systemPrompt;
    }

    JsonArray contents = doc["contents"].to<JsonArray>();
    for (const auto& turn : m_conversation) {
        if (turn.text.isEmpty()) {
            continue;
        }
        JsonObject content = contents.add<JsonObject>();
        content["role"] = turn.role;
        content["parts"][0]["text"] = turn.text;
    }

    JsonObject gc = doc["generationConfig"].to<JsonObject>();
    gc["temperature"] = m_temperature;
    gc["topP"] = m_topP;
    gc["topK"] = m_topK;
    gc["maxOutputTokens"] = m_maxTokens;

    if (!m_safetySettings.isEmpty()) {
        JsonArray safety = doc["safetySettings"].to<JsonArray>();
        int pos = 0;
        while (pos < static_cast<int>(m_safetySettings.length())) {
            const int end = m_safetySettings.indexOf(',', pos);
            const int span = (end < 0) ? m_safetySettings.length() : end;
            JsonDocument entry;
            if (deserializeJson(entry, m_safetySettings.substring(pos, span)) == DeserializationError::Ok) {
                safety.add(entry.as<JsonObject>());
            }
            pos = (end < 0) ? m_safetySettings.length() : end + 1;
        }
    }

    serializeJson(doc, m_requestBuffer);
    return m_requestBuffer;
}

bool GeminiClient::sendRequest(const String& body) noexcept {
    if (!m_client.connected()) {
        return false;
    }

    String host = m_endpoint;
    {
        const int ps = host.indexOf("://");
        if (ps >= 0) host = host.substring(ps + 3);
    }
    String path = "/";
    {
        const int ps = host.indexOf('/');
        if (ps >= 0) {
            path = host.substring(ps);
            host = host.substring(0, ps);
        }
    }
    {
        const int ps = host.indexOf(':');
        if (ps >= 0) host = host.substring(0, ps);
    }

    String request;
    request.reserve(body.length() + 256U);
    request += "POST ";
    request += path;
    request += "?key=";
    request += m_apiKey;
    request += " HTTP/1.1\r\nHost: ";
    request += host;
    request += "\r\nContent-Type: application/json\r\nContent-Length: ";
    request += String(body.length());
    request += "\r\nConnection: close\r\n\r\n";
    request += body;

    const size_t written = m_client.write(
        reinterpret_cast<const uint8_t*>(request.c_str()), request.length());

    if (written != request.length()) {
        LOG_ERROR(kTag, "Short write (%u of %u bytes)", written, request.length());
        return false;
    }

    m_client.flush();
    m_lastActivityTime = millis();
    return true;
}

// ============================================================================
// HTTP response parsing
// ============================================================================

int GeminiClient::parseStatusCode(const String& line) const noexcept {
    int code = 0;
    const int sp1 = line.indexOf(' ');
    if (sp1 < 0) {
        return 0;
    }
    const int sp2 = line.indexOf(' ', sp1 + 1);
    const int end = (sp2 < 0) ? line.length() : sp2;
    for (int i = sp1 + 1; i < end; ++i) {
        code = code * 10 + charToInt(line.charAt(i));
    }
    return code;
}

bool GeminiClient::parseHttpHeaders() noexcept {
    if (m_headersParsed) {
        return true;
    }

    if (isConnectionTimedOut()) {
        LOG_ERROR(kTag, "Header parse timeout");
        return false;
    }

    while (m_client.available()) {
        esp_task_wdt_reset();
        char c = static_cast<char>(m_client.read());
        if (c < 0 || c == 0) break;
        if (c == '\n') {
            m_headerLine.trim();
            if (m_headerLine.isEmpty() || m_headerLine == "\r") {
                m_headersParsed = true;
                m_headerLine.clear();
                return true;
            }
            if (m_headerLine.startsWith("HTTP/")) {
                m_httpStatusCode = parseStatusCode(m_headerLine);
                LOG_DEBUG(kTag, "HTTP %d", m_httpStatusCode);
            } else if (m_headerLine.startsWith("Content-Length:")) {
                m_contentLength = m_headerLine.substring(15).toInt();
            } else if (m_headerLine.startsWith("Transfer-Encoding:") && m_headerLine.indexOf("chunked") >= 0) {
                m_chunkedEncoding = true;
            }
            m_headerLine.clear();
        } else {
            m_headerLine += c;
        }
    }

    if (!m_client.connected() && !m_client.available() && !m_headerLine.isEmpty()) {
        m_headerLine.clear();
        return false;
    }

    return false;
}

void GeminiClient::readAvailableData() noexcept {
    String& dest = m_streaming ? m_streamingBuffer : m_responseBuffer;

    // Ensure the destination buffer has its capacity reserved once so the
    // byte-by-byte appends below do not cause repeated heap reallocations.
    if (dest.length() == 0) {
        dest.reserve(kBufferReserveSize);
    }

    // Hard cap on body buffer to prevent unbounded growth under attack or
    // malformed responses. Excess data is discarded; response will be marked
    // as truncated so the caller can detect incomplete data.
    const bool bufferFull = dest.length() >= kMaxBodyBufferBytes;

    if (m_chunkedEncoding) {
        while (m_client.available()) {
            esp_task_wdt_reset();

            if (m_chunkRemaining > 0) {
                // Resume mid-chunk: consume the remaining bytes of the current
                // chunk. State is kept across ticks so a socket that runs dry
                // mid-chunk never corrupts the stream on the next call.
                while (m_chunkRemaining > 0 && m_client.available()) {
                    esp_task_wdt_reset();
                    const int c = m_client.read();
                    if (c < 0) break;
                    if (!bufferFull) {
                        dest += static_cast<char>(c);
                    }
                    --m_chunkRemaining;
                }
                if (m_chunkRemaining > 0) {
                    break; // need more data; resume on the next tick
                }
                continue; // chunk consumed; read the next size line below
            }

            // Read the chunk-size line, accumulating across ticks so a partial
            // line (socket ran dry) is never misinterpreted as a size value.
            // Bound the accumulator so a malformed/hostile stream that never
            // sends a newline cannot grow it without limit.
            while (m_client.available() && m_chunkLine.indexOf('\n') < 0) {
                esp_task_wdt_reset();
                const int c = m_client.read();
                if (c < 0) break;
                if (m_chunkLine.length() >= 64) {
                    m_bodyComplete = true; // malformed chunk framing; stop here
                    return;
                }
                m_chunkLine += static_cast<char>(c);
            }
            const int nlPos = m_chunkLine.indexOf('\n');
            if (nlPos < 0) {
                break; // line incomplete; wait for more data next tick
            }
            String line = m_chunkLine.substring(0, nlPos);
            m_chunkLine = m_chunkLine.substring(nlPos + 1);
            line.trim();
            if (line.isEmpty()) {
                continue;
            }

            const unsigned long chunkSize = strtoul(line.c_str(), nullptr, 16);
            if (chunkSize == 0) {
                m_bodyComplete = true;
                return;
            }
            m_chunkRemaining = chunkSize;
        }
    } else {
        while (m_client.available()) {
            esp_task_wdt_reset();
            const int c = m_client.read();
            if (c >= 0 && !bufferFull) {
                dest += static_cast<char>(c);
            }
        }
    }

    m_lastActivityTime = millis();
}

bool GeminiClient::isBodyComplete() noexcept {
    if (m_bodyComplete) {
        return true;
    }
    if (m_contentLength > 0) {
        return static_cast<int>(m_responseBuffer.length()) >= m_contentLength;
    }
    return !m_client.connected() && !m_client.available();
}

// ============================================================================
// HTTP error handling
// ============================================================================

bool GeminiClient::handleHttpResponse(int statusCode) noexcept {
    switch (statusCode) {
        case 200:
            return true;

        case 400:
            LOG_ERROR(kTag, "HTTP 400 Bad Request");
            setError(GeminiError::API_ERROR);
            return false;

        case 401:
        case 403:
            LOG_ERROR(kTag, "HTTP %d Authentication failed", statusCode);
            setError(GeminiError::AUTHENTICATION);
            return false;

        case 429:
            LOG_WARN(kTag, "HTTP 429 Rate limited");
            setError(GeminiError::RATE_LIMIT);
            return shouldRetry();

        case 500:
        case 502:
            LOG_WARN(kTag, "HTTP %d Server error", statusCode);
            setError(GeminiError::API_ERROR);
            return shouldRetry();

        case 503:
            LOG_WARN(kTag, "HTTP 503 Service unavailable");
            setError(GeminiError::API_ERROR);
            return shouldRetry();

        default:
            LOG_ERROR(kTag, "HTTP %d Unexpected", statusCode);
            setError(GeminiError::API_ERROR);
            return false;
    }
}

// ============================================================================
// Response parsing (non-streaming)
// ============================================================================

bool GeminiClient::parseResponse(const String& response) noexcept {
    if (response.isEmpty()) {
        LOG_ERROR(kTag, "Empty response body");
        setError(GeminiError::API_ERROR);
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, response);
    if (err) {
        LOG_ERROR(kTag, "JSON parse error: %s", err.c_str());
        setError(GeminiError::JSON_ERROR);
        return false;
    }

    if (doc.containsKey("error")) {
        const int code = doc["error"]["code"] | 0;
        const String msg = doc["error"]["message"] | "Unknown API error";
        LOG_ERROR(kTag, "API error %d: %s", code, msg.c_str());
        if (code == 401 || code == 403) {
            setError(GeminiError::AUTHENTICATION);
        } else if (code == 429) {
            setError(GeminiError::RATE_LIMIT);
        } else {
            setError(GeminiError::API_ERROR);
        }
        return false;
    }

    if (!doc.containsKey("candidates") || doc["candidates"].size() == 0) {
        LOG_WARN(kTag, "No candidates in response");
        setError(GeminiError::API_ERROR);
        return false;
    }

    const JsonObject candidate = doc["candidates"][0];

    if (candidate.containsKey("finishReason")) {
        m_response.finishReason = candidate["finishReason"] | "STOP";

        if (m_response.finishReason == "SAFETY" ||
            m_response.finishReason == "RECITATION" ||
            m_response.finishReason == "PROHIBITED_CONTENT") {
            LOG_ERROR(kTag, "Blocked by content filter: %s", m_response.finishReason.c_str());
            setError(GeminiError::SAFETY);
            return false;
        }
    }

    if (!candidate.containsKey("content") || !candidate["content"].containsKey("parts") ||
        candidate["content"]["parts"].size() == 0) {
        LOG_WARN(kTag, "Candidate has no content parts");
        m_response.responseText = "";
    } else {
        m_response.responseText = candidate["content"]["parts"][0]["text"] | "";
        if (m_response.responseText.length() > kMaxResponseTextChars) {
            m_response.responseText = m_response.responseText.substring(0, kMaxResponseTextChars);
            m_responseTruncated = true;
            m_response.finishReason = "MAX_TOKENS";
        }
    }

    if (doc.containsKey("usageMetadata")) {
        const JsonObject usage = doc["usageMetadata"];
        m_response.promptTokens = usage["promptTokenCount"] | 0;
        m_response.responseTokens = usage["candidatesTokenCount"] | 0;
        m_response.totalTokens = usage["totalTokenCount"] | 0;
    }

    m_response.latencyMs = millis() - m_requestStartTime;
    m_response.timestamp = millis();
    m_response.error = GeminiError::NONE;

    if (!m_response.responseText.isEmpty()) {
        addConversationTurn("model", m_response.responseText);

        if (m_cacheEnabled && !m_lastPrompt.isEmpty()) {
            storeCache(m_lastPrompt, m_response.responseText);
            m_lastPrompt.clear();
        }
    }

    LOG_INFO(kTag, "Response: %d tokens, %lu ms",
        m_response.totalTokens, m_response.latencyMs);
    return true;
}

// ============================================================================
// Streaming response parsing
// ============================================================================

void GeminiClient::processSseEvents() noexcept {
    // Reserve the response buffer once so SSE deltas accumulate into a fixed
    // buffer instead of repeated exact-fit reallocations as the reply streams.
    if (m_response.responseText.isEmpty()) {
        m_response.responseText.reserve(kMaxResponseTextChars);
    }

    int pos = 0;
    const int len = static_cast<int>(m_streamingBuffer.length());

    while (pos < len) {
        int dataStart = m_streamingBuffer.indexOf("data: ", pos);
        if (dataStart < 0) {
            break;
        }

        int lineEnd = m_streamingBuffer.indexOf('\n', dataStart);
        if (lineEnd < 0) {
            break;
        }

        String payload = m_streamingBuffer.substring(dataStart + 6, lineEnd);
        payload.trim();

        if (payload == "[DONE]") {
            m_streamComplete = true;
            int nextEvent = m_streamingBuffer.indexOf('\n', lineEnd + 1);
            if (nextEvent > lineEnd) {
                m_streamingBuffer.remove(0, nextEvent + 1);
            } else {
                m_streamingBuffer.clear();
            }
            LOG_INFO(kTag, "Stream [DONE] received");
            return;
        }

        if (!payload.isEmpty()) {
            parseStreamingChunk(payload);
        }

        pos = lineEnd + 1;
        while (pos < len && m_streamingBuffer.charAt(pos) == '\n') {
            ++pos;
        }
    }

    if (pos > 0) {
        m_streamingBuffer.remove(0, pos);
    }
}

bool GeminiClient::parseStreamingChunk(const String& chunk) noexcept {
    if (chunk.isEmpty()) {
        return false;
    }

    // Reuse the member document: clear() keeps the pool capacity from the
    // previous chunk, avoiding a fresh JsonDocument allocation per SSE chunk.
    JsonDocument& doc = m_streamParseDoc;
    doc.clear();
    const DeserializationError err = deserializeJson(doc, chunk);
    if (err) {
        LOG_DEBUG(kTag, "Streaming chunk parse error (expected for partial data)");
        return false;
    }

    if (!doc.containsKey("candidates") || doc["candidates"].size() == 0) {
        return false;
    }

    const JsonObject candidate = doc["candidates"][0];

    if (candidate.containsKey("finishReason")) {
        const String reason = candidate["finishReason"] | "";
        if (!reason.isEmpty() && reason != "STOP") {
            m_response.finishReason = reason;
            if (reason == "SAFETY" || reason == "RECITATION" || reason == "PROHIBITED_CONTENT") {
                setError(GeminiError::SAFETY);
            }
            return true;
        }
    }

    if (candidate.containsKey("content") &&
        candidate["content"].containsKey("parts") &&
        candidate["content"]["parts"].size() > 0) {
        const String delta = candidate["content"]["parts"][0]["text"] | "";
        if (!delta.isEmpty()) {
            const size_t newLen = m_response.responseText.length() + delta.length();
            if (newLen <= kMaxResponseTextChars) {
                m_response.responseText += delta;
            } else {
                // Truncate to fit the hard cap.
                if (m_response.responseText.length() < kMaxResponseTextChars) {
                    m_response.responseText += delta.substring(0, kMaxResponseTextChars - m_response.responseText.length());
                }
                m_responseTruncated = true;
                m_response.finishReason = "MAX_TOKENS";
            }
        }
    }

    if (doc.containsKey("usageMetadata")) {
        const JsonObject usage = doc["usageMetadata"];
        m_response.promptTokens = usage["promptTokenCount"] | m_response.promptTokens;
        m_response.responseTokens = usage["candidatesTokenCount"] | m_response.responseTokens;
        m_response.totalTokens = usage["totalTokenCount"] | m_response.totalTokens;
    }

    return true;
}

// ============================================================================
// Timeout handling
// ============================================================================

void GeminiClient::checkTimeouts() noexcept {
    if (m_currentState == GeminiState::CONNECTING && isConnectionTimedOut()) {
        if (shouldRetry()) {
            applyBackoff();
            disconnectFromApi();
            LOG_WARN(kTag, "Connection timeout, retry %d/%d",
                static_cast<int>(m_retryCount), static_cast<int>(kMaxRetries));
        } else {
            LOG_ERROR(kTag, "Connection timeout");
            disconnectFromApi();
            setError(GeminiError::TIMEOUT);
            changeState(GeminiState::ERROR);
        }
        return;
    }

    if ((m_currentState == GeminiState::WAITING || m_currentState == GeminiState::RECEIVING) &&
        isRequestTimedOut()) {
        if (shouldRetry()) {
            applyBackoff();
            disconnectFromApi();
            changeState(GeminiState::CONNECTING);
            LOG_WARN(kTag, "Request timeout, retry %d/%d",
                static_cast<int>(m_retryCount), static_cast<int>(kMaxRetries));
        } else {
            LOG_ERROR(kTag, "Request timeout");
            disconnectFromApi();
            setError(GeminiError::TIMEOUT);
            changeState(GeminiState::ERROR);
        }
    }
}

bool GeminiClient::isConnectionTimedOut() const noexcept {
    return (millis() - m_connectionStartTime) >= kConnectionTimeoutMs;
}

bool GeminiClient::isRequestTimedOut() const noexcept {
    if (m_timeoutMs == 0) {
        return false;
    }
    return (millis() - m_requestStartTime) >= m_timeoutMs;
}

// ============================================================================
// Retry / backoff
// ============================================================================

bool GeminiClient::shouldRetry() const noexcept {
    return m_retryCount < kMaxRetries;
}

void GeminiClient::applyBackoff() noexcept {
    ++m_retryCount;
    m_retryStartTime = millis();
    m_telemetry.retryCount++;
}

unsigned long GeminiClient::currentBackoffDelay() const noexcept {
    if (m_retryCount == 0) {
        return 0;
    }
    unsigned long delay = kRetryBaseDelayMs * (1UL << (m_retryCount - 1));
    return (delay < kRetryMaxDelayMs) ? delay : kRetryMaxDelayMs;
}

bool GeminiClient::isBackoffComplete() const noexcept {
    if (m_retryStartTime == 0) {
        return true;
    }
    return (millis() - m_retryStartTime) >= currentBackoffDelay();
}

// ============================================================================
// Token estimation
// ============================================================================

int GeminiClient::estimateTokens(const String& text) const noexcept {
    if (text.isEmpty()) {
        return 0;
    }
    return (text.length() / 4) + 1;
}

// ============================================================================
// Response cache
// ============================================================================

uint32_t GeminiClient::hashString(const String& str) const noexcept {
    uint32_t hash = 5381;
    for (size_t i = 0; i < str.length(); ++i) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(str.charAt(i));
    }
    return hash;
}

String GeminiClient::buildCacheKey(const String& prompt) const noexcept {
    String key;
    key.reserve(prompt.length() + 64);
    key += prompt;
    key += '|';
    key += String(m_temperature, 2);
    key += '|';
    key += String(m_topP, 2);
    key += '|';
    key += String(m_topK);
    key += '|';
    key += String(m_maxTokens);
    key += '|';
    key += String(m_systemPromptVersion);
    return key;
}

bool GeminiClient::lookupCache(const String& key, String& value) const noexcept {
    if (m_cacheCount == 0 || key.isEmpty()) {
        return false;
    }

    const uint32_t h = hashString(key);

    for (uint8_t i = 0; i < m_cacheCount; ++i) {
        if (m_cacheHashes[i] == h) {
            value = m_cacheResponses[i];
            return true;
        }
    }

    return false;
}

void GeminiClient::storeCache(const String& key, const String& value) noexcept {
    if (key.isEmpty() || value.isEmpty()) {
        return;
    }

    const uint32_t h = hashString(key);
    const unsigned long now = millis();

    if (m_cacheCount < kCacheCapacity) {
        m_cacheHashes[m_cacheCount] = h;
        m_cacheResponses[m_cacheCount] = value;
        m_cacheTimestamps[m_cacheCount] = now;
        ++m_cacheCount;
        LOG_DEBUG(kTag, "Cache stored (%d/%d)", m_cacheCount, kCacheCapacity);
        return;
    }

    unsigned long oldest = m_cacheTimestamps[0];
    uint8_t oldestIdx = 0;
    for (uint8_t i = 1; i < kCacheCapacity; ++i) {
        if (m_cacheTimestamps[i] < oldest) {
            oldest = m_cacheTimestamps[i];
            oldestIdx = i;
        }
    }

    m_cacheHashes[oldestIdx] = h;
    m_cacheResponses[oldestIdx] = value;
    m_cacheTimestamps[oldestIdx] = now;
    LOG_DEBUG(kTag, "Cache evicted entry %d", oldestIdx);
}
