#include "sound_manager.h"

// ============================================================================
// Global instance
// ============================================================================

SoundManager soundManager;

// ============================================================================
// Anonymous namespace – internal helpers
// ============================================================================

namespace {

constexpr const char* kTag = "SoundManager";

/**
 * @brief Compute a linear amplitude envelope.
 * @param position    Current sample index (0-based).
 * @param total       Total samples in the tone segment.
 * @param fadeInSmp   Number of fade-in samples.
 * @param fadeOutSmp  Number of fade-out samples.
 * @return Envelope factor in [0.0, 1.0].
 */
inline float amplitudeEnvelope(uint32_t position, uint32_t total,
                               uint32_t fadeInSmp,
                               uint32_t fadeOutSmp) noexcept {
    if (position < fadeInSmp && fadeInSmp > 0) {
        return static_cast<float>(position) / static_cast<float>(fadeInSmp);
    }
    const uint32_t fadeStart = total - fadeOutSmp;
    if (position >= fadeStart && fadeOutSmp > 0 && total > fadeOutSmp) {
        const uint32_t dist = position - fadeStart;
        return 1.0f - (static_cast<float>(dist) / static_cast<float>(fadeOutSmp));
    }
    return 1.0f;
}

} // anonymous namespace

// ============================================================================
// Constructor
// ============================================================================

SoundManager::SoundManager() noexcept
    : m_initialized(false)
    , m_state(PlayState::IDLE)
    , m_queueHead(0)
    , m_queueTail(0)
    , m_queueCount(0)
    , m_activeTone()
    , m_activeSampleOffset(0)
    , m_activeTotalSamples(0)
    , m_loopEnabled(false)
    , m_loopTone()
    , m_sampleBuffer{} {}

// ============================================================================
// Destructor
// ============================================================================

