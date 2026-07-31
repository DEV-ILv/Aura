#include "text_to_speech.h"

TextToSpeech textToSpeech;

TextToSpeech::TextToSpeech() noexcept
    : m_initialized(false)
    , m_currentState(TTSState::IDLE)
    , m_lastError(TTSError::NONE)
    , m_response()
    , m_speed(kDefaultSpeed)
    , m_pitch(kDefaultPitch)
    , m_volume(kDefaultVolume)
    , m_streaming(false)
    , m_audioBuffer(nullptr)
    , m_audioBufferSize(0)
    , m_audioBufferCapacity(0)
    , m_playbackOffset(0)
    , m_requestStartTime(0)
    , m_connectionStartTime(0)
    , m_playbackStartTime(0)
    , m_headersParsed(false)
    , m_contentLength(-1)
    , m_chunkedEncoding(false)
    , m_paused(false)
    , m_pausedOffset(0) {
    strncpy(m_apiKey, Secrets::GOOGLE_TTS_API_KEY, sizeof(m_apiKey) - 1);
    m_apiKey[sizeof(m_apiKey) - 1] = '\0';
    m_endpoint[0] = '\0';
    strncpy(m_voice, kDefaultVoice, sizeof(m_voice) - 1);
    m_voice[sizeof(m_voice) - 1] = '\0';
    strncpy(m_language, kDefaultLanguage, sizeof(m_language) - 1);
    m_language[sizeof(m_language) - 1] = '\0';
    m_rootCA[0] = '\0';
    m_queue.reserve(kMaxQueueSize);
}

TextToSpeech::~TextToSpeech() noexcept {
    stopPlayback();
    disconnectFromApi();
    if (m_audioBuffer) {
        free(m_audioBuffer);
        m_audioBuffer = nullptr;
    }
}

bool TextToSpeech::initialize() noexcept {
    if (m_initialized) return true;

    if (!wifiManager.isConnected()) {
        LOG_ERROR("TextToSpeech", "WiFi not connected");
        setError(TTSError::NETWORK);
        return false;
    }

    if (m_apiKey[0] == '\0') {
        LOG_ERROR("TextToSpeech", "API key not set (set GOOGLE_TTS_API_KEY in secrets.h)");
        setError(TTSError::AUTHENTICATION);
        return false;
    }

    if (m_endpoint[0] == '\0') {
        LOG_ERROR("TextToSpeech", "API endpoint not set");
        setError(TTSError::API_ERROR);
        return false;
    }

    m_audioBufferCapacity = kMaxAudioBuffer;
    m_audioBuffer = static_cast<uint8_t*>(malloc(m_audioBufferCapacity));
    if (!m_audioBuffer) {
        LOG_ERROR("TextToSpeech", "Failed to allocate audio buffer");
        setError(TTSError::AUDIO_ERROR);
        return false;
    }

    if (m_rootCA[0] != '\0') {
        m_client.setCACert(m_rootCA);
    } else {
        LOG_ERROR("TextToSpeech", "No root CA — TLS required");
        setError(TTSError::TLS_ERROR);
        return false;
    }

    m_client.setTimeout(kConnectionTimeoutMs);

    m_initialized = true;
    m_lastError = TTSError::NONE;
    LOG_INFO("TextToSpeech", "Initialized");
    return true;
}

void TextToSpeech::run() noexcept {
    if (!m_initialized) return;

    checkTimeouts();

    switch (m_currentState) {
        case TTSState::CONNECTING:
            if (connectToApi()) {
                changeState(TTSState::SENDING);
            }
            break;

        case TTSState::SENDING:
            if (sendRequest(buildRequest())) {
                changeState(TTSState::WAITING);
                m_requestStartTime = millis();
            } else {
                disconnectFromApi();
                setError(TTSError::NETWORK);
                changeState(TTSState::ERROR);
            }
            break;

        case TTSState::WAITING:
        case TTSState::RECEIVING:
            if (m_client.connected() || m_client.available()) {
                String response = readResponse();
                if (!response.isEmpty()) {
                    if (parseResponse(response)) {
                        if (decodeAudio(m_responseBuffer)) {
                            changeState(TTSState::DECODING);
                        } else {
                            setError(TTSError::DECODE_ERROR);
                            changeState(TTSState::ERROR);
                        }
                    } else {
                        setError(TTSError::API_ERROR);
                        changeState(TTSState::ERROR);
                    }
                }
            }
            break;

        case TTSState::DECODING:
            if (playAudio()) {
                changeState(TTSState::PLAYING);
                m_playbackStartTime = millis();
            } else {
                setError(TTSError::AUDIO_ERROR);
                changeState(TTSState::ERROR);
            }
            break;

        case TTSState::PLAYING:
            if (!m_paused) {
                if (!playAudio()) {
                    changeState(TTSState::COMPLETED);
                }
            }
            break;

        case TTSState::COMPLETED:
            stopPlayback();
            advanceQueue();
            if (!m_queue.empty()) {
                processQueue();
            } else {
                changeState(TTSState::IDLE);
            }
            break;

        case TTSState::IDLE:
            if (!m_queue.empty()) {
                processQueue();
            }
            break;

        default:
            break;
    }
}

