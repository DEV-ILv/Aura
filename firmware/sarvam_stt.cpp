#include "sarvam_stt.h"
#include "audio_manager.h"
#include <math.h>

SarvamSpeechToText speechToText;

namespace {
constexpr float   kVoiceThreshold   = 350.0f;        // RMS above this = speech
constexpr unsigned long kSilenceEndMs = 550UL;       // quiet period ends utterance
constexpr size_t  kReadBudgetBytes  = 2048;          // max PCM pulled per update()
constexpr size_t  kMinPcmForApi     = 1600;          // ~50 ms before hitting the API
constexpr unsigned long kMaxUtteranceMs = 60000UL;   // hard cap per utterance

// Raw PCM multipart body parts (boundary must match SarvamClient). The codec
// field precedes the file field and tells Sarvam the payload is s16le PCM.
const char kMultipartHead[] =
    "--BOUNDARY101\r\n"
    "Content-Disposition: form-data; name=\"input_audio_codec\"\r\n\r\n"
    "pcm_s16le\r\n"
    "--BOUNDARY101\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"audio.pcm\"\r\n"
    "Content-Type: application/octet-stream\r\n\r\n";
const char kMultipartTail[] = "\r\n--BOUNDARY101--\r\n";
} // namespace

SarvamSpeechToText::~SarvamSpeechToText() noexcept {
    cancelRecognition();
    if (m_pcmRing) {
        free(m_pcmRing);
        m_pcmRing = nullptr;
    }
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

    m_initialized = sarvamClient.isAvailable();
    if (!m_initialized) {
        LOG_WARN("SarvamSTT", "Initialize failed (missing key/CA)");
        return false;
    }

    // Allocate the PCM ring buffer on heap to avoid BSS bloat at boot
    if (!m_pcmRing) {
        m_pcmRing = (uint8_t*)malloc(kPcmRingCap);
        if (!m_pcmRing) {
            LOG_ERROR("SarvamSTT", "Failed to allocate %u B PCM ring", (unsigned)kPcmRingCap);
            m_initialized = false;
            return false;
        }
    }

    LOG_INFO("SarvamSTT", "Initialized (sample rate %lu Hz, ring %u B)",
             (unsigned long)m_sampleRate, (unsigned)kPcmRingCap);
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
    // Adaptive voice threshold: use the AudioManager's (adaptively updated)
    // noise floor + threshold when available, falling back to the fixed 350.
    const float adaptiveFloor =
        static_cast<float>(audioManager.getNoiseFloor()) +
        static_cast<float>(audioManager.getNoiseThreshold());
    const float voiceThreshold = (adaptiveFloor > 0.0f) ? adaptiveFloor : kVoiceThreshold;

    uint8_t chunk[512];
    size_t budget = kReadBudgetBytes;
    while (budget >= 512) {
        size_t n = 0;
        if (!audioManager.record(chunk, 512, n)) break;
        if (n == 0) break;

        if (frameEnergy(chunk, n) > voiceThreshold) {
            m_sawVoice = true;
            m_lastVoiceMs = millis();
        }
        m_pcmLen += n;
        ringPut(chunk, n);

        // Kick off the chunked upload as soon as real speech is present.
        if (m_sawVoice && !m_streaming && m_pcmLen >= kMinPcmForApi) {
            beginStream();
        }
        if (m_streaming) feedClientRing();
        budget -= n;
    }
    if (m_streaming) feedClientRing();

    // Mid-stream failure (no retry possible once bytes are on the socket).
    if (m_streaming && sarvamClient.sttDone()) {
        m_pendingError = SpeechError::NONE;
        mapHttpError();
        m_result.clear();
        m_result.isFinal = true;
        m_result.timestamp = millis();
        m_result.durationMs = (unsigned long)millis() - m_sessionStart;
        m_error = m_pendingError;
        m_apiState = ApiState::DISCONNECTED;
        setState(SpeechState::COMPLETED);
        LOG_WARN("SarvamSTT", "Stream failed mid-utterance (err %d)", (int)sarvamClient.lastError());
        return;
    }

    const unsigned long now = millis();
    // Silence ended the utterance. finalizeCapture() completes with an empty
    // transcript when the utterance is too short to upload (< kMinPcmForApi);
    // requiring m_pcmLen >= kMinPcmForApi here would wedge LISTENING forever on
    // a short burst (m_sawVoice set, pcmLen too small, timeout branch dead).
    if (m_sawVoice && (now - m_lastVoiceMs >= kSilenceEndMs)) {
        finalizeCapture(m_pcmLen >= kMinPcmForApi);
        return;
    }
    if (!m_sawVoice && (now - m_sessionStart >= m_timeoutMs)) {
        LOG_INFO("SarvamSTT", "No speech within timeout; skipping upload");
        finalizeCapture(false);
        return;
    }
    // Hard utterance cap: a continuous speech/noise stream must not hold
    // LISTENING open indefinitely. Finalize with whatever was captured so a
    // long dictation is still transcribed rather than dropped.
    if (m_sawVoice && (now - m_sessionStart >= kMaxUtteranceMs)) {
        LOG_INFO("SarvamSTT", "Max utterance length reached - finalizing");
        finalizeCapture(true);
        return;
    }
}

