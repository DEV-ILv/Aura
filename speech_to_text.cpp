#include "speech_to_text.h"

SpeechToText speechToText;

SpeechToText::SpeechToText() noexcept
    : m_initialized(false)
    , m_currentState(SpeechState::IDLE)
    , m_lastError(SpeechError::NONE)
    , m_apiState(ApiState::DISCONNECTED)
    , m_mode(RecognitionMode::ONESHOT)
    , m_result()
    , m_audioBuffer(nullptr)
    , m_bufferCapacity(kDefaultBufferSize)
    , m_bufferLength(0)
    , m_bufferOverflow(false)
    , m_sampleRate(kDefaultSampleRate)
    , m_timeoutMs(kDefaultTimeoutMs)
    , m_partialResultsEnabled(true)
    , m_profanityFilter(false)
    , m_maxAlternatives(1)
    , m_sessionStartTime(0)
    , m_lastAudioTime(0)
    , m_requestStartTime(0) {
    strncpy(m_language, kDefaultLanguage, sizeof(m_language) - 1);
    m_language[sizeof(m_language) - 1] = '\0';
    m_audioFormat = AudioFormat::PCM16_MONO;
    m_apiEndpoint[0] = '\0';
    strncpy(m_apiKey, Secrets::GOOGLE_STT_API_KEY, sizeof(m_apiKey) - 1);
    m_apiKey[sizeof(m_apiKey) - 1] = '\0';
    m_rootCA[0] = '\0';
}

SpeechToText::~SpeechToText() noexcept {
    if (m_audioBuffer) {
        free(m_audioBuffer);
        m_audioBuffer = nullptr;
    }
    disconnectApi();
}

bool SpeechToText::initialize() noexcept {
    if (m_initialized) {
        return true;
    }

    if (!wifiManager.isConnected()) {
        LOG_ERROR("SpeechToText", "WiFi not connected");
        setError(SpeechError::NETWORK);
        return false;
    }

    m_audioBuffer = static_cast<int16_t*>(malloc(m_bufferCapacity * sizeof(int16_t)));
    if (!m_audioBuffer) {
        LOG_ERROR("SpeechToText", "Failed to allocate audio buffer");
        setError(SpeechError::BUFFER_OVERFLOW);
        return false;
    }

    if (m_rootCA[0] != '\0') {
        m_client.setCACert(m_rootCA);
    } else {
        LOG_ERROR("SpeechToText", "No root CA — TLS required");
        setError(SpeechError::TLS_ERROR);
        return false;
    }

    m_client.setTimeout(kConnectionTimeoutMs);

    m_initialized = true;
    m_lastError = SpeechError::NONE;
    LOG_INFO("SpeechToText", "Initialized");
    return true;
}

void SpeechToText::run() noexcept {
    if (!m_initialized) return;

    checkTimeouts();

    switch (m_currentState) {
        case SpeechState::LISTENING:
            if (isRecognitionTimeout()) {
                LOG_WARN("SpeechToText", "Recognition timeout");
                stopRecognition();
            }
            break;

        case SpeechState::PROCESSING:
            if (isRequestTimeout()) {
                LOG_WARN("SpeechToText", "Request timeout");
                disconnectApi();
                setError(SpeechError::TIMEOUT);
                changeState(SpeechState::ERROR);
            } else if (m_apiState == ApiState::RECEIVING) {
                String response = readResponse();
                if (!response.isEmpty()) {
                    if (m_mode == RecognitionMode::CONTINUOUS) {
                        parseStreamingResponse(response);
                    } else {
                        if (parseResponse(response)) {
                            changeState(SpeechState::COMPLETED);
                        } else {
                            setError(SpeechError::API_ERROR);
                            changeState(SpeechState::ERROR);
                        }
                    }
                }
            }
            break;

        case SpeechState::COMPLETED:
            if (m_mode == RecognitionMode::ONESHOT) {
                changeState(SpeechState::IDLE);
            }
            break;

        default:
            break;
    }
}

void SpeechToText::update() noexcept {
    run();
}