void TextToSpeech::update() noexcept {
    run();
}

bool TextToSpeech::speak(const String& text, bool priority) noexcept {
    if (!m_initialized) {
        setError(TTSError::UNKNOWN);
        return false;
    }
    if (text.isEmpty()) return false;

    TTSQueueItem item(text, m_voice, m_language, m_speed, m_pitch, m_volume, priority);

    if (priority) {
        m_queue.insert(m_queue.begin(), item);
    } else {
        if (m_queue.size() >= kMaxQueueSize) {
            LOG_WARN("TextToSpeech", "Queue full, dropping oldest");
            m_queue.erase(m_queue.begin());
        }
        m_queue.push_back(item);
    }

    LOG_DEBUG("TextToSpeech", "Queued text (%d chars, priority=%d)", text.length(), priority);
    return true;
}

void TextToSpeech::stop() noexcept {
    stopPlayback();
    clearQueue();
    changeState(TTSState::IDLE);
    LOG_INFO("TextToSpeech", "Stopped");
}

void TextToSpeech::pause() noexcept {
    if (m_currentState == TTSState::PLAYING && !m_paused) {
        m_paused = true;
        m_pausedOffset = m_playbackOffset;
        audioManager.stopPlayback();
        LOG_DEBUG("TextToSpeech", "Paused at offset %d", m_pausedOffset);
    }
}

void TextToSpeech::resume() noexcept {
    if (m_paused && m_currentState == TTSState::PLAYING) {
        m_paused = false;
        m_playbackOffset = m_pausedOffset;
        LOG_DEBUG("TextToSpeech", "Resumed from offset %d", m_playbackOffset);
    }
}

void TextToSpeech::clearQueue() noexcept {
    m_queue.clear();
    LOG_DEBUG("TextToSpeech", "Queue cleared");
}

void TextToSpeech::setVoice(const String& voice) noexcept {
    strncpy(m_voice, voice.c_str(), sizeof(m_voice) - 1);
    m_voice[sizeof(m_voice) - 1] = '\0';
}

void TextToSpeech::setLanguage(const String& language) noexcept {
    strncpy(m_language, language.c_str(), sizeof(m_language) - 1);
    m_language[sizeof(m_language) - 1] = '\0';
}

void TextToSpeech::setSpeed(float speed) noexcept {
    m_speed = (speed < 0.25f) ? 0.25f : (speed > 4.0f ? 4.0f : speed);
}

void TextToSpeech::setPitch(float pitch) noexcept {
    m_pitch = (pitch < -20.0f) ? -20.0f : (pitch > 20.0f ? 20.0f : pitch);
}

void TextToSpeech::setVolume(uint8_t volume) noexcept {
    m_volume = (volume > 100) ? 100 : volume;
}

void TextToSpeech::setApiKey(const String& apiKey) noexcept {
    strncpy(m_apiKey, apiKey.c_str(), sizeof(m_apiKey) - 1);
    m_apiKey[sizeof(m_apiKey) - 1] = '\0';
}

void TextToSpeech::setApiEndpoint(const String& endpoint) noexcept {
    strncpy(m_endpoint, endpoint.c_str(), sizeof(m_endpoint) - 1);
    m_endpoint[sizeof(m_endpoint) - 1] = '\0';
}

void TextToSpeech::setRootCA(const String& caCert) noexcept {
    strncpy(m_rootCA, caCert.c_str(), sizeof(m_rootCA) - 1);
    m_rootCA[sizeof(m_rootCA) - 1] = '\0';
}

void TextToSpeech::enableStreaming() noexcept {
    m_streaming = true;
}

