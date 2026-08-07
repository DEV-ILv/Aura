#include "sarvam_stt.h"
#include "audio_manager.h"
#include <math.h>

SarvamSpeechToText speechToText;

namespace {
constexpr float   kVoiceThreshold   = 350.0f;        // RMS above this = speech
constexpr unsigned long kSilenceEndMs = 550UL;       // quiet period ends utterance
constexpr size_t  kReadBudgetBytes  = 2048;          // max PCM pulled per update()
constexpr size_t  kMinPcmForApi     = 1600;          // ~50 ms before hitting the API

// Multipart body (must match the boundary used by SarvamClient).
const char kMultipartHead[] =
    "--BOUNDARY101\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
    "Content-Type: audio/wav\r\n\r\n";
const char kMultipartTail[] = "\r\n--BOUNDARY101--\r\n";
} // namespace

SarvamSpeechToText::~SarvamSpeechToText() noexcept {
    cancelRecognition();
    delete[] m_upBuf;
    m_upBuf = nullptr;
}

bool SarvamSpeechToText::initialize() noexcept {
    if (m_initialized) return true;

    if (m_apiKey.isEmpty()) m_apiKey = Secrets::SARVAM_API_KEY;
    if (m_endpoint.isEmpty()) m_endpoint = SARVAM_BASE_URL;
    if (m_rootCA.isEmpty()) m_rootCA = Secrets::SARVAM_ROOT_CA;

    sarvamClient.setApiKey(m_apiKey);
    sarvamClient.setEndpoint(m_endpoint);
    sarvamClient.setRootCA(m_rootCA);
    sarvamClient.config().sampleRate = m_sampleRate;
    sarvamClient.config().language = m_language;

    if (!m_upBuf) {
        m_upCap = kPcmOffset + kCaptureCap + 256;
        m_upBuf = new (std::nothrow) uint8_t[m_upCap];
    }
    m_initialized = (m_upBuf != nullptr) && sarvamClient.isAvailable();
    if (!m_initialized) {
        LOG_WARN("SarvamSTT", "Initialize failed (missing key/CA or no heap)");
        return false;
    }

    LOG_INFO("SarvamSTT", "Initialized (sample rate %lu Hz)", (unsigned long)m_sampleRate);
    return true;
}

void SarvamSpeechToText::run() noexcept { update(); }

void SarvamSpeechToText::update() noexcept {
    if (!m_initialized) return;

    switch (m_state) {
        case SpeechState::LISTENING:
            captureMic();
            break;
        case SpeechState::PROCESSING:
            sarvamClient.run();
            if (sarvamClient.sttDone()) {
                const SarvamSttResult& r = sarvamClient.sttResult();
                m_result.transcript = r.transcript;
                m_result.confidence = r.confidence;
                m_result.isFinal = true;
                m_result.timestamp = millis();
                m_result.audioLength = m_pcmLen;
                m_result.durationMs = (unsigned long)millis() - m_sessionStart;
                mapHttpError();
                m_error = r.ok ? SpeechError::NONE : m_pendingError;
                restoreMic();
                setState(SpeechState::COMPLETED);
                if (!r.transcript.isEmpty()) {
                    LOG_INFO("SarvamSTT", "Final transcript (%lu ms): %s",
                             m_result.durationMs, r.transcript.c_str());
                }
            }
            break;
        default:
            break;
    }
}

void SarvamSpeechToText::captureMic() noexcept {
    size_t budget = kReadBudgetBytes;
    while (budget >= 512) {
        const size_t avail = kCaptureCap - m_pcmLen;
        if (avail == 0) break;
        size_t n = 0;
        const size_t want = (avail > 512) ? 512 : avail;
        if (!audioManager.record(m_upBuf + kPcmOffset + m_pcmLen, want, n)) break;
        if (n == 0) break;

        if (frameEnergy(m_upBuf + kPcmOffset + m_pcmLen, n) > kVoiceThreshold) {
            m_sawVoice = true;
            m_lastVoiceMs = millis();
        }
        m_pcmLen += n;
        budget -= n;
    }

    const unsigned long now = millis();
    if (m_sawVoice && m_pcmLen >= kMinPcmForApi && (now - m_lastVoiceMs >= kSilenceEndMs)) {
        finalizeCapture(true);
        return;
    }
    if (!m_sawVoice && (now - m_sessionStart >= m_timeoutMs)) {
        LOG_INFO("SarvamSTT", "No speech within timeout; skipping upload");
        finalizeCapture(false);
        return;
    }
    if (m_pcmLen >= kCaptureCap) {
        finalizeCapture(true);
        return;
    }
}