bool SpeechToText::startRecognition(RecognitionMode mode) noexcept {
    if (!m_initialized) {
        setError(SpeechError::UNKNOWN);
        return false;
    }
    if (m_currentState != SpeechState::IDLE) {
        LOG_WARN("SpeechToText", "Already in state %d", static_cast<int>(m_currentState));
        return false;
    }
    if (!wifiManager.isConnected()) {
        LOG_ERROR("SpeechToText", "WiFi disconnected");
        setError(SpeechError::NETWORK);
        return false;
    }
    if (m_apiEndpoint[0] == '\0') {
        LOG_ERROR("SpeechToText", "API endpoint not set");
        setError(SpeechError::API_ERROR);
        return false;
    }

    m_mode = mode;
    resetSession();
    resetBuffer();
    changeState(SpeechState::LISTENING);
    m_sessionStartTime = millis();
    m_lastAudioTime = millis();

    LOG_INFO("SpeechToText", "Recognition started (mode=%d)", static_cast<int>(mode));
    return true;
}

bool SpeechToText::stopRecognition() noexcept {
    if (m_currentState != SpeechState::LISTENING) {
        return false;
    }

    if (m_bufferLength == 0) {
        LOG_WARN("SpeechToText", "No audio recorded");
        changeState(SpeechState::IDLE);
        return false;
    }

    changeState(SpeechState::PROCESSING);
    m_apiState = ApiState::CONNECTING;
    m_requestStartTime = millis();

    if (!connectToApi()) {
        setError(SpeechError::NETWORK);
        changeState(SpeechState::ERROR);
        return false;
    }

    if (!sendRequest(buildRequestBody())) {
        disconnectApi();
        setError(SpeechError::NETWORK);
        changeState(SpeechState::ERROR);
        return false;
    }

    m_apiState = ApiState::RECEIVING;
    LOG_INFO("SpeechToText", "Audio sent, waiting for response");
    return true;
}

void SpeechToText::cancelRecognition() noexcept {
    if (m_currentState == SpeechState::IDLE) return;

    disconnectApi();
    resetBuffer();
    m_result.clear();
    changeState(SpeechState::IDLE);
    LOG_INFO("SpeechToText", "Recognition cancelled");
}

void SpeechToText::processAudio(const int16_t* audioData, size_t length) noexcept {
    if (m_currentState != SpeechState::LISTENING) return;
    if (!audioData || length == 0) return;

    appendAudio(audioData, length);
    m_lastAudioTime = millis();
}

void SpeechToText::processAudioChunk(const AudioChunk& chunk) noexcept {
    if (m_currentState != SpeechState::LISTENING) return;
    if (!chunk.data || chunk.length == 0) return;

    appendAudio(chunk.data, chunk.length);
    m_lastAudioTime = chunk.timestamp;
}

bool SpeechToText::sendAudio() noexcept {
    return stopRecognition();
}

bool SpeechToText::flushBuffer() noexcept {
    if (m_currentState != SpeechState::LISTENING) return false;
    if (m_bufferLength == 0) return false;

    if (m_mode != RecognitionMode::CONTINUOUS) return false;

    if (!connectToApi()) return false;
    if (!sendRequest(buildRequestBody())) {
        disconnectApi();
        return false;
    }
    m_apiState = ApiState::RECEIVING;
    m_requestStartTime = millis();
    return true;
}

void SpeechToText::clearResult() noexcept {
    m_result.clear();
    m_lastError = SpeechError::NONE;
}

void SpeechToText::setLanguage(const String& language) noexcept {
    strncpy(m_language, language.c_str(), sizeof(m_language) - 1);
    m_language[sizeof(m_language) - 1] = '\0';
}

void SpeechToText::setTimeout(unsigned long timeoutMs) noexcept {
    m_timeoutMs = timeoutMs;
}

void SpeechToText::setSampleRate(uint32_t sampleRate) noexcept {
    m_sampleRate = sampleRate;
}

void SpeechToText::setAudioFormat(AudioFormat format) noexcept {
    m_audioFormat = format;
}

void SpeechToText::setMode(RecognitionMode mode) noexcept {
    m_mode = mode;
}

void SpeechToText::setApiEndpoint(const String& endpoint) noexcept {
    strncpy(m_apiEndpoint, endpoint.c_str(), sizeof(m_apiEndpoint) - 1);
    m_apiEndpoint[sizeof(m_apiEndpoint) - 1] = '\0';
}

void SpeechToText::setApiKey(const String& apiKey) noexcept {
    strncpy(m_apiKey, apiKey.c_str(), sizeof(m_apiKey) - 1);
    m_apiKey[sizeof(m_apiKey) - 1] = '\0';
}

void SpeechToText::setRootCA(const String& caCert) noexcept {
    strncpy(m_rootCA, caCert.c_str(), sizeof(m_rootCA) - 1);
    m_rootCA[sizeof(m_rootCA) - 1] = '\0';
}

