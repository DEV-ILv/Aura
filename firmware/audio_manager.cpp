#include "audio_manager.h"
#include "event_bus.h"
#include <driver/i2s_std.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <algorithm>

/// Global AudioManager instance
AudioManager audioManager;

// ============================================================================
// Anonymous Namespace - Internal Helpers
// ============================================================================

namespace {

constexpr const char* kLogCategory = "AudioManager";
constexpr size_t kI2SFrameSize = 512U;
constexpr unsigned long kFlushTimeoutMs = 1000UL;
// Wall-clock bound for noise-floor calibration: a dead mic must not spin the
// collection loop forever (previously it looped on delay(1) until samples were
// gathered). 5 s is generous for a healthy mic to deliver the requested window.
constexpr unsigned long kCalibrateTimeoutMs = 5000UL;

/**
 * @brief Clamp a value to a range
 */
template<typename T>
constexpr T clamp(T value, T minimum, T maximum) noexcept
{
    return (value < minimum) ? minimum : (value > maximum) ? maximum : value;
}

/**
 * @brief Apply gain to a 16-bit PCM sample with clipping
 */
int16_t applySampleGain(int16_t sample, float gainFactor) noexcept
{
    int32_t scaled = static_cast<int32_t>(sample) * gainFactor;
    return static_cast<int16_t>(clamp(scaled, int32_t(-32768), int32_t(32767)));
}

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

AudioManager::AudioManager() noexcept
    : m_state(AudioState::IDLE),
      m_initialized(false),
      m_recording(false),
      m_playing(false),
      m_muted(false),
      m_volume(DEFAULT_VOLUME),
      m_microphoneGain(DEFAULT_MIC_GAIN),
      m_sampleRate(DEFAULT_SAMPLE_RATE),
      m_dmaBufferSize(DEFAULT_DMA_BUFFER_SIZE),
      m_bufferSize(DEFAULT_DMA_BUFFER_SIZE * 4),
      m_recordBuffer(nullptr),
      m_playbackBuffer(nullptr),
      m_microphoneHandle(nullptr),
      m_speakerHandle(nullptr),
      m_lastUpdateTime(0),
      m_i2sInitialized(false),
      m_lastAudioEnergy(0.0f),
      m_lastAudioPeak(0.0f),
      m_noiseFloor(50),
      m_noiseThreshold(WAKE_WORD_NOISE_THRESHOLD_DEFAULT),
      m_voiceActive(false),
      m_vadMonitoring(false),
      m_voiceDebounce(0),
      m_lastVadSampleTime(0),
      m_queueProcessing(false)
{
}

AudioManager::~AudioManager() noexcept
{
    stopRecording();
    stopPlayback();

    releaseI2S();

    if (m_recordBuffer)
    {
        heap_caps_free(m_recordBuffer);
        m_recordBuffer = nullptr;
    }

    if (m_playbackBuffer)
    {
        heap_caps_free(m_playbackBuffer);
        m_playbackBuffer = nullptr;
    }
}

// ============================================================================
// Public API - Lifecycle
// ============================================================================

bool AudioManager::initialize() noexcept
{
    if (m_initialized)
    {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    Logger::info(kLogCategory, "Initializing audio manager");

    // Allocate recording buffer (try PSRAM first, then internal)
    m_recordBuffer = static_cast<uint8_t*>(heap_caps_malloc(
        m_bufferSize,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!m_recordBuffer)
    {
        Logger::debug(kLogCategory, "PSRAM not available, using internal RAM");
        m_recordBuffer = static_cast<uint8_t*>(heap_caps_malloc(
            m_bufferSize,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    if (!m_recordBuffer)
    {
        Logger::error(kLogCategory, "Recording buffer allocation failed");
        return false;
    }

    // Allocate playback buffer (try PSRAM first, then internal)
    m_playbackBuffer = static_cast<uint8_t*>(heap_caps_malloc(
        m_bufferSize,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!m_playbackBuffer)
    {
        Logger::debug(kLogCategory, "PSRAM not available, using internal RAM");
        m_playbackBuffer = static_cast<uint8_t*>(heap_caps_malloc(
            m_bufferSize,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    if (!m_playbackBuffer)
    {
        Logger::error(kLogCategory, "Playback buffer allocation failed");
        heap_caps_free(m_recordBuffer);
        m_recordBuffer = nullptr;
        return false;
    }

    clearBuffers();

    // Configure I2S
    if (!initializeI2S())
{
    if (m_recordBuffer)
    {
        heap_caps_free(m_recordBuffer);
        m_recordBuffer = nullptr;
    }

    if (m_playbackBuffer)
    {
        heap_caps_free(m_playbackBuffer);
        m_playbackBuffer = nullptr;
    }

    return false;
}
        

    m_initialized = true;
    m_state = AudioState::IDLE;

    Logger::info(kLogCategory, "Audio manager initialized (16kHz, 16-bit, Mono)");

    // Boot-time microphone self-test: logs live RMS/peak for hardware verification
    {
        Logger::info(kLogCategory, "Mic: First DMA read...");
        uint8_t raw[kI2SFrameSize];
        size_t n = 0;
        if (m_microphoneHandle != nullptr &&
            i2s_channel_read(m_microphoneHandle, raw, sizeof(raw), &n, pdMS_TO_TICKS(50)) == ESP_OK &&
            n > 0)
        {
            const int32_t* w = reinterpret_cast<const int32_t*>(raw);
            const size_t cnt = n / sizeof(int32_t);
            const size_t show = cnt < 8 ? cnt : 8;
            for (size_t i = 0; i < show; ++i)
            {
                Logger::info(kLogCategory, "Mic raw[%u] = 0x%08X (%ld)",
                    static_cast<unsigned>(i),
                    static_cast<unsigned>(static_cast<uint32_t>(w[i])),
                    static_cast<long>(w[i]));
            }
        }
        else
        {
            Logger::warning(kLogCategory, "Mic self-test: no I2S samples captured");
        }

        for (int round = 0; round < 10; ++round)
        {
            if (round > 0) delay(300);
            float micRms = 0.0f;
            float micPeak = 0.0f;
            if (sampleMicLevel(micRms, micPeak))
            {
                Logger::info(kLogCategory, "Mic self-test[%d]: RMS=%u peak=%u",
                    round,
                    static_cast<unsigned>(micRms), static_cast<unsigned>(micPeak));
            }
        }

        // Full windowed diagnostic: RMS, peak, min/max, valid/zero/clip %, rate,
        // I2S read errors. This is the "is the mic producing audio at all" test.
        MicDiagnostic diag;
        if (runMicDiagnostics(diag, 500U))
        {
            Logger::info(kLogCategory,
                "Mic diag: %u ms, %u frames, %u valid, read_calls=%u read_errs=%u",
                diag.windowMs, diag.frames, diag.validSamples, diag.readCalls, diag.readErrors);
            Logger::info(kLogCategory,
                "Mic diag: RMS=%u peak=%u min=%ld max=%ld",
                static_cast<unsigned>(diag.rms), static_cast<unsigned>(diag.peak),
                static_cast<long>(diag.minSample), static_cast<long>(diag.maxSample));
            Logger::info(kLogCategory,
                "Mic diag: zero=%.1f%% clip=%.1f%% rate=%u Hz",
                static_cast<double>(diag.zeroPercent),
                static_cast<double>(diag.clipPercent),
                diag.detectedSampleRate);
            Logger::info(kLogCategory,
                "Mic diag words: 0xFFFFFFFF=%u zero=0x00000000=%u other=%u",
                static_cast<unsigned>(diag.onesWordCount),
                static_cast<unsigned>(diag.zeroWordCount),
                static_cast<unsigned>(diag.otherWordCount));
        }
        else
        {
            Logger::warning(kLogCategory, "Mic diag: no frames captured (read_errs=%u)",
                diag.readErrors);
        }
    }

    return true;
}

void AudioManager::run() noexcept
{
    update();
}

void AudioManager::update() noexcept
{
    if (!m_initialized)
    {
        return;
    }

    m_lastUpdateTime = millis();

    if (m_state == AudioState::RECORDING && m_recording)
    {
        // An external consumer (STT) owns the mic DMA stream: do not drain here
        // or we would steal the samples it is capturing. When no consumer is
        // active, drain to keep the DMA flowing and the idle VAD latch relaxed.
        m_voiceActive = false;
        if (!m_captureConsumerActive)
        {
            size_t dummy = 0;
            readMicrophone(nullptr, 0, dummy);
        }
        return;
    }

    if (m_state == AudioState::PLAYING && m_playing)
    {
        size_t dummy = 0;
        writeSpeaker(nullptr, 0, dummy);
        // Keep a lightweight non-blocking mic probe running during playback so
        // barge-in can detect that the user started speaking. readMicrophone()
        // is non-blocking (0 ms tick) and updates m_lastAudioEnergy.
        readMicrophone(nullptr, 0, dummy);
        return;
    }

    // Otherwise the mic is idle: keep an always-on speech monitor so the
    // LED/UI can react to voice before any STT/AI pipeline starts.
    runVoiceActivityDetection();
}

// ============================================================================
// Recording API
// ============================================================================

bool AudioManager::startRecording() noexcept
{
    if (!m_initialized)
    {
        Logger::error(kLogCategory, "Cannot record: not initialized");
        return false;
    }

    if (m_state == AudioState::RECORDING)
    {
        return true;
    }

    if (m_state == AudioState::PLAYING)
    {
        Logger::warning(kLogCategory, "Cannot record while playing");
        return false;
    }

    m_recording = true;
    m_state = AudioState::RECORDING;
    clearBuffers();

    Logger::info(kLogCategory, "Recording started");

    return true;
}

bool AudioManager::stopRecording() noexcept
{
    if (m_state != AudioState::RECORDING)
    {
        return true;
    }

    m_recording = false;
    m_state = AudioState::IDLE;

    Logger::info(kLogCategory, "Recording stopped");

    return true;
}

bool AudioManager::record(uint8_t* buffer, size_t bufferSize, size_t& bytesRead) noexcept
{
    bytesRead = 0;

    if (!m_initialized || !m_recording || !buffer || bufferSize == 0)
    {
        return false;
    }

    if (m_state != AudioState::RECORDING)
    {
        return false;
    }

    return readMicrophone(buffer, bufferSize, bytesRead);
}

// ============================================================================
// Playback API
// ============================================================================

bool AudioManager::startPlayback() noexcept
{
    if (!m_initialized)
    {
        Logger::error(kLogCategory, "Cannot play: not initialized");
        return false;
    }

    if (m_state == AudioState::PLAYING)
    {
        return true;
    }

    if (m_state == AudioState::RECORDING)
    {
        Logger::warning(kLogCategory, "Cannot play while recording");
        return false;
    }

    m_playing = true;
    m_state = AudioState::PLAYING;
    clearBuffers();

    Logger::info(kLogCategory, "Playback started");

    return true;
}

bool AudioManager::stopPlayback() noexcept
{
    if (m_state != AudioState::PLAYING)
    {
        return true;
    }

    m_playing = false;
    m_state = AudioState::IDLE;

    Logger::info(kLogCategory, "Playback stopped");

    return true;
}

bool AudioManager::play(const uint8_t* buffer, size_t bufferSize, size_t& bytesWritten) noexcept
{
    bytesWritten = 0;

    if (!m_initialized || !m_playing || !buffer || bufferSize == 0)
    {
        return false;
    }

    if (m_state != AudioState::PLAYING)
    {
        return false;
    }

    return writeSpeaker(buffer, bufferSize, bytesWritten);
}

// ============================================================================
// Volume & Gain Control
// ============================================================================

bool AudioManager::setVolume(uint8_t volume) noexcept
{
    m_volume = clamp(volume, uint8_t(0), MAX_VOLUME);
    updateVolume();
    return true;
}

bool AudioManager::setMicrophoneGain(uint8_t gain) noexcept
{
    m_microphoneGain = clamp(gain, uint8_t(0), MAX_MIC_GAIN);
    updateGain();
    return true;
}

bool AudioManager::mute() noexcept
{
    m_muted = true;
    Logger::debug(kLogCategory, "Audio muted");
    return true;
}

bool AudioManager::unmute() noexcept
{
    m_muted = false;
    Logger::debug(kLogCategory, "Audio unmuted");
    return true;
}

bool AudioManager::isMuted() const noexcept
{
    return m_muted;
}

// ============================================================================
// State Queries
// ============================================================================

bool AudioManager::isRecording() const noexcept
{
    return m_state == AudioState::RECORDING && m_recording;
}

bool AudioManager::isPlaying() const noexcept
{
    return m_state == AudioState::PLAYING && m_playing;
}

bool AudioManager::hasSpeaker() const noexcept
{
    return m_speakerHandle != nullptr;
}

bool AudioManager::isInitialized() const noexcept
{
    return m_initialized;
}

uint32_t AudioManager::getMicReadErrorCount() const noexcept
{
    return m_micReadErrorCount;
}

void AudioManager::resetMicReadErrorCount() noexcept
{
    m_micReadErrorCount = 0;
    m_micZeroReadStreak = 0;
    m_micLastDataMs = millis();
}

uint8_t AudioManager::getVolume() const noexcept
{
    return m_volume;
}

uint8_t AudioManager::getMicrophoneGain() const noexcept
{
    return m_microphoneGain;
}

uint32_t AudioManager::getSampleRate() const noexcept
{
    return m_sampleRate;
}

size_t AudioManager::getBufferSize() const noexcept
{
    return m_bufferSize;
}

AudioState AudioManager::getState() const noexcept
{
    return m_state;
}

// ============================================================================
// Buffer Management
// ============================================================================

bool AudioManager::flushPlayback() noexcept
{
    if (m_state != AudioState::PLAYING)
    {
        return true;
    }

    // Non-blocking flush: clear buffers immediately instead of waiting
    clearBuffers();
    m_playing = false;
    m_state = AudioState::IDLE;

    Logger::debug(kLogCategory, "Playback buffer flushed");
    return true;
}

// ============================================================================
// Private: Initialization
// ============================================================================

bool AudioManager::configureInput() noexcept
{
    Logger::debug(kLogCategory, "Configuring microphone input");
    Logger::info(kLogCategory, "Mic: Installing I2S driver (port 0, RX, master)...");

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DEFAULT_DMA_BUFFER_COUNT,
        .dma_frame_num = m_dmaBufferSize,
        .auto_clear = true,
    };

    esp_err_t result = i2s_new_channel(&chan_cfg, nullptr, &m_microphoneHandle);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Microphone I2S channel alloc failed: %d", static_cast<int>(result));
        return false;
    }
    Logger::info(kLogCategory, "Mic: Driver OK");

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = m_sampleRate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
            .bit_shift = true,
            .msb_right = true,
        },
        .gpio_cfg = {
            .mclk = static_cast<gpio_num_t>(I2S_GPIO_UNUSED),
            .bclk = static_cast<gpio_num_t>(MIC_BCLK_PIN),
            .ws = static_cast<gpio_num_t>(MIC_WS_PIN),
            .dout = static_cast<gpio_num_t>(-1),
            .din = static_cast<gpio_num_t>(MIC_DATA_PIN),
        },
    };

    Logger::info(kLogCategory, "Mic: Setting pins BCLK=%d LRCLK=%d DATA=%d",
        static_cast<int>(MIC_BCLK_PIN), static_cast<int>(MIC_WS_PIN), static_cast<int>(MIC_DATA_PIN));
    result = i2s_channel_init_std_mode(m_microphoneHandle, &std_cfg);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Microphone I2S std mode init failed: %d", static_cast<int>(result));
        i2s_del_channel(m_microphoneHandle);
        m_microphoneHandle = nullptr;
        return false;
    }
    Logger::info(kLogCategory, "Mic: Pins OK (std mode, 32-bit, Philips, left)");

    Logger::info(kLogCategory, "Mic: Starting peripheral...");
    result = i2s_channel_enable(m_microphoneHandle);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Microphone I2S enable failed: %d", static_cast<int>(result));
        i2s_del_channel(m_microphoneHandle);
        m_microphoneHandle = nullptr;
        return false;
    }
    Logger::info(kLogCategory, "Mic: Running (DMA %d x %d, %u Hz)",
        static_cast<int>(DEFAULT_DMA_BUFFER_COUNT),
        static_cast<unsigned>(m_dmaBufferSize),
        static_cast<unsigned>(m_sampleRate));

    Logger::debug(kLogCategory, "Microphone configured (I2S port %d)", static_cast<int>(I2S_NUM_0));
    return true;
}

bool AudioManager::configureOutput() noexcept
{
    Logger::debug(kLogCategory, "Configuring speaker output");

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DEFAULT_DMA_BUFFER_COUNT,
        .dma_frame_num = m_dmaBufferSize,
        .auto_clear = true,
    };

    esp_err_t result = i2s_new_channel(&chan_cfg, &m_speakerHandle, nullptr);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Speaker I2S channel alloc failed: %d", static_cast<int>(result));
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = m_sampleRate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .msb_right = false,
        },
        .gpio_cfg = {
            .mclk = static_cast<gpio_num_t>(I2S_GPIO_UNUSED),
            .bclk = static_cast<gpio_num_t>(SPK_BCLK_PIN),
            .ws = static_cast<gpio_num_t>(SPK_LRC_PIN),
            .dout = static_cast<gpio_num_t>(SPK_DATA_PIN),
            .din = static_cast<gpio_num_t>(-1),
        },
    };

    result = i2s_channel_init_std_mode(m_speakerHandle, &std_cfg);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Speaker I2S std mode init failed: %d", static_cast<int>(result));
        i2s_del_channel(m_speakerHandle);
        m_speakerHandle = nullptr;
        return false;
    }