void SarvamSpeechToText::finalizeCapture(bool requestTranscription) noexcept {
    restoreMic();

    setState(SpeechState::PROCESSING);
    m_apiState = ApiState::CONNECTING;

    if (!requestTranscription || m_pcmLen < kMinPcmForApi) {
        m_result.clear();
        m_result.isFinal = true;
        m_result.timestamp = millis();
        m_result.durationMs = (unsigned long)millis() - m_sessionStart;
        m_error = SpeechError::NONE;
        m_apiState = ApiState::DISCONNECTED;
        setState(SpeechState::COMPLETED);
        LOG_INFO("SarvamSTT", "Completed with empty transcript");
        return;
    }

    assembleMultipart();
    m_pendingError = SpeechError::NONE;
    sarvamClient.startTranscription(m_upBuf, m_totalLen);
}

void SarvamSpeechToText::assembleMultipart() noexcept {
    const size_t hl = strlen(kMultipartHead);
    const size_t tl = strlen(kMultipartTail);
    memcpy(m_upBuf, kMultipartHead, hl);

    uint8_t wav[44];
    SarvamClient::buildWavHeader(wav, sizeof(wav), m_sampleRate, (uint32_t)m_pcmLen);
    memcpy(m_upBuf + hl, wav, 44);

    const size_t dataAt = hl + 44;
    if (kPcmOffset != dataAt) {
        memmove(m_upBuf + dataAt, m_upBuf + kPcmOffset, m_pcmLen);
    }
    memcpy(m_upBuf + dataAt + m_pcmLen, kMultipartTail, tl);
    m_totalLen = dataAt + m_pcmLen + tl;
    LOG_INFO("SarvamSTT", "Uploading %u bytes PCM (~%lu ms)",
             (unsigned)m_pcmLen, (unsigned long)(m_pcmLen * 1000UL / m_sampleRate / 2U));
}

void SarvamSpeechToText::mapHttpError() noexcept {
    switch (sarvamClient.lastError()) {
        case SarvamHttpError::TIMEOUT:        m_pendingError = SpeechError::TIMEOUT; break;
        case SarvamHttpError::NETWORK:        m_pendingError = SpeechError::NETWORK; break;
        case SarvamHttpError::AUTHENTICATION: m_pendingError = SpeechError::UNAUTHORIZED; break;
        case SarvamHttpError::TLS_ERROR:      m_pendingError = SpeechError::TLS_ERROR; break;
        case SarvamHttpError::HTTP_ERROR:
        case SarvamHttpError::JSON_ERROR:
        case SarvamHttpError::UNKNOWN:        m_pendingError = SpeechError::API_ERROR; break;
        default:                              m_pendingError = SpeechError::NONE; break;
    }
}

void SarvamSpeechToText::restoreMic() noexcept {
    if (audioManager.isRecording()) audioManager.stopRecording();
    if (m_initialized) audioManager.setVoiceActivityMonitoring(true);
}

float SarvamSpeechToText::frameEnergy(const uint8_t* buf, size_t bytes) const noexcept {
    const size_t n = bytes / 2;
    if (n == 0) return 0.0f;
    double sum = 0.0;
    const int16_t* p = reinterpret_cast<const int16_t*>(buf);
    for (size_t i = 0; i < n; ++i) sum += (double)p[i] * (double)p[i];
    return (float)sqrt(sum / (double)n);
}

bool SarvamSpeechToText::startRecognition(RecognitionMode mode) noexcept {
    if (!m_initialized) return false;
    m_mode = mode;

    m_result.clear();
    m_error = SpeechError::NONE;
    m_pcmLen = 0;
    m_sawVoice = false;
    m_pendingError = SpeechError::NONE;
    m_sessionStart = millis();
    m_lastVoiceMs = m_sessionStart;

    if (audioManager.isInitialized()) {
        audioManager.setVoiceActivityMonitoring(false);
        audioManager.startRecording();
    }

    setState(SpeechState::LISTENING);
    m_apiState = ApiState::DISCONNECTED;
    LOG_INFO("SarvamSTT", "Listening (mode=%d)", (int)mode);
    return true;
}