SoundManager::~SoundManager() noexcept {
    stop();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool SoundManager::initialize() noexcept {
    if (m_initialized) {
        return true;
    }

    if (!audioManager.isInitialized()) {
        LOG_ERROR(kTag, "AudioManager not initialized");
        return false;
    }

    m_initialized = true;
    LOG_INFO(kTag, "Initialized");
    return true;
}

void SoundManager::run() noexcept {
    update();
}

void SoundManager::update() noexcept {
    if (!m_initialized || m_state == PlayState::IDLE) {
        return;
    }

    if (!audioManager.isPlaying()) {
        if (!audioManager.isInitialized()) {
            m_state = PlayState::IDLE;
            return;
        }
        audioManager.startPlayback();
    }

    generateChunk();
    deliverChunk();

    if (m_activeTone.durationMs > 0 &&
        m_activeSampleOffset >= m_activeTotalSamples) {

        if (m_queueCount > 0) {
            advanceToNextTone();
        } else if (m_loopEnabled) {
            enqueueTone(m_loopTone.frequency, m_loopTone.durationMs,
                        m_loopTone.amplitude,
                        m_loopTone.fadeInMs, m_loopTone.fadeOutMs);
            advanceToNextTone();
        } else {
            m_state = PlayState::IDLE;
            flushPlayback();
            LOG_DEBUG(kTag, "Playback complete");
        }
    }
}

// ============================================================================
// Playback control
// ============================================================================

void SoundManager::stop() noexcept {
    if (m_state == PlayState::IDLE && m_queueCount == 0) {
        return;
    }

    m_loopEnabled = false;
    clearQueue();
    m_state = PlayState::IDLE;

    if (audioManager.isInitialized()) {
        audioManager.flushPlayback();
        audioManager.stopPlayback();
    }

    LOG_DEBUG(kTag, "Playback stopped");
}

bool SoundManager::isPlaying() const noexcept {
    return m_state != PlayState::IDLE;
}

bool SoundManager::isInitialized() const noexcept {
    return m_initialized;
}

// ============================================================================
// UI sound effects
// ============================================================================

void SoundManager::playStartup() noexcept {
    if (AudioAssetManager::instance().playAsset("startup")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(523, 150, 128, 10, 10);   // C5
    enqueueTone(659, 150, 128, 10, 10);   // E5
    enqueueTone(784, 200, 128, 10, 20);   // G5
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Startup sound");
}

void SoundManager::playShutdown() noexcept {
    if (AudioAssetManager::instance().playAsset("shutdown")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(784, 150, 128, 10, 10);   // G5
    enqueueTone(659, 150, 128, 10, 10);   // E5
    enqueueTone(523, 200, 128, 10, 30);   // C5
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Shutdown sound");
}

void SoundManager::playReady() noexcept {
    if (AudioAssetManager::instance().playAsset("ready")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(880, 100, 80, 10, 20);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Ready sound");
}

void SoundManager::playListening() noexcept {
    if (AudioAssetManager::instance().playAsset("listening")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(1000, 20, 160, 2, 5);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Listening click");
}

void SoundManager::playThinking() noexcept {
    clearQueue();
    m_loopEnabled = true;
    m_loopTone = SoundTone(440, 500, 100, 20, 20);
    enqueueTone(440, 500, 100, 20, 20);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Thinking tone started");
}

void SoundManager::stopThinking() noexcept {
    m_loopEnabled = false;
    clearQueue();
    LOG_DEBUG(kTag, "Thinking tone stopped");
}

void SoundManager::playConfirmation() noexcept {
    if (AudioAssetManager::instance().playAsset("confirmation")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(523, 100, 128, 5, 10);   // C5
    enqueueTone(659, 150, 128, 10, 20);  // E5
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Confirmation sound");
}

void SoundManager::playSuccess() noexcept {
    if (AudioAssetManager::instance().playAsset("success")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(523, 100, 128, 5, 10);   // C5
    enqueueTone(659, 100, 128, 5, 10);   // E5
    enqueueTone(784, 150, 128, 10, 20);  // G5
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Success sound");
}

void SoundManager::playNotification() noexcept {
    if (AudioAssetManager::instance().playAsset("notification")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(880, 150, 128, 10, 10);
    enqueueTone(0,   100, 0,   0,  0);   // pause
    enqueueTone(880, 150, 128, 10, 20);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Notification sound");
}

void SoundManager::playReminder() noexcept {
    clearQueue();
    m_loopEnabled = true;
    m_loopTone = SoundTone(660, 200, 100, 15, 15);
    enqueueTone(660, 200, 100, 15, 15);
    enqueueTone(0,   200, 0,   0,  0);    // pause between repeats
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Reminder tone started");
}

void SoundManager::playWifiConnected() noexcept {
    if (AudioAssetManager::instance().playAsset("wifi_connected")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(523, 80,  128, 5, 5);    // C5
    enqueueTone(659, 80,  128, 5, 5);    // E5
    enqueueTone(784, 120, 128, 5, 20);   // G5
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "WiFi connected sound");
}

void SoundManager::playWifiError() noexcept {
    if (AudioAssetManager::instance().playAsset("wifi_error")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(250, 100, 200, 5, 10);
    enqueueTone(0,   100, 0,   0,  0);   // pause
    enqueueTone(250, 100, 200, 5, 10);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "WiFi error sound");
}

void SoundManager::playOtaStart() noexcept {
    if (AudioAssetManager::instance().playAsset("ota_start")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(440, 80,  128, 5, 5);
    enqueueTone(880, 80,  128, 5, 5);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "OTA start sound");
}

void SoundManager::playOtaFinished() noexcept {
    if (AudioAssetManager::instance().playAsset("ota_finished")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(523, 100, 128, 5, 10);   // C5
    enqueueTone(659, 100, 128, 5, 10);   // E5
    enqueueTone(880, 200, 128, 10, 30);  // A5
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "OTA finished sound");
}

void SoundManager::playButtonClick() noexcept {
    if (AudioAssetManager::instance().playAsset("button_click")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(2000, 10, 160, 2, 3);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Button click");
}

void SoundManager::playTouch() noexcept {
    if (AudioAssetManager::instance().playAsset("touch")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(1500, 20, 140, 3, 5);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Touch sound");
}

void SoundManager::playError() noexcept {
    if (AudioAssetManager::instance().playAsset("error")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(300, 100, 200, 5, 10);
    enqueueTone(200, 150, 200, 10, 30);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Error sound");
}

void SoundManager::playWarning() noexcept {
    if (AudioAssetManager::instance().playAsset("warning")) return;
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(880, 80, 200, 5, 5);
    enqueueTone(0,   80, 0,   0, 0);     // pause
    enqueueTone(880, 80, 200, 5, 5);
    beginHardwarePlayback();
    LOG_DEBUG(kTag, "Warning sound");
}

// ============================================================================
// Custom tone API
// ============================================================================

void SoundManager::playTone(uint16_t frequency, uint16_t durationMs,
                            uint8_t amplitude, uint16_t fadeInMs,
                            uint16_t fadeOutMs) noexcept {
    clearQueue();
    m_loopEnabled = false;
    enqueueTone(frequency, durationMs, amplitude, fadeInMs, fadeOutMs);
    beginHardwarePlayback();
}

void SoundManager::playSequence(std::initializer_list<SoundTone> tones) noexcept {
    clearQueue();
    m_loopEnabled = false;

    for (const auto& t : tones) {
        enqueueTone(t.frequency, t.durationMs, t.amplitude,
                    t.fadeInMs, t.fadeOutMs);
        if (m_queueCount >= kMaxQueuedTones) {
            break;
        }
    }

    if (m_queueCount > 0) {
        beginHardwarePlayback();
    }
}

// ============================================================================
// Private – tone queue
// ============================================================================

void SoundManager::enqueueTone(uint16_t freq, uint16_t dur, uint8_t amp,
                               uint16_t fadeIn, uint16_t fadeOut) noexcept {
    if (m_queueCount >= kMaxQueuedTones) {
        m_queue[m_queueHead] = SoundTone(freq, dur, amp, fadeIn, fadeOut);
        m_queueHead = (m_queueHead + 1) % kMaxQueuedTones;
        m_queueTail = (m_queueTail + 1) % kMaxQueuedTones;
        return;
    }

    m_queue[m_queueTail] = SoundTone(freq, dur, amp, fadeIn, fadeOut);
    m_queueTail = (m_queueTail + 1) % kMaxQueuedTones;
    ++m_queueCount;
}

void SoundManager::clearQueue() noexcept {
    m_queueHead = 0;
    m_queueTail = 0;
    m_queueCount = 0;
    m_activeTone = SoundTone();
    m_activeSampleOffset = 0;
    m_activeTotalSamples = 0;
}

void SoundManager::advanceToNextTone() noexcept {
    if (m_queueCount == 0) {
        m_activeTone = SoundTone();
        m_activeSampleOffset = 0;
        m_activeTotalSamples = 0;
        return;
    }

    m_activeTone = m_queue[m_queueHead];
    m_queueHead = (m_queueHead + 1) % kMaxQueuedTones;
    --m_queueCount;
    m_activeSampleOffset = 0;
    m_activeTotalSamples = (static_cast<uint32_t>(m_activeTone.durationMs) *
                            kSampleRate) / 1000U;
}

// ============================================================================
// Private – sample generation
// ============================================================================

void SoundManager::generateChunk() noexcept {
    if (m_activeTone.durationMs == 0 && m_queueCount > 0) {
        advanceToNextTone();
    }

    if (m_activeTone.durationMs == 0) {
        memset(m_sampleBuffer, 0, kChunkBytes);
        return;
    }

    const size_t written = renderTone(m_sampleBuffer, kChunkSamples,
                                      m_activeTone, m_activeSampleOffset);
    m_activeSampleOffset += static_cast<uint32_t>(written);

    if (written < kChunkSamples) {
        const size_t remaining = kChunkSamples - written;
        memset(m_sampleBuffer + written, 0, remaining * sizeof(int16_t));
        m_activeSampleOffset = m_activeTotalSamples;
    }
}

size_t SoundManager::renderTone(int16_t* buffer, size_t count,
                                const SoundTone& tone, uint32_t offset) const noexcept {
    if (tone.durationMs == 0 || count == 0) {
        return 0;
    }

    const uint32_t totalSamples = (static_cast<uint32_t>(tone.durationMs) *
                                   kSampleRate) / 1000U;
    if (offset >= totalSamples) {
        return 0;
    }

    const uint32_t fadeInSmp  = (static_cast<uint32_t>(tone.fadeInMs) *
                                 kSampleRate) / 1000U;
    const uint32_t fadeOutSmp = (static_cast<uint32_t>(tone.fadeOutMs) *
                                 kSampleRate) / 1000U;
    const float    ampScaled  = static_cast<float>(tone.amplitude) * kAmplitudeScale;

    size_t written = 0;
    for (size_t i = 0; i < count && offset < totalSamples; ++i, ++offset) {
        const float sine = sinf(kTwoPi * tone.frequency *
                                static_cast<float>(offset) /
                                static_cast<float>(kSampleRate));
        const float env  = amplitudeEnvelope(offset, totalSamples,
                                             fadeInSmp, fadeOutSmp);
        buffer[i] = static_cast<int16_t>(sine * env * ampScaled);
        ++written;
    }

    return written;
}

// ============================================================================
// Private – AudioManager bridge
// ============================================================================

void SoundManager::beginHardwarePlayback() noexcept {
    if (!m_initialized || !audioManager.isInitialized()) {
        return;
    }

    if (m_queueCount == 0) {
        return;
    }

    advanceToNextTone();
    m_state = PlayState::ACTIVE;

    if (!audioManager.isPlaying()) {
        audioManager.startPlayback();
    }
}

void SoundManager::deliverChunk() noexcept {
    if (!audioManager.isPlaying()) {
        return;
    }

    size_t bytesWritten = 0;
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(m_sampleBuffer);

    if (!audioManager.play(raw, kChunkBytes, bytesWritten)) {
        LOG_WARNING(kTag, "AudioManager::play returned false");
    }
}

void SoundManager::flushPlayback() noexcept {
    if (!audioManager.isInitialized()) {
        return;
    }
    audioManager.flushPlayback();
    audioManager.stopPlayback();
}