void SpeechToText::setPartialResults(bool enable) noexcept {
    m_partialResultsEnabled = enable;
}

void SpeechToText::setProfanityFilter(bool enable) noexcept {
    m_profanityFilter = enable;
}

void SpeechToText::setMaxAlternatives(uint8_t max) noexcept {
    m_maxAlternatives = (max > 10) ? 10 : (max < 1 ? 1 : max);
}

const SpeechResult& SpeechToText::getResult() const noexcept {
    return m_result;
}

SpeechState SpeechToText::getState() const noexcept {
    return m_currentState;
}

SpeechError SpeechToText::getError() const noexcept {
    return m_lastError;
}

ApiState SpeechToText::getApiState() const noexcept {
    return m_apiState;
}

bool SpeechToText::isRecognizing() const noexcept {
    return m_currentState == SpeechState::LISTENING;
}

bool SpeechToText::isBusy() const noexcept {
    return m_currentState == SpeechState::LISTENING || m_currentState == SpeechState::PROCESSING;
}

bool SpeechToText::isInitialized() const noexcept {
    return m_initialized;
}

bool SpeechToText::isBufferNearFull() const noexcept {
    return m_bufferLength > (m_bufferCapacity * 9 / 10);
}

void SpeechToText::getBufferStats(size_t& usedBytes, size_t& capacityBytes) const noexcept {
    usedBytes = m_bufferLength * sizeof(int16_t);
    capacityBytes = m_bufferCapacity * sizeof(int16_t);
}

void SpeechToText::changeState(SpeechState newState) noexcept {
    if (m_currentState == newState) return;
    LOG_DEBUG("SpeechToText", "State %d -> %d", static_cast<int>(m_currentState), static_cast<int>(newState));
    m_currentState = newState;
}

void SpeechToText::setError(SpeechError error) noexcept {
    if (m_lastError == error) return;
    m_lastError = error;
    m_result.error = error;
    LOG_ERROR("SpeechToText", "Error %d", static_cast<int>(error));
}

void SpeechToText::resetSession() noexcept {
    m_result.clear();
    m_lastError = SpeechError::NONE;
    m_apiState = ApiState::DISCONNECTED;
    m_sessionStartTime = 0;
    m_lastAudioTime = 0;
    m_requestStartTime = 0;
}

bool SpeechToText::ensureCapacity(size_t additionalBytes) noexcept {
    size_t requiredBytes = m_bufferLength * sizeof(int16_t) + additionalBytes;
    size_t currentCapacityBytes = m_bufferCapacity * sizeof(int16_t);

    if (requiredBytes <= currentCapacityBytes) return true;

    size_t newCapacity = m_bufferCapacity;
    while (newCapacity * sizeof(int16_t) < requiredBytes && newCapacity < kMaxBufferSize) {
        newCapacity *= 2;
    }
    if (newCapacity > kMaxBufferSize) newCapacity = kMaxBufferSize;
    if (requiredBytes > newCapacity * sizeof(int16_t)) return false;

    int16_t* newBuffer = static_cast<int16_t*>(realloc(m_audioBuffer, newCapacity * sizeof(int16_t)));
    if (!newBuffer) return false;

    m_audioBuffer = newBuffer;
    m_bufferCapacity = newCapacity;
    return true;
}

void SpeechToText::appendAudio(const int16_t* data, size_t samples) noexcept {
    size_t additionalBytes = samples * sizeof(int16_t);
    if (!ensureCapacity(additionalBytes)) {
        if (!m_bufferOverflow) {
            m_bufferOverflow = true;
            LOG_ERROR("SpeechToText", "Audio buffer overflow");
            setError(SpeechError::BUFFER_OVERFLOW);
        }
        return;
    }

    memcpy(m_audioBuffer + m_bufferLength, data, additionalBytes);
    m_bufferLength += samples;
}

void SpeechToText::resetBuffer() noexcept {
    m_bufferLength = 0;
    m_bufferOverflow = false;
}

