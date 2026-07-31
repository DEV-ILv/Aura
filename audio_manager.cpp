#include "audio_manager.h"
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
      m_queueProcessing(false)
{
    m_speechQueue.reserve(kMaxSpeechQueue);
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
        readMicrophone(nullptr, 0, m_bufferSize);
    }

    if (m_state == AudioState::PLAYING && m_playing)
    {
        writeSpeaker(nullptr, 0, m_bufferSize);
    }
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

bool AudioManager::isInitialized() const noexcept
{
    return m_initialized;
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
            .bclk = static_cast<gpio_num_t>(MIC_BCLK_PIN),
            .ws = static_cast<gpio_num_t>(MIC_WS_PIN),
            .dout = static_cast<gpio_num_t>(-1),
            .din = static_cast<gpio_num_t>(MIC_DATA_PIN),
        },
    };

    result = i2s_channel_init_std_mode(m_microphoneHandle, &std_cfg);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Microphone I2S std mode init failed: %d", static_cast<int>(result));
        i2s_del_channel(m_microphoneHandle);
        m_microphoneHandle = nullptr;
        return false;
    }

    result = i2s_channel_enable(m_microphoneHandle);
    if (result != ESP_OK)
    {
        Logger::error(kLogCategory, "Microphone I2S enable failed: %d", static_cast<int>(result));
        i2s_del_channel(m_microphoneHandle);
        m_microphoneHandle = nullptr;
        return false;
    }

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

    if (result != ESP_OK || frameBytesRead == 0)
    {
        return true;  // No data available, but not an error
    }

    int16_t* samples = reinterpret_cast<int16_t*>(tempBuffer);
    size_t sampleCount = frameBytesRead / sizeof(int16_t);

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
    if (buffer && bufferSize > 0)
    {
        size_t copySize = (frameBytesRead < bufferSize) ? frameBytesRead : bufferSize;
        std::memcpy(buffer, tempBuffer, copySize);
        bytesRead = copySize;
    }
    else
    {
        // Store in internal buffer for later retrieval
        if (frameBytesRead <= m_bufferSize)
        {
            std::memcpy(m_recordBuffer, tempBuffer, frameBytesRead);
        }
        bytesRead = frameBytesRead;
    }

    return true;
}

bool AudioManager::writeSpeaker(const uint8_t* buffer, size_t bufferSize, size_t& bytesWritten) noexcept
{
    bytesWritten = 0;

    if (!m_initialized || !m_playbackBuffer)
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

    while (collected < samples)
    {
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