    result = i2s_channel_enable(m_speakerHandle);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Speaker I2S enable failed: %d", static_cast<int>(result));
        i2s_del_channel(m_speakerHandle);
        m_speakerHandle = nullptr;
        return false;
    }

    Logger::debug(kLogCategory, "Speaker configured (I2S port %d)", static_cast<int>(I2S_NUM_1));
    return true;
}

bool AudioManager::initializeI2S() noexcept
{
    Logger::debug(kLogCategory, "Initializing I2S peripherals");

    if (!configureInput())
    {
        Logger::error(kLogCategory, "Failed to configure microphone");
        return false;
    }

#if AURA_HW_SPEAKER_PRESENT
    if (!configureOutput())
    {
        Logger::error(kLogCategory, "Failed to configure speaker");
        if (m_microphoneHandle != nullptr)
        {
            i2s_channel_disable(m_microphoneHandle);
            i2s_del_channel(m_microphoneHandle);
            m_microphoneHandle = nullptr;
        }
        return false;
    }
#else
    Logger::info(kLogCategory, "Speaker output skipped (not connected)");
#endif

    m_i2sInitialized = true;
    Logger::debug(kLogCategory, "I2S initialized");

    return true;
}

bool AudioManager::releaseI2S() noexcept
{
    if (!m_i2sInitialized)
    {
        return true;
    }

    if (m_microphoneHandle != nullptr)
    {
        i2s_channel_disable(m_microphoneHandle);
        i2s_del_channel(m_microphoneHandle);
        m_microphoneHandle = nullptr;
    }

    if (m_speakerHandle != nullptr)
    {
        i2s_channel_disable(m_speakerHandle);
        i2s_del_channel(m_speakerHandle);
        m_speakerHandle = nullptr;
    }

    m_i2sInitialized = false;

    Logger::debug(kLogCategory, "I2S released");

    return true;
}