void SarvamSpeechToText::beginStream() noexcept {
    m_pendingError = SpeechError::NONE;
    sarvamClient.startTranscriptionStream(kMultipartHead, strlen(kMultipartHead),
                                          kMultipartTail, strlen(kMultipartTail));
    m_streaming = true;
    m_apiState = ApiState::CONNECTING;
    LOG_INFO("SarvamSTT", "Streaming upload started");
    feedClientRing();
}

void SarvamSpeechToText::feedClientRing() noexcept {
    sarvamClient.run();
    while (m_ringCount > 0 && sarvamClient.sttStreamActive()) {
        const size_t take = (m_ringCount > 512) ? 512 : m_ringCount;
        uint8_t buf[512];
        for (size_t i = 0; i < take; ++i) {
            buf[i] = m_pcmRing[(m_ringTail + i) % kPcmRingCap];
        }
        if (!sarvamClient.streamPcm(buf, take)) break;
        m_ringTail = (m_ringTail + take) % kPcmRingCap;
        m_ringCount -= take;
    }
}

void SarvamSpeechToText::ringPut(const uint8_t* data, size_t len) noexcept {
    for (size_t i = 0; i < len; ++i) {
        m_pcmRing[m_ringHead] = data[i];
        m_ringHead = (m_ringHead + 1) % kPcmRingCap;
        if (m_ringCount < kPcmRingCap) {
            ++m_ringCount;
        } else {
            m_ringTail = (m_ringTail + 1) % kPcmRingCap;  // overwrite oldest
        }
    }
}

void SarvamSpeechToText::finalizeCapture(bool requestTranscription) noexcept {
    restoreMic();

    setState(SpeechState::PROCESSING);
    m_apiState = ApiState::CONNECTING;

    if (!requestTranscription || m_pcmLen < kMinPcmForApi) {
        sarvamClient.cancel();
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

    // Flush remaining PCM, then close the chunked body.
    feedClientRing();
    sarvamClient.endTranscriptionStream();
    LOG_INFO("SarvamSTT", "Uploaded %u bytes PCM (~%lu ms)",
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
    audioManager.setCaptureConsumerActive(false);
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

    // A prior session may still be mid-flight (e.g. TRANSCRIBING timed out at
    // the conversation layer while the Sarvam upload was still running). Tear
    // it down so this session cannot inherit a stale stream or ring data.
    if (m_state == SpeechState::PROCESSING || m_streaming) {
        sarvamClient.cancel();
        m_streaming = false;
        m_ringHead = 0;
        m_ringTail = 0;
        m_ringCount = 0;
    }

    if (audioManager.isInitialized()) {
        audioManager.setVoiceActivityMonitoring(false);
        audioManager.setCaptureConsumerActive(true);
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
    m_streaming = false;
    m_ringHead = 0;
    m_ringTail = 0;
    m_ringCount = 0;
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

void SarvamSpeechToText::clearError() noexcept {
    m_error = SpeechError::NONE;
    m_pendingError = SpeechError::NONE;
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
    usedBytes = m_ringCount;
    capacityBytes = kPcmRingCap;
}
bool SarvamSpeechToText::isBufferNearFull() const noexcept {
    return (m_ringCount + kReadBudgetBytes) >= kPcmRingCap;
}
bool SarvamSpeechToText::flushBuffer() noexcept { return true; }
bool SarvamSpeechToText::sendAudio() noexcept { return false; }