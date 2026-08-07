#include "sarvam_tts.h"
#include "audio_manager.h"

SarvamTextToSpeech textToSpeech;

namespace {
constexpr size_t kMaxQueueItems = 16;
constexpr uint32_t kOutSampleRate = 16000;
constexpr uint16_t kOutChannels = 1;
} // namespace

bool SarvamTextToSpeech::pcmChunkCallback(const uint8_t* data, size_t len, void* user) noexcept {
    if (!user) return false;
    auto* self = static_cast<SarvamTextToSpeech*>(user);
    return self->onPcm(data, len);
}

SarvamTextToSpeech::~SarvamTextToSpeech() noexcept {
    stop();
}

bool SarvamTextToSpeech::initialize() noexcept {
    if (m_initialized) return true;

    if (m_apiKey.isEmpty()) m_apiKey = Secrets::SARVAM_API_KEY;
    if (m_endpoint.isEmpty()) m_endpoint = SARVAM_BASE_URL;
    if (m_rootCA.isEmpty()) m_rootCA = Secrets::SARVAM_ROOT_CA;

    sarvamClient.setApiKey(m_apiKey);
    sarvamClient.setEndpoint(m_endpoint);
    sarvamClient.setRootCA(m_rootCA);
    sarvamClient.config().voice = m_voice;
    sarvamClient.config().language = m_language;

    m_initialized = sarvamClient.isAvailable();
    if (!m_initialized) {
        LOG_WARN("SarvamTTS", "Initialize failed (missing key/CA)");
        return false;
    }
    LOG_INFO("SarvamTTS", "Initialized (voice=%s, lang=%s)", m_voice.c_str(), m_language.c_str());
    return true;
}

void SarvamTextToSpeech::run() noexcept { update(); }

void SarvamTextToSpeech::update() noexcept {
    if (!m_initialized) return;

    if (m_srvActive) sarvamClient.run();

    if (!m_paused) drainPlayback();

    if (m_srvActive) {
        if (m_state == TTSState::CONNECTING && sarvamClient.ttsInProgress()) {
            m_state = TTSState::RECEIVING;
        }
        if (sarvamClient.ttsDone()) {
            m_srvActive = false;
            if (sarvamClient.ttsOk()) {
                m_state = TTSState::PLAYING;
                m_draining = true;
            } else {
                mapHttpError();
                finishItem(false, m_error);
            }
        }
    } else if (m_draining) {
        // Waiting for the ring + speaker to flush.
        if (m_ringCount == 0 && !audioManager.isPlaying()) {
            finishItem(true, TTSError::NONE);
        }
    }

    if (m_state == TTSState::IDLE && !m_paused && !m_queue.empty()) {
        beginNext();
    }
}

void SarvamTextToSpeech::beginNext() noexcept {
    if (m_queue.empty()) return;

    m_current = m_queue.front();
    m_queue.pop_front();

    if (!m_current.voice.isEmpty()) sarvamClient.config().voice = m_current.voice;
    else sarvamClient.config().voice = m_voice;
    if (!m_current.language.isEmpty()) sarvamClient.config().language = m_current.language;
    else sarvamClient.config().language = m_language;

    if (audioManager.isInitialized()) audioManager.setVolume(m_volume);

    m_audioPushedBytes = 0;
    m_ringDropped = 0;
    m_draining = false;
    m_playStarted = false;
    m_error = TTSError::NONE;
    m_state = TTSState::CONNECTING;

    sarvamClient.startSynthesis(m_current.text, pcmChunkCallback, this);
    m_srvActive = true;
    LOG_INFO("SarvamTTS", "Synthesizing: %s", m_current.text.c_str());
}

bool SarvamTextToSpeech::onPcm(const uint8_t* data, size_t len) noexcept {
    if (!m_srvActive) return false;
    ringPut(data, len);
    return true;
}