bool AudioManager::recoverMicrophone() noexcept
{
    // Bounded, idempotent microphone recovery. The HealthManager recovery path
    // previously called startRecording() alone, which is a no-op when already
    // RECORDING and can never recreate a failed I2S driver. Recreate both I2S
    // channels from scratch so a dead/broken mic path is actually restored.
    const bool wasRecording = m_recording;
    const bool wasPlaying = m_playing;

    if (m_recording) stopRecording();
    if (m_playing) stopPlayback();

    releaseI2S();
    m_initialized = false;

    if (!initializeI2S())
    {
        Logger::error(kLogCategory, "Mic recovery: I2S re-init failed");
        return false;
    }

    m_initialized = true;
    m_state = AudioState::IDLE;
    m_micLastDataMs = 0;
    m_micZeroReadStreak = 0;
    m_micReadErrorCount = 0;

    Logger::info(kLogCategory, "Mic recovery: I2S recreated");

    if (wasRecording) startRecording();
    if (wasPlaying) startPlayback();

    return true;
}

// ============================================================================
// Private: Buffer Management
// ============================================================================

void AudioManager::clearBuffers() noexcept
{
    if (m_recordBuffer)
    {
        std::memset(m_recordBuffer, 0, m_bufferSize);
    }

    if (m_playbackBuffer)
    {
        std::memset(m_playbackBuffer, 0, m_bufferSize);
    }
}