bool SarvamSpeechToText::stopRecognition() noexcept {
    if (m_state != SpeechState::LISTENING && m_state != SpeechState::PROCESSING) return false;
    if (m_state == SpeechState::LISTENING) finalizeCapture(true);
    return true;
}

void SarvamSpeechToText::cancelRecognition() noexcept {
    sarvamClient.cancel();
    restoreMic();
    m_pcmLen = 0;
    setState(SpeechState::IDLE);
    m_apiState = ApiState::DISCONNECTED;
}

void SarvamSpeechToText::processAudio(const int16_t* audioData, size_t length) noexcept {
    (void)audioData; (void)length; // capture is self-managed via AudioManager
}

void SarvamSpeechToText::processAudioChunk(const AudioChunk& chunk) noexcept {
    (void)chunk;
}

bool SarvamSpeechToText::isRecognizing() const noexcept {
    return m_state == SpeechState::LISTENING || m_state == SpeechState::PROCESSING;
}
bool SarvamSpeechToText::isBusy() const noexcept {
    return m_state == SpeechState::LISTENING || m_state == SpeechState::PROCESSING;
}
bool SarvamSpeechToText::isInitialized() const noexcept { return m_initialized; }
const SpeechResult& SarvamSpeechToText::getResult() const noexcept { return m_result; }

void SarvamSpeechToText::clearResult() noexcept {
    m_result.clear();
    if (m_state == SpeechState::COMPLETED) setState(SpeechState::IDLE);
}

void SarvamSpeechToText::setState(SpeechState s) noexcept { m_state = s; }

void SarvamSpeechToText::setLanguage(const String& language) noexcept {
    m_language = language;
    sarvamClient.config().language = language;
}
void SarvamSpeechToText::setTimeout(unsigned long timeoutMs) noexcept { m_timeoutMs = timeoutMs; }
void SarvamSpeechToText::setSampleRate(uint32_t sampleRate) noexcept { m_sampleRate = sampleRate; }
void SarvamSpeechToText::setAudioFormat(AudioFormat format) noexcept { m_format = format; }
void SarvamSpeechToText::setMode(RecognitionMode mode) noexcept { m_mode = mode; }
SpeechState SarvamSpeechToText::getState() const noexcept { return m_state; }
SpeechError SarvamSpeechToText::getError() const noexcept { return m_error; }

ApiState SarvamSpeechToText::getApiState() const noexcept {
    if (m_state == SpeechState::PROCESSING) {
        return sarvamClient.sttInProgress() ? ApiState::CONNECTING : ApiState::DISCONNECTED;
    }
    return m_apiState;
}

void SarvamSpeechToText::setApiEndpoint(const String& endpoint) noexcept {
    m_endpoint = endpoint;
    sarvamClient.setEndpoint(endpoint);
}
void SarvamSpeechToText::setApiKey(const String& apiKey) noexcept {
    m_apiKey = apiKey;
    sarvamClient.setApiKey(apiKey);
}
void SarvamSpeechToText::setRootCA(const String& caCert) noexcept {
    m_rootCA = caCert;
    sarvamClient.setRootCA(caCert);
}
void SarvamSpeechToText::setPartialResults(bool enable) noexcept { m_partialResults = enable; }
void SarvamSpeechToText::setProfanityFilter(bool enable) noexcept { m_profanityFilter = enable; }
void SarvamSpeechToText::setMaxAlternatives(uint8_t max) noexcept { m_maxAlternatives = max; }

void SarvamSpeechToText::getBufferStats(size_t& usedBytes, size_t& capacityBytes) const noexcept {
    usedBytes = m_pcmLen;
    capacityBytes = kCaptureCap;
}
bool SarvamSpeechToText::isBufferNearFull() const noexcept {
    return (m_pcmLen + kReadBudgetBytes) >= kCaptureCap;
}
bool SarvamSpeechToText::flushBuffer() noexcept { return true; }
bool SarvamSpeechToText::sendAudio() noexcept { return false; }