void TextToSpeech::disableStreaming() noexcept {
    m_streaming = false;
}

bool TextToSpeech::isBusy() const noexcept {
    return m_currentState != TTSState::IDLE;
}

bool TextToSpeech::isPlaying() const noexcept {
    return m_currentState == TTSState::PLAYING && !m_paused;
}

bool TextToSpeech::isInitialized() const noexcept {
    return m_initialized;
}

TTSState TextToSpeech::getState() const noexcept {
    return m_currentState;
}

TTSError TextToSpeech::getError() const noexcept {
    return m_lastError;
}

const TTSResponse& TextToSpeech::getResponse() const noexcept {
    return m_response;
}

size_t TextToSpeech::getQueueSize() const noexcept {
    return m_queue.size();
}

void TextToSpeech::changeState(TTSState newState) noexcept {
    if (m_currentState == newState) return;

    static constexpr bool validTransition[9][9] = {
        {0,1,0,0,0,0,0,0,1},  // IDLE -> CONNECTING, ERROR
        {0,0,1,0,0,0,0,0,1},  // CONNECTING -> SENDING, ERROR
        {0,0,0,1,0,0,0,0,1},  // SENDING -> WAITING, ERROR
        {0,0,0,0,1,0,0,0,1},  // WAITING -> RECEIVING, ERROR
        {0,0,0,0,1,1,0,0,1},  // RECEIVING -> RECEIVING, DECODING, ERROR
        {0,0,0,0,0,0,1,0,1},  // DECODING -> PLAYING, ERROR
        {0,0,0,0,0,0,1,1,1},  // PLAYING -> PLAYING, COMPLETED, ERROR
        {1,0,0,0,0,0,0,0,1},  // COMPLETED -> IDLE, ERROR
        {1,0,0,0,0,0,0,0,0}   // ERROR -> IDLE
    };

    if (!validTransition[static_cast<uint8_t>(m_currentState)][static_cast<uint8_t>(newState)]) {
        LOG_WARN("TextToSpeech", "Invalid transition %d -> %d", static_cast<int>(m_currentState), static_cast<int>(newState));
        return;
    }

    LOG_DEBUG("TextToSpeech", "State %d -> %d", static_cast<int>(m_currentState), static_cast<int>(newState));
    m_currentState = newState;
}

void TextToSpeech::setError(TTSError error) noexcept {
    if (m_lastError == error) return;
    m_lastError = error;
    m_response.error = error;
    LOG_ERROR("TextToSpeech", "Error %d", static_cast<int>(error));
}

void TextToSpeech::resetRequest() noexcept {
    m_response.clear();
    m_lastError = TTSError::NONE;
    m_responseBuffer.clear();
    m_headersParsed = false;
    m_contentLength = -1;
    m_chunkedEncoding = false;
    m_audioBufferSize = 0;
    m_playbackOffset = 0;
    m_paused = false;
    m_pausedOffset = 0;
}

bool TextToSpeech::connectToApi() noexcept {
    if (m_client.connected()) return true;

    String host = m_endpoint;
    int port = 443;

    int protoEnd = host.indexOf("://");
    if (protoEnd >= 0) host = host.substring(protoEnd + 3);

    int pathStart = host.indexOf('/');
    if (pathStart >= 0) {
        host = host.substring(0, pathStart);
    }

    int colonPos = host.indexOf(':');
    if (colonPos >= 0) {
        port = host.substring(colonPos + 1).toInt();
        host = host.substring(0, colonPos);
    }

    if (!m_client.connect(host.c_str(), port)) {
        LOG_ERROR("TextToSpeech", "TLS connect failed to %s:%d", host.c_str(), port);
        return false;
    }

    LOG_INFO("TextToSpeech", "Connected to API");
    return true;
}

void TextToSpeech::disconnectFromApi() noexcept {
    if (m_client.connected()) {
        m_client.stop();
    }
}

String TextToSpeech::buildRequest() noexcept {
    if (m_queue.empty()) return "";

    JsonDocument doc;
    doc["input"]["text"] = m_queue.front().text;
    doc["voice"]["name"] = m_queue.front().voice;
    doc["voice"]["languageCode"] = m_queue.front().language;
    doc["audioConfig"]["audioEncoding"] = "LINEAR16";
    doc["audioConfig"]["speakingRate"] = m_queue.front().speed;
    doc["audioConfig"]["pitch"] = m_queue.front().pitch;
    doc["audioConfig"]["volumeGainDb"] = (m_queue.front().volume - 50) * 0.2f;
    doc["audioConfig"]["sampleRateHertz"] = 16000;

    String output;
    output.reserve(1024);
    serializeJson(doc, output);
    return output;
}