void SarvamTextToSpeech::drainPlayback() noexcept {
    if (m_ringCount == 0) return;

    if (!audioManager.isPlaying()) {
        if (m_ringCount > 0 && audioManager.startPlayback()) {
            m_playStarted = true;
        } else {
            return;
        }
    }

    uint8_t frame[512];
    int guard = 0;
    while (m_ringCount > 0 && guard < 4) {
        const size_t give = (m_ringCount < sizeof(frame)) ? m_ringCount : sizeof(frame);
        const size_t rd = ringPeek(frame, give);
        if (rd == 0) break;
        size_t wr = 0;
        audioManager.play(frame, rd, wr);
        m_audioPushedBytes += wr;
        ringAdvance(wr);
        if (wr < rd) break; // speaker backpressure; try again next tick
        ++guard;
    }
}

void SarvamTextToSpeech::finishItem(bool ok, TTSError err) noexcept {
    if (ok) {
        m_result.latencyMs = sarvamClient.ttsLatencyMs();
        m_result.audioSize = (size_t)m_audioPushedBytes;
        m_result.sampleRate = kOutSampleRate;
        m_result.channels = kOutChannels;
        m_result.bitsPerSample = 16;
        m_result.durationMs = (unsigned long)((uint64_t)m_audioPushedBytes * 1000UL / (kOutSampleRate * 2U));
        m_result.timestamp = millis();
        m_result.error = TTSError::NONE;
        m_error = TTSError::NONE;
        LOG_INFO("SarvamTTS", "Spoken %u ms (%u bytes%s)", m_result.durationMs,
                 (unsigned)m_audioPushedBytes, m_ringDropped ? ", ring overflows" : "");
    } else {
        m_result.error = err;
        m_result.timestamp = millis();
        m_result.latencyMs = sarvamClient.ttsLatencyMs();
        m_error = err;
        LOG_WARN("SarvamTTS", "Synthesis failed (err=%d)", (int)err);
    }

    m_srvActive = false;
    m_draining = false;
    m_playStarted = false;
    m_audioPushedBytes = 0;
    m_ringHead = 0;
    m_ringTail = 0;
    m_ringCount = 0;
    m_current = TTSQueueItem();
    m_state = TTSState::IDLE;
}

bool SarvamTextToSpeech::speak(const String& text, bool priority) noexcept {
    if (!m_initialized) return false;
    if (text.isEmpty()) return false;
    if (m_queue.size() >= kMaxQueueItems) return false;

    TTSQueueItem item(text, m_voice, m_language, m_speed, m_pitch, m_volume, priority);
    if (priority) m_queue.push_front(item);
    else m_queue.push_back(item);
    return true;
}

void SarvamTextToSpeech::stop() noexcept {
    sarvamClient.cancel();
    m_srvActive = false;
    m_draining = false;
    m_playStarted = false;
    if (audioManager.isPlaying()) audioManager.stopPlayback();
    m_ringHead = m_ringTail = m_ringCount = 0;
    m_audioPushedBytes = 0;
    m_current = TTSQueueItem();
    m_queue.clear();
    m_paused = false;
    m_state = TTSState::IDLE;
}

void SarvamTextToSpeech::pause() noexcept { m_paused = true; }
void SarvamTextToSpeech::resume() noexcept { m_paused = false; }
void SarvamTextToSpeech::clearQueue() noexcept { m_queue.clear(); }

void SarvamTextToSpeech::setVoice(const String& voice) noexcept {
    m_voice = voice;
    if (!m_current.text.isEmpty()) sarvamClient.config().voice = voice;
}
void SarvamTextToSpeech::setLanguage(const String& language) noexcept {
    m_language = language;
    if (!m_current.text.isEmpty()) sarvamClient.config().language = language;
}
void SarvamTextToSpeech::setSpeed(float speed) noexcept { m_speed = speed; }
void SarvamTextToSpeech::setPitch(float pitch) noexcept { m_pitch = pitch; }
void SarvamTextToSpeech::setVolume(uint8_t volume) noexcept {
    m_volume = volume;
    if (audioManager.isInitialized()) audioManager.setVolume(volume);
}
void SarvamTextToSpeech::setApiKey(const String& apiKey) noexcept {
    m_apiKey = apiKey;
    sarvamClient.setApiKey(apiKey);
}
void SarvamTextToSpeech::setApiEndpoint(const String& endpoint) noexcept {
    m_endpoint = endpoint;
    sarvamClient.setEndpoint(endpoint);
}
void SarvamTextToSpeech::setRootCA(const String& caCert) noexcept {
    m_rootCA = caCert;
    sarvamClient.setRootCA(caCert);
}
void SarvamTextToSpeech::enableStreaming() noexcept { m_streaming = true; }
void SarvamTextToSpeech::disableStreaming() noexcept { m_streaming = false; }