void AudioManager::resetState() noexcept
{
    m_recording = false;
    m_playing = false;
    m_state = AudioState::IDLE;
    clearBuffers();
}

// ============================================================================
// Private: Audio I/O
// ============================================================================

bool AudioManager::readMicrophone(uint8_t* buffer, size_t bufferSize, size_t& bytesRead) noexcept
{
    bytesRead = 0;

    if (!m_initialized || !m_recordBuffer)
    {
        return false;
    }

    uint8_t tempBuffer[kI2SFrameSize];
    size_t frameBytesRead = 0;

    // Non-blocking read from I2S
    esp_err_t result = i2s_channel_read(
        m_microphoneHandle,
        tempBuffer,
        sizeof(tempBuffer),
        &frameBytesRead,
        0);

    if (result != ESP_OK)
    {
        ++m_micReadErrorCount;
        return true;  // Transient read error; next read may recover
    }
    if (frameBytesRead == 0)
    {
        ++m_micZeroReadStreak;
        return true;  // No data available, but not an error
    }

    // Healthy data received: refresh the stall timestamp and clear transient
    // error/zero-read accumulation so only sustained failures are reported.
    m_micLastDataMs = millis();
    m_micZeroReadStreak = 0;
    m_micReadErrorCount = 0;

    // Convert 32-bit-slot samples (24-bit left-justified from INMP441) to 16-bit
    const int32_t* rawSamples = reinterpret_cast<const int32_t*>(tempBuffer);
    size_t rawSampleCount = frameBytesRead / sizeof(int32_t);
    int16_t converted[kI2SFrameSize / sizeof(int16_t)];
    size_t sampleCount = 0;
    for (size_t i = 0; i < rawSampleCount; ++i)
    {
        converted[sampleCount++] = static_cast<int16_t>(rawSamples[i] >> 16);
    }

    int16_t* samples = converted;

    // Compute energy and peak before gain
    {
        float sumSq = 0.0f;
        float peak = 0.0f;
        for (size_t i = 0; i < sampleCount; ++i)
        {
            float s = static_cast<float>(samples[i]);
            sumSq += s * s;
            float absS = fabsf(s);
            if (absS > peak) peak = absS;
        }
        m_lastAudioEnergy = sqrtf(sumSq / static_cast<float>(sampleCount > 0 ? sampleCount : 1));
        m_lastAudioPeak = peak;
    }

    // Apply microphone gain to frame
    if (m_microphoneGain > 0)
    {
        float gainFactor = 1.0F + (static_cast<float>(m_microphoneGain) / 100.0F);

        for (size_t i = 0; i < sampleCount; ++i)
        {
            samples[i] = applySampleGain(samples[i], gainFactor);
        }
    }

    // Store in application recording buffer (simple buffering)
    const size_t convertedBytes = sampleCount * sizeof(int16_t);
    if (buffer && bufferSize > 0)
    {
        size_t copySize = (convertedBytes < bufferSize) ? convertedBytes : bufferSize;
        std::memcpy(buffer, converted, copySize);
        bytesRead = copySize;
    }
    else
    {
        // Store in internal buffer for later retrieval
        if (convertedBytes <= m_bufferSize)
        {
            std::memcpy(m_recordBuffer, converted, convertedBytes);
        }
        bytesRead = convertedBytes;
    }

    return true;
}