bool TextToSpeech::sendRequest(const String& body) noexcept {
    if (!m_client.connected()) return false;

    String host = m_endpoint;
    int protoEnd = host.indexOf("://");
    if (protoEnd >= 0) host = host.substring(protoEnd + 3);
    int pathStart = host.indexOf('/');
    String path = "/";
    if (pathStart >= 0) {
        path = host.substring(pathStart);
        host = host.substring(0, pathStart);
    }
    int colonPos = host.indexOf(':');
    if (colonPos >= 0) host = host.substring(0, colonPos);

    String request;
    request.reserve(body.length() + 512);
    request = "POST " + path + "?key=" + String(m_apiKey) + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + String(body.length()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += body;

    size_t written = m_client.write(reinterpret_cast<const uint8_t*>(request.c_str()), request.length());
    if (written != request.length()) {
        LOG_ERROR("TextToSpeech", "Failed to write full request");
        return false;
    }

    m_client.flush();
    LOG_DEBUG("TextToSpeech", "Request sent (%d bytes)", body.length());
    return true;
}

String TextToSpeech::readResponse() noexcept {
    if (!m_client.connected() && !m_client.available()) {
        return "";
    }

    unsigned long startRead = millis();

    if (!m_headersParsed) {
        while (m_client.connected() || m_client.available()) {
            if (millis() - startRead > kDefaultTimeoutMs) break;

            String line = m_client.readStringUntil('\n');
            if (line.length() == 0 || line == "\r") {
                m_headersParsed = true;
                break;
            }

            line.trim();
            if (line.startsWith("Content-Length:")) {
                m_contentLength = line.substring(15).toInt();
            } else if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0) {
                m_chunkedEncoding = true;
            }
        }
    }

    if (!m_headersParsed) return "";

    if (m_chunkedEncoding) {
        while (m_client.connected() || m_client.available()) {
            String chunkSizeLine = m_client.readStringUntil('\n');
            chunkSizeLine.trim();
            int chunkSize = strtol(chunkSizeLine.c_str(), nullptr, 16);
            if (chunkSize == 0) break;

            String chunk;
            chunk.reserve(chunkSize);
            while (chunk.length() < chunkSize && (m_client.connected() || m_client.available())) {
                char c = m_client.read();
                if (c >= 0) chunk += c;
            }
            m_responseBuffer += chunk;
            m_client.readStringUntil('\n');
        }
    } else if (m_contentLength > 0) {
        while (m_responseBuffer.length() < m_contentLength && (m_client.connected() || m_client.available())) {
            char c = m_client.read();
            if (c >= 0) m_responseBuffer += c;
        }
    } else {
        while (m_client.available()) {
            char c = m_client.read();
            if (c >= 0) m_responseBuffer += c;
        }
    }

    return m_responseBuffer;
}

bool TextToSpeech::parseResponse(const String& response) noexcept {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        LOG_ERROR("TextToSpeech", "JSON parse error: %s", err.c_str());
        return false;
    }

    if (doc.containsKey("error")) {
        int code = doc["error"]["code"] | 0;
        String message = doc["error"]["message"] | "Unknown API error";
        LOG_ERROR("TextToSpeech", "API error %d: %s", code, message.c_str());
        if (code == 401 || code == 403) setError(TTSError::AUTHENTICATION);
        else setError(TTSError::API_ERROR);
        return false;
    }

    if (doc.containsKey("audioContent")) {
        m_responseBuffer = doc["audioContent"] | "";
    }

    m_response.sampleRate = doc["audioConfig"]["sampleRateHertz"] | 16000;
    m_response.channels = 1;
    m_response.bitsPerSample = 16;

    m_response.latencyMs = millis() - m_requestStartTime;
    m_response.timestamp = millis();
    m_response.error = TTSError::NONE;

    LOG_DEBUG("TextToSpeech", "Response parsed (audioContent=%d chars)", m_responseBuffer.length());
    return true;
}