String SpeechToText::buildRequestBody() noexcept {
    JsonDocument doc;
    doc["config"]["encoding"] = (m_audioFormat == AudioFormat::PCM16_MONO) ? "LINEAR16" : "LINEAR16";
    doc["config"]["sampleRateHertz"] = m_sampleRate;
    doc["config"]["languageCode"] = m_language;
    doc["config"]["enableAutomaticPunctuation"] = true;
    doc["config"]["profanityFilter"] = m_profanityFilter;
    doc["config"]["maxAlternatives"] = m_maxAlternatives;

    size_t audioBytes = m_bufferLength * sizeof(int16_t);
    String base64Audio;
    base64Audio.reserve((audioBytes + 2) / 3 * 4);

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(m_audioBuffer);
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (size_t i = 0; i < audioBytes; i += 3) {
        uint32_t triplet = (bytes[i] << 16);
        if (i + 1 < audioBytes) triplet |= (bytes[i + 1] << 8);
        if (i + 2 < audioBytes) triplet |= bytes[i + 2];

        base64Audio += b64[(triplet >> 18) & 0x3F];
        base64Audio += b64[(triplet >> 12) & 0x3F];
        base64Audio += (i + 1 < audioBytes) ? b64[(triplet >> 6) & 0x3F] : '=';
        base64Audio += (i + 2 < audioBytes) ? b64[triplet & 0x3F] : '=';
    }

    doc["audio"]["content"] = base64Audio;

    String output;
    serializeJson(doc, output);
    return output;
}

bool SpeechToText::parseResponse(const String& response) noexcept {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        LOG_ERROR("SpeechToText", "JSON parse error: %s", err.c_str());
        return false;
    }

    if (doc.containsKey("error")) {
        int code = doc["error"]["code"] | 0;
        String message = doc["error"]["message"] | "Unknown API error";
        LOG_ERROR("SpeechToText", "API error %d: %s", code, message.c_str());
        if (code == 401 || code == 403) setError(SpeechError::UNAUTHORIZED);
        else setError(SpeechError::API_ERROR);
        return false;
    }

    if (!doc.containsKey("results") || doc["results"].size() == 0) {
        m_result.transcript = "";
        m_result.isFinal = true;
        m_result.confidence = 0.0f;
        m_result.durationMs = millis() - m_sessionStartTime;
        m_result.timestamp = millis();
        m_result.audioLength = m_bufferLength * sizeof(int16_t);
        return true;
    }

    JsonObject result = doc["results"][0];
    if (result.containsKey("alternatives") && result["alternatives"].size() > 0) {
        JsonObject alt = result["alternatives"][0];
        m_result.transcript = alt["transcript"] | "";
        m_result.confidence = alt["confidence"] | 0.0f;
    }

    m_result.isFinal = result["isFinal"] | true;
    m_result.durationMs = millis() - m_sessionStartTime;
    m_result.timestamp = millis();
    m_result.audioLength = m_bufferLength * sizeof(int16_t);

    return true;
}

bool SpeechToText::parseStreamingResponse(const String& chunk) noexcept {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, chunk);
    if (err) return false;

    if (doc.containsKey("results") && doc["results"].size() > 0) {
        JsonObject result = doc["results"][0];
        if (result.containsKey("alternatives") && result["alternatives"].size() > 0) {
            JsonObject alt = result["alternatives"][0];
            String partial = alt["transcript"] | "";
            bool isFinal = result["isFinal"] | false;
            float conf = alt["confidence"] | 0.0f;

            if (isFinal) {
                m_result.transcript = partial;
                m_result.confidence = conf;
                m_result.isFinal = true;
                m_result.durationMs = millis() - m_sessionStartTime;
                m_result.timestamp = millis();
                m_result.audioLength = m_bufferLength * sizeof(int16_t);
            } else if (m_partialResultsEnabled) {
                m_result.partial = partial;
            }
        }
    }
    return true;
}

bool SpeechToText::connectToApi() noexcept {
    if (m_client.connected()) return true;

    m_apiState = ApiState::CONNECTING;
    String host = m_apiEndpoint;
    int port = 443;

    int protoEnd = host.indexOf("://");
    if (protoEnd >= 0) host = host.substring(protoEnd + 3);

    int pathStart = host.indexOf('/');
    String path = "/";
    if (pathStart >= 0) {
        path = host.substring(pathStart);
        host = host.substring(0, pathStart);
    }

    int colonPos = host.indexOf(':');
    if (colonPos >= 0) {
        port = host.substring(colonPos + 1).toInt();
        host = host.substring(0, colonPos);
    }

    if (!m_client.connect(host.c_str(), port)) {
        LOG_ERROR("SpeechToText", "TLS connect failed to %s:%d", host.c_str(), port);
        m_apiState = ApiState::ERROR;
        return false;
    }

    m_apiState = ApiState::CONNECTED;
    LOG_INFO("SpeechToText", "Connected to API");
    return true;
}