bool AudioManager::writeSpeaker(const uint8_t* buffer, size_t bufferSize, size_t& bytesWritten) noexcept
{
    bytesWritten = 0;

    if (!m_initialized || !m_playbackBuffer || m_speakerHandle == nullptr)
    {
        return false;
    }

    uint8_t tempBuffer[kI2SFrameSize];
    size_t frameSize = 0;

    // Prepare data to write
    if (buffer && bufferSize > 0)
    {
        frameSize = (bufferSize < sizeof(tempBuffer)) ? bufferSize : sizeof(tempBuffer);
        std::memcpy(tempBuffer, buffer, frameSize);
    }
    else
    {
        // Use internal playback buffer
        frameSize = (m_bufferSize < sizeof(tempBuffer)) ? m_bufferSize : sizeof(tempBuffer);
        if (frameSize > 0 && m_playbackBuffer)
        {
            std::memcpy(tempBuffer, m_playbackBuffer, frameSize);
        }
    }

    // Apply volume and mute control
    if (!m_muted)
    {
        if (m_volume < MAX_VOLUME)
        {
            float gainFactor = static_cast<float>(m_volume) / static_cast<float>(MAX_VOLUME);
            int16_t* samples = reinterpret_cast<int16_t*>(tempBuffer);
            size_t sampleCount = frameSize / sizeof(int16_t);

            for (size_t i = 0; i < sampleCount; ++i)
            {
                samples[i] = applySampleGain(samples[i], gainFactor);
            }
        }
    }
    else
    {
        std::memset(tempBuffer, 0, frameSize);
    }

    // Non-blocking write to I2S
    if (frameSize == 0)
    {
        return true;
    }

    size_t frameWritten = 0;
    esp_err_t result = i2s_channel_write(
        m_speakerHandle,
        tempBuffer,
        frameSize,
        &frameWritten,
        0);

    if (result != ESP_OK)
    {
        Logger::warning(kLogCategory, "I2S write failed: %d", static_cast<int>(result));
        return false;
    }

    bytesWritten = frameWritten;
    return true;
}