bool TextToSpeech::decodeAudio(const String& base64Audio) noexcept {
    if (base64Audio.isEmpty()) {
        LOG_ERROR("TextToSpeech", "Empty base64 audio");
        return false;
    }

    size_t inputLen = base64Audio.length();
    size_t maxOutput = (inputLen * 3) / 4;
    if (maxOutput > m_audioBufferCapacity) {
        LOG_ERROR("TextToSpeech", "Decoded audio too large (%d > %d)", maxOutput, m_audioBufferCapacity);
        return false;
    }

    static const int8_t decodeTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };

    size_t outIdx = 0;
    for (size_t i = 0; i < inputLen;) {
        uint32_t sextet[4] = {0,0,0,0};
        int valid = 0;

        for (int j = 0; j < 4 && i < inputLen; ++j, ++i) {
            char c = base64Audio[i];
            if (c == '=') break;
            int8_t v = decodeTable[(uint8_t)c];
            if (v >= 0) {
                sextet[valid++] = v;
            }
        }

        if (valid < 2) break;

        m_audioBuffer[outIdx++] = (sextet[0] << 2) | (sextet[1] >> 4);
        if (valid > 2) m_audioBuffer[outIdx++] = (sextet[1] << 4) | (sextet[2] >> 2);
        if (valid > 3) m_audioBuffer[outIdx++] = (sextet[2] << 6) | sextet[3];

        if (i >= inputLen || base64Audio[i] == '=') break;
    }

    m_audioBufferSize = outIdx;
    m_response.audioSize = outIdx;
    m_response.durationMs = (m_response.sampleRate > 0) ? ((outIdx * 1000) / (m_response.sampleRate * (m_response.bitsPerSample / 8) * m_response.channels)) : 0;

    LOG_INFO("TextToSpeech", "Decoded %d bytes PCM audio (%d ms)", outIdx, m_response.durationMs);
    return true;
}

bool TextToSpeech::playAudio() noexcept {
    if (m_playbackOffset >= m_audioBufferSize) {
        return false;
    }

    size_t remaining = m_audioBufferSize - m_playbackOffset;
    size_t chunkSize = std::min<size_t>(remaining, 1024);

    size_t bytesWritten = 0;
    if (!audioManager.play(m_audioBuffer + m_playbackOffset, chunkSize, bytesWritten)) {
        LOG_ERROR("TextToSpeech", "AudioManager play failed");
        return false;
    }

    m_playbackOffset += bytesWritten;
    return m_playbackOffset < m_audioBufferSize;
}

void TextToSpeech::stopPlayback() noexcept {
    if (m_currentState == TTSState::PLAYING || m_currentState == TTSState::DECODING) {
        audioManager.stopPlayback();
        m_audioBufferSize = 0;
        m_playbackOffset = 0;
        m_paused = false;
        m_pausedOffset = 0;
    }
}

void TextToSpeech::processQueue() noexcept {
    if (m_queue.empty()) return;

    resetRequest();
    changeState(TTSState::CONNECTING);
    m_connectionStartTime = millis();
    LOG_INFO("TextToSpeech", "Processing queue item (%d chars)", m_queue.front().text.length());
}

void TextToSpeech::advanceQueue() noexcept {
    if (!m_queue.empty()) {
        m_queue.erase(m_queue.begin());
    }
}

void TextToSpeech::checkTimeouts() noexcept {
    if (m_currentState == TTSState::CONNECTING && isConnectionTimeout()) {
        LOG_WARN("TextToSpeech", "Connection timeout");
        disconnectFromApi();
        setError(TTSError::TIMEOUT);
        changeState(TTSState::ERROR);
    }
    if ((m_currentState == TTSState::WAITING || m_currentState == TTSState::RECEIVING) && isRequestTimeout()) {
        LOG_WARN("TextToSpeech", "Request timeout");
        disconnectFromApi();
        setError(TTSError::TIMEOUT);
        changeState(TTSState::ERROR);
    }
    if (m_currentState == TTSState::PLAYING && isPlaybackTimeout()) {
        LOG_WARN("TextToSpeech", "Playback timeout");
        stopPlayback();
        setError(TTSError::TIMEOUT);
        changeState(TTSState::ERROR);
    }
}

bool TextToSpeech::isConnectionTimeout() const noexcept {
    return (millis() - m_connectionStartTime) >= kConnectionTimeoutMs;
}

bool TextToSpeech::isRequestTimeout() const noexcept {
    return (millis() - m_requestStartTime) >= kDefaultTimeoutMs;
}

bool TextToSpeech::isPlaybackTimeout() const noexcept {
    return (millis() - m_playbackStartTime) >= kPlaybackTimeoutMs;
}