bool SarvamTextToSpeech::isBusy() const noexcept {
    return m_state != TTSState::IDLE || !m_queue.empty();
}
bool SarvamTextToSpeech::isPlaying() const noexcept {
    return audioManager.isPlaying();
}
bool SarvamTextToSpeech::isInitialized() const noexcept { return m_initialized; }
TTSState SarvamTextToSpeech::getState() const noexcept { return m_state; }
TTSError SarvamTextToSpeech::getError() const noexcept { return m_error; }
const TTSResponse& SarvamTextToSpeech::getResponse() const noexcept { return m_result; }
size_t SarvamTextToSpeech::getQueueSize() const noexcept { return m_queue.size(); }

void SarvamTextToSpeech::mapHttpError() noexcept {
    switch (sarvamClient.lastError()) {
        case SarvamHttpError::TIMEOUT:        m_error = TTSError::TIMEOUT; break;
        case SarvamHttpError::NETWORK:        m_error = TTSError::NETWORK; break;
        case SarvamHttpError::AUTHENTICATION: m_error = TTSError::AUTHENTICATION; break;
        case SarvamHttpError::TLS_ERROR:      m_error = TTSError::TLS_ERROR; break;
        case SarvamHttpError::HTTP_ERROR:     m_error = TTSError::API_ERROR; break;
        case SarvamHttpError::JSON_ERROR:     m_error = TTSError::JSON_ERROR; break;
        default:                              m_error = TTSError::UNKNOWN; break;
    }
}

void SarvamTextToSpeech::ringPut(const uint8_t* data, size_t len) noexcept {
    if (len == 0) return;
    if (len > kRingCap) { // oversized chunk: truncate to available, drop the rest
        len = kRingCap;
        m_ringDropped += len;
    }
    while (len > 0) {
        if (m_ringCount >= kRingCap) {
            // Overflow: drop oldest bytes to keep decoding live.
            ringAdvance(1);
            ++m_ringDropped;
        }
        const size_t spaceTail = kRingCap - m_ringHead;
        const size_t n = (len < spaceTail) ? len : spaceTail;
        memcpy(m_ring + m_ringHead, data, n);
        m_ringHead = (m_ringHead + (uint32_t)n) % kRingCap;
        m_ringCount += (uint32_t)n;
        data += n;
        len -= n;
    }
}

size_t SarvamTextToSpeech::ringPeek(uint8_t* out, size_t cap) noexcept {
    if (m_ringCount == 0 || cap == 0) return 0;
    const size_t n = (m_ringCount < cap) ? m_ringCount : cap;
    const size_t first = kRingCap - m_ringTail;
    if (n <= first) {
        memcpy(out, m_ring + m_ringTail, n);
    } else {
        memcpy(out, m_ring + m_ringTail, first);
        memcpy(out + first, m_ring, n - first);
    }
    return n;
}

void SarvamTextToSpeech::ringAdvance(size_t n) noexcept {
    if (n >= m_ringCount) {
        m_ringHead = m_ringTail = 0;
        m_ringCount = 0;
    } else {
        m_ringTail = (m_ringTail + (uint32_t)n) % kRingCap;
        m_ringCount -= (uint32_t)n;
    }
}

size_t SarvamTextToSpeech::ringCount() const noexcept { return m_ringCount; }