// ============================================================================
// Private: Gain and Volume Updates
// ============================================================================

void AudioManager::updateGain() noexcept
{
    // Gain is applied during recording in readMicrophone()
    // This is a placeholder for any future gain calibration logic
}

void AudioManager::updateVolume() noexcept
{
    // Volume is applied during playback in writeSpeaker()
    // This is a placeholder for any future volume curve adjustments
}

// ============================================================================
// Wake Word Audio Support
// ============================================================================

float AudioManager::getAudioEnergy() const noexcept
{
    return m_lastAudioEnergy;
}

float AudioManager::getAudioPeak() const noexcept
{
    return m_lastAudioPeak;
}

bool AudioManager::sampleMicLevel(float& energyOut, float& peakOut) noexcept
{
    energyOut = 0.0f;
    peakOut = 0.0f;

    if (!m_initialized || m_microphoneHandle == nullptr)
    {
        return false;
    }

    float sumSq = 0.0f;
    float peak = 0.0f;
    size_t totalSamples = 0;

    // Non-blocking reads only: the DMA ring stays continuously filled on a
    // healthy mic, so each pass returns immediately. If the ring is momentarily
    // empty we return false and the caller retries next interval, keeping the
    // main loop fully responsive (no up-to-200ms blocking).
    uint8_t tempBuffer[kI2SFrameSize];
    for (int pass = 0; pass < 8; ++pass)
    {
        size_t frameBytes = 0;
        const esp_err_t result = i2s_channel_read(
            m_microphoneHandle, tempBuffer, sizeof(tempBuffer), &frameBytes, 0);
        if (result != ESP_OK)
        {
            ++m_micReadErrorCount;
            continue;
        }
        if (frameBytes == 0)
        {
            ++m_micZeroReadStreak;
            continue;
        }

        const int32_t* raw = reinterpret_cast<const int32_t*>(tempBuffer);
        const size_t count = frameBytes / sizeof(int32_t);
        for (size_t i = 0; i < count; ++i)
        {
            const float v = static_cast<float>(static_cast<int16_t>(raw[i] >> 16));
            sumSq += v * v;
            const float absV = fabsf(v);
            if (absV > peak) peak = absV;
        }
        totalSamples += count;
        if (totalSamples >= 512) break;
    }

    if (totalSamples == 0) return false;

    // Confirmed healthy burst: refresh stall timestamp and clear transient
    // error/zero-read accumulation.
    m_micLastDataMs = millis();
    m_micZeroReadStreak = 0;
    m_micReadErrorCount = 0;

    const float rms = sqrtf(sumSq / static_cast<float>(totalSamples));
    m_lastAudioEnergy = rms;
    m_lastAudioPeak = peak;
    energyOut = rms;
    peakOut = peak;
    return true;
}

bool AudioManager::hasMicStall() const noexcept
{
    if (!m_initialized || m_captureConsumerActive)
    {
        return false;
    }

    // Only meaningful when we are the ones reading the mic. While STT owns the
    // stream (capture consumer active) or we are not reading at all, a stale
    // timestamp must not be reported as a stall.
    const bool readingMic = m_vadMonitoring ||
        m_state == AudioState::RECORDING ||
        m_state == AudioState::PLAYING;
    if (!readingMic)
    {
        return false;
    }

    if (m_micZeroReadStreak >= kMicZeroReadFailThreshold)
    {
        return true;
    }

    const unsigned long now = millis();
    return m_micLastDataMs != 0 && (now - m_micLastDataMs) >= kMicNoDataMs;
}