void SpeechToText::disconnectApi() noexcept {
    if (m_client.connected()) {
        m_client.stop();
    }
    m_apiState = ApiState::DISCONNECTED;
}

bool SpeechToText::sendRequest(const String& body) noexcept {
    if (!m_client.connected()) return false;

    m_apiState = ApiState::SENDING;

    String endpoint = m_apiEndpoint;
    String host = endpoint;
    String path = "/";

    int protoEnd = host.indexOf("://");
    if (protoEnd >= 0)
        host = host.substring(protoEnd + 3);

    int pathStart = host.indexOf('/');
    if (pathStart >= 0)
    {
        path = host.substring(pathStart);
        host = host.substring(0, pathStart);
    }

    int colonPos = host.indexOf(':');
    if (colonPos >= 0)
    {
        host = host.substring(0, colonPos);
    }

    String request;
    request.reserve(body.length() + 512);

    request = "POST " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Content-Type: application/json\r\n";
    if (m_apiKey[0] != '\0') {
        request += "Authorization: Bearer " + String(m_apiKey) + "\r\n";
    }
    request += "Content-Length: " + String(body.length()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += body;

    size_t written = m_client.write(reinterpret_cast<const uint8_t*>(request.c_str()), request.length());
    if (written != request.length()) {
        LOG_ERROR("SpeechToText", "Failed to write full request");
        return false;
    }

    m_client.flush();
    return true;
}

String SpeechToText::readResponse() noexcept {
    if (!m_client.connected() && !m_client.available()) {
        return "";
    }

    String response;
    response.reserve(2048);

    unsigned long startRead = millis();
    bool headersDone = false;
    int contentLength = -1;
    bool chunked = false;

    while (m_client.connected() || m_client.available()) {
        if (millis() - startRead > kRequestTimeoutMs) break;

        String line = m_client.readStringUntil('\n');

        if (line == "\r" || line == "") {
            headersDone = true;
            break;
        }

        line.trim();

        if (!headersDone) {
            if (line.startsWith("Content-Length:")) {
                contentLength = line.substring(15).toInt();
            } else if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0) {
                chunked = true;
            }
        }
    }

    if (!headersDone) return "";

    if (chunked) {
        while (m_client.connected() || m_client.available()) {
            String chunkSizeLine = m_client.readStringUntil('\n');
            chunkSizeLine.trim();
            int chunkSize = strtol(chunkSizeLine.c_str(), nullptr, 16);
            if (chunkSize == 0) break;

            String chunkData;
            chunkData.reserve(chunkSize);
            while (chunkData.length() < chunkSize && (m_client.connected() || m_client.available())) {
                char c = m_client.read();
                if (c >= 0) chunkData += c;
            }
            response += chunkData;
            m_client.readStringUntil('\n');
        }
    } else if (contentLength > 0) {
        response.reserve(contentLength);
        while (response.length() < contentLength && (m_client.connected() || m_client.available())) {
            char c = m_client.read();
            if (c >= 0) response += c;
        }
    } else {
        while (m_client.available()) {
            char c = m_client.read();
            if (c >= 0) response += c;
        }
    }

    return response;
}

bool SpeechToText::handleHttpResponse(int statusCode, const String& body) noexcept {
    if (statusCode == 200) return true;
    if (statusCode == 401 || statusCode == 403) setError(SpeechError::UNAUTHORIZED);
    else if (statusCode == 400) setError(SpeechError::INVALID_AUDIO);
    else if (statusCode >= 500) setError(SpeechError::API_ERROR);
    else setError(SpeechError::NETWORK);
    return false;
}

void SpeechToText::checkTimeouts() noexcept {
    if (m_currentState == SpeechState::LISTENING && isRecognitionTimeout()) {
        LOG_WARN("SpeechToText", "Recognition timeout");
        stopRecognition();
    }
    if (m_currentState == SpeechState::PROCESSING && isRequestTimeout()) {
        LOG_WARN("SpeechToText", "Request timeout");
        disconnectApi();
        setError(SpeechError::TIMEOUT);
        changeState(SpeechState::ERROR);
    }
}

bool SpeechToText::isRecognitionTimeout() const noexcept {
    if (m_timeoutMs == 0) return false;
    return (millis() - m_sessionStartTime) >= m_timeoutMs;
}

bool SpeechToText::isRequestTimeout() const noexcept {
    return (millis() - m_requestStartTime) >= kRequestTimeoutMs;
}