bool AudioManager::runMicDiagnostics(MicDiagnostic& out, uint32_t windowMs) noexcept
{
    out = MicDiagnostic{};
    if (!m_initialized || m_microphoneHandle == nullptr)
    {
        return false;
    }

    const unsigned long startMs = millis();
    uint8_t buf[kI2SFrameSize];
    double sumSq = 0.0;
    int32_t minS = 0;
    int32_t maxS = 0;
    uint32_t zeroCount = 0;
    uint32_t clipCount = 0;
    uint32_t allOnesCount = 0;
    uint32_t otherCount = 0;
    bool first = true;
    uint32_t loggedReads = 0;

    while (millis() - startMs < windowMs)
    {
        size_t n = 0;
        out.readCalls++;
        const esp_err_t result = i2s_channel_read(
            m_microphoneHandle, buf, sizeof(buf), &n, pdMS_TO_TICKS(10));
        if (result != ESP_OK)
        {
            out.readErrors++;
            continue;
        }
        if (n == 0)
        {
            delay(1);
            continue;
        }

        const int32_t* raw = reinterpret_cast<const int32_t*>(buf);
        const size_t count = n / sizeof(int32_t);
        if (loggedReads < 3 && count >= 8)
        {
            Logger::info(kLogCategory, "Mic diag raw[%u] = 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X",
                static_cast<unsigned>(loggedReads),
                static_cast<unsigned>(static_cast<uint32_t>(raw[0])),
                static_cast<unsigned>(static_cast<uint32_t>(raw[1])),
                static_cast<unsigned>(static_cast<uint32_t>(raw[2])),
                static_cast<unsigned>(static_cast<uint32_t>(raw[3])),
                static_cast<unsigned>(static_cast<uint32_t>(raw[4])),
                static_cast<unsigned>(static_cast<uint32_t>(raw[5])),
                static_cast<unsigned>(static_cast<uint32_t>(raw[6])),
                static_cast<unsigned>(static_cast<uint32_t>(raw[7])));
            loggedReads++;
        }
        for (size_t i = 0; i < count; ++i)
        {
            const int32_t s16 = static_cast<int16_t>(raw[i] >> 16);
            if (static_cast<uint32_t>(raw[i]) == 0xFFFFFFFFU) allOnesCount++;
            else if (static_cast<uint32_t>(raw[i]) == 0x00000000U) zeroCount++;
            else otherCount++;
            if (first)
            {
                minS = maxS = s16;
                first = false;
            }
            else
            {
                if (s16 < minS) minS = s16;
                if (s16 > maxS) maxS = s16;
            }
            if (s16 == -32768 || s16 == 32767) clipCount++;
            sumSq += static_cast<double>(s16) * static_cast<double>(s16);
            out.frames++;
        }
    }

    out.windowMs = static_cast<uint32_t>(millis() - startMs);
    out.onesWordCount = allOnesCount;
    out.zeroWordCount = zeroCount;
    out.otherWordCount = otherCount;
    if (out.frames > 0)
    {
        out.validSamples = out.frames - zeroCount;
        out.minSample = minS;
        out.maxSample = maxS;
        out.rms = static_cast<float>(sqrt(sumSq / static_cast<double>(out.frames)));
        out.peak = static_cast<float>(maxS > -minS ? maxS : -minS);
        out.zeroPercent = 100.0f * static_cast<float>(zeroCount) / static_cast<float>(out.frames);
        out.clipPercent = 100.0f * static_cast<float>(clipCount) / static_cast<float>(out.frames);
        if (out.windowMs > 0)
        {
            out.detectedSampleRate = static_cast<uint32_t>(
                (static_cast<uint64_t>(out.frames) * 1000ULL) / out.windowMs);
        }
        m_lastAudioEnergy = out.rms;
        m_lastAudioPeak = out.peak;
        return true;
    }
    return false;
}

void AudioManager::runVoiceActivityDetection() noexcept
{
    if (!m_initialized || m_microphoneHandle == nullptr || !m_vadMonitoring)
    {
        return;
    }

    const unsigned long now = millis();
    const uint32_t interval = m_voiceActive ? kVadActiveIntervalMs : kVadIdleIntervalMs;
    if (now - m_lastVadSampleTime < interval)
    {
        return;
    }
    m_lastVadSampleTime = now;

    float energy = 0.0f;
    float peak = 0.0f;
    if (!sampleMicLevel(energy, peak))
    {
        return;
    }

    const float threshold = static_cast<float>(m_noiseFloor) + static_cast<float>(m_noiseThreshold);
    const bool above = (energy > threshold);

    if (!m_voiceActive)
    {
        if (above)
        {
            m_voiceActive = true;
            m_voiceDebounce = 0;

            // Map RMS energy (0..32768) to a 0..255 voice level for the VU.
            const uint8_t level = static_cast<uint8_t>(
                255U * (energy / 16384.0F));
            if (eventBus.isInitialized())
            {
                eventBus.publish(EventType::VOICE_DETECTED, "AudioManager",
                    "{\"energy\":" + String(energy, 1)
                    + ",\"level\":" + String(level) + "}");
            }
        }
        return;
    }

    // Voice is latched on: keep sampling to auto-release after a short
    // hysteresis so natural speech pauses do not flicker the indicator.
    if (!above)
    {
        if (++m_voiceDebounce >= kVadReleaseFrames)
        {
            m_voiceActive = false;
            m_voiceDebounce = 0;
            if (eventBus.isInitialized())
            {
                eventBus.publish(EventType::VOICE_ENDED, "AudioManager", "");
            }
        }
    }
    else
    {
        m_voiceDebounce = 0;
    }
}

bool AudioManager::isVoiceActive() const noexcept
{
    return m_voiceActive;
}

void AudioManager::setVoiceActivityMonitoring(bool enabled) noexcept
{
    if (m_vadMonitoring == enabled) return;
    m_vadMonitoring = enabled;
    if (!enabled) {
        m_voiceActive = false;   // reset the latch so the LED relaxes immediately
        m_lastVadSampleTime = 0;
    }
}

bool AudioManager::isVoiceActivityMonitoringEnabled() const noexcept
{
    return m_vadMonitoring;
}

void AudioManager::setCaptureConsumerActive(bool active) noexcept
{
    m_captureConsumerActive = active;
}

uint16_t AudioManager::getNoiseFloor() const noexcept
{
    return m_noiseFloor;
}

bool AudioManager::calibrateNoiseFloor(size_t samples) noexcept
{
    if (!m_initialized || !m_recording) return false;

    float sumSq = 0.0f;
    size_t collected = 0;
    uint8_t buf[512];
    size_t readBytes = 0;
    const unsigned long startMs = millis();

    while (collected < samples)
    {
        // Bounded: a dead/unresponsive mic must not hold the loop task forever.
        if ((millis() - startMs) >= kCalibrateTimeoutMs)
        {
            Logger::warning(kLogCategory, "Noise floor calibration timed out (collected %u/%u)",
                static_cast<unsigned>(collected), static_cast<unsigned>(samples));
            return false;
        }

        if (!readMicrophone(buf, sizeof(buf), readBytes) || readBytes == 0)
        {
            delay(1);
            continue;
        }
        int16_t* s = reinterpret_cast<int16_t*>(buf);
        size_t cnt = readBytes / sizeof(int16_t);
        for (size_t i = 0; i < cnt && collected < samples; ++i, ++collected)
        {
            float v = static_cast<float>(s[i]);
            sumSq += v * v;
        }
    }

    if (collected == 0) return false;
    m_noiseFloor = static_cast<uint16_t>(sqrtf(sumSq / static_cast<float>(collected)));
    Logger::info(kLogCategory, "Noise floor calibrated: %u", m_noiseFloor);
    return true;
}

void AudioManager::updateAdaptiveNoiseFloor(uint16_t currentRms, float alpha) noexcept
{
    if (currentRms == 0) return;
    float newEstimate = alpha * static_cast<float>(currentRms) + (1.0f - alpha) * static_cast<float>(m_noiseFloor);
    m_noiseFloor = static_cast<uint16_t>(newEstimate + 0.5f);
}

void AudioManager::setNoiseThreshold(uint16_t threshold) noexcept
{
    m_noiseThreshold = threshold;
}

uint16_t AudioManager::getNoiseThreshold() const noexcept
{
    return m_noiseThreshold;
}

// ============================================================================
// Audio Queue & Speech Cancellation
// ============================================================================

void AudioManager::queueSpeech(const String& text) noexcept {
    if (text.isEmpty()) return;
    if (m_speechQueue.size() >= kMaxSpeechQueue) {
        m_speechQueue.erase(m_speechQueue.begin());
    }
    m_speechQueue.push_back(text);
    LOG_DEBUG("AudioManager", "Speech queued: %s (queue: %u)",
              text.c_str(), m_speechQueue.size());
}

bool AudioManager::hasQueuedSpeech() const noexcept {
    return !m_speechQueue.empty();
}

void AudioManager::clearSpeechQueue() noexcept {
    m_speechQueue.clear();
}

void AudioManager::cancelPlayback() noexcept {
    if (isPlaying()) {
        stopPlayback();
        clearBuffers();
        LOG_INFO("AudioManager", "Playback cancelled");
    }
    // Clear pending TTS
    while (!m_speechQueue.empty()) {
        if (m_speechQueue.size() > 1) {
            m_speechQueue.erase(m_speechQueue.begin());
        } else {
            break;
        }
    }
}

void AudioManager::playEarcon(uint8_t earcon) noexcept {
    // Earcons are short duration silent markers (can be expanded with actual tone data)
    // For now, skip if currently playing to avoid interrupt
    if (isPlaying()) return;

    // Simple earcon: short high-frequency beep (placeholder)
    // In production, this would play a pre-recorded WAV chunk
    constexpr size_t earconSamples = 800; // 50ms at 16kHz
    size_t earconBytes = earconSamples * sizeof(int16_t);
    if (earconBytes > 2048) earconBytes = 2048;

    int16_t earconBuf[1024];
    size_t sampleCount = earconBytes / sizeof(int16_t);
    for (size_t i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(DEFAULT_SAMPLE_RATE);
        float freq = 800.0f + static_cast<float>(earcon) * 200.0f;
        float envelope = 1.0f - (static_cast<float>(i) / static_cast<float>(sampleCount));
        earconBuf[i] = static_cast<int16_t>(8192.0f * sin(2.0f * 3.14159f * freq * t) * envelope);
    }

    if (startPlayback()) {
        size_t written = 0;
        play(reinterpret_cast<const uint8_t*>(earconBuf), earconBytes, written);
    }
}

void AudioManager::processSpeechQueue() noexcept {
    if (m_queueProcessing || isPlaying() || m_speechQueue.empty()) return;

    m_queueProcessing = true;
    String nextText = m_speechQueue.front();
    m_speechQueue.erase(m_speechQueue.begin());

    // Delegate to TTS system for actual synthesis
    // The speech queue holds text that needs to be synthesized
    // Actual synthesis happens via the TTS manager
    // This method just manages the queue lifecycle
    LOG_DEBUG("AudioManager", "Processing queued speech: %s", nextText.c_str());
    m_queueProcessing = false;
}