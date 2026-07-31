#pragma once
#ifndef AURA_SOUND_MANAGER_H
#define AURA_SOUND_MANAGER_H

#include <Arduino.h>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <initializer_list>
#include "audio_manager.h"
#include "audio_asset_manager.h"
#include "config.h"
#include "logger.h"

/**
 * @struct SoundTone
 * @brief Describes a single sinusoidal tone segment.
 */
struct SoundTone {
    uint16_t frequency;       ///< Tone frequency in Hz (0 = silence/rest)
    uint16_t durationMs;      ///< Segment duration in milliseconds
    uint8_t  amplitude;       ///< Amplitude 0-255 (255 = full scale)
    uint16_t fadeInMs;        ///< Fade-in time in milliseconds
    uint16_t fadeOutMs;       ///< Fade-out time in milliseconds

    constexpr SoundTone() noexcept
        : frequency(0), durationMs(0), amplitude(0), fadeInMs(0), fadeOutMs(0) {}

    constexpr SoundTone(uint16_t freq, uint16_t dur, uint8_t amp = 128,
                        uint16_t fadeIn = 5, uint16_t fadeOut = 5) noexcept
        : frequency(freq), durationMs(dur), amplitude(amp),
          fadeInMs(fadeIn), fadeOutMs(fadeOut) {}
};

/**
 * @class SoundManager
 * @brief Single authority for UI sound effects on the AURA AI Desktop Assistant.
 *
 * Generates PCM16 mono 16 kHz audio mathematically (sine-wave synthesis)
 * and feeds samples to the existing AudioManager for I2S playback.
 *
 * SoundManager is NOT responsible for:
 *   - Speech synthesis (TextToSpeech)
 *   - Music or voice playback
 *   - Streaming audio
 *   - Audio recording
 *
 * All sounds are generated at runtime. No external WAV files, no SD card access.
 *
 * Thread-safe for main-loop or FreeRTOS task usage. Fully non-blocking.
 */
class SoundManager {
public:
    /** @brief Constructor */
    SoundManager() noexcept;

    /** @brief Destructor */
    ~SoundManager() noexcept;

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;
    SoundManager(SoundManager&&) = delete;
    SoundManager& operator=(SoundManager&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialise the sound manager.
     * @return true when AudioManager is available and ready.
     * @note Safe to call multiple times.
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Scheduler-compatible update alias.
     */
    void run() noexcept;

    /**
     * @brief Generate and deliver the next sample chunk to AudioManager.
     * @note Call regularly from loop() or a dedicated FreeRTOS task.
     */
    void update() noexcept;

    // ========================================================================
    // Playback control
    // ========================================================================

    /**
     * @brief Immediately stop all playback and flush AudioManager buffers.
     */
    void stop() noexcept;

    /**
     * @brief Check whether a sound is actively playing.
     * @return true if samples are still being generated or delivered.
     */
    [[nodiscard]] bool isPlaying() const noexcept;

    /**
     * @brief Check initialisation status.
     * @return true after a successful call to initialize().
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    // ========================================================================
    // UI sound effects
    // ========================================================================

    /** @brief Ascending 3-note chime on power-up. */
    void playStartup() noexcept;

    /** @brief Descending chime on graceful shutdown. */
    void playShutdown() noexcept;

    /** @brief Single soft chime indicating the system is ready. */
    void playReady() noexcept;

    /** @brief Very short click to confirm the microphone is open. */
    void playListening() noexcept;

    /**
     * @brief Start a gentle repeating pulse while the AI is processing.
     * @note Call stopThinking() to end the loop.
     */
    void playThinking() noexcept;

    /** @brief Stop the repeating thinking pulse. */
    void stopThinking() noexcept;

    /** @brief Two-note positive confirmation. */
    void playConfirmation() noexcept;

    /** @brief Ascending three-note success jingle. */
    void playSuccess() noexcept;

    /** @brief Generic notification double-tone. */
    void playNotification() noexcept;

    /** @brief Gentle repeating reminder tone (loops until stopped). */
    void playReminder() noexcept;

    /** @brief Short positive melody confirming Wi-Fi connection. */
    void playWifiConnected() noexcept;

    /** @brief Negative double beep indicating Wi-Fi error. */
    void playWifiError() noexcept;

    /** @brief Short ascending sweep when an OTA update begins. */
    void playOtaStart() noexcept;

    /** @brief Longer ascending confirmation when OTA finishes. */
    void playOtaFinished() noexcept;

    /** @brief Very short tick for button presses. */
    void playButtonClick() noexcept;

    /** @brief Slightly longer tick for touch events. */
    void playTouch() noexcept;

    /** @brief Low descending tone for errors. */
    void playError() noexcept;

    /** @brief Double warning beep. */
    void playWarning() noexcept;

    // ========================================================================
    // Custom tone API
    // ========================================================================

    /**
     * @brief Play a single tone immediately.
     * @param frequency  Tone frequency in Hz (0 = silence).
     * @param durationMs Duration in milliseconds.
     * @param amplitude  Amplitude 0-255 (default 128).
     * @param fadeInMs   Fade-in time in milliseconds (default 5).
     * @param fadeOutMs  Fade-out time in milliseconds (default 5).
     */
    void playTone(uint16_t frequency, uint16_t durationMs,
                  uint8_t amplitude = kDefaultAmp,
                  uint16_t fadeInMs = kDefaultFadeMs,
                  uint16_t fadeOutMs = kDefaultFadeMs) noexcept;

    /**
     * @brief Play a sequence of tones.
     * @param tones An initializer_list of SoundTone descriptors.
     *
     * @code
     *   soundManager.playSequence({
     *       {523, 150, 128, 10, 10},
     *       {659, 150, 128, 10, 10},
     *       {784, 200, 128, 10, 20}
     *   });
     * @endcode
     */
    void playSequence(std::initializer_list<SoundTone> tones) noexcept;

private:
    // ========================================================================
    // Internal state
    // ========================================================================

    enum class PlayState : uint8_t {
        IDLE,       ///< No playback active
        ACTIVE      ///< Generating and delivering samples
    };

    // ========================================================================
    // Tone queue helpers
    // ========================================================================

    /** @brief Enqueue one tone (ring-buffer behaviour; drops oldest when full). */
    void enqueueTone(uint16_t freq, uint16_t dur, uint8_t amp,
                     uint16_t fadeIn, uint16_t fadeOut) noexcept;

    /** @brief Clear the entire tone queue without stopping hardware. */
    void clearQueue() noexcept;

    /** @brief Pop the next tone from the queue and prepare generation state. */
    void advanceToNextTone() noexcept;

    // ========================================================================
    // Sample generation
    // ========================================================================

    /** @brief Fill m_sampleBuffer with the next chunk of PCM16 samples. */
    void generateChunk() noexcept;

    /**
     * @brief Write a single tone segment into a sample buffer.
     * @param[out] buffer  Destination int16_t array.
     * @param[in]  count   Number of samples to write.
     * @param[in]  tone    The tone descriptor.
     * @param[in]  offset  Sample offset into this tone (0-based).
     * @return The number of samples actually written (may be < count at end).
     */
    size_t renderTone(int16_t* buffer, size_t count,
                      const SoundTone& tone, uint32_t offset) const noexcept;

    // ========================================================================
    // AudioManager bridge
    // ========================================================================

    /** @brief Call audioManager.startPlayback() if not already playing. */
    void beginHardwarePlayback() noexcept;

    /** @brief Deliver m_sampleBuffer to AudioManager. */
    void deliverChunk() noexcept;

    /** @brief Flush AudioManager buffers and stop hardware playback. */
    void flushPlayback() noexcept;

    // ========================================================================
    // Constants
    // ========================================================================

    static constexpr uint16_t kSampleRate      = 16000U;
    static constexpr size_t   kChunkSamples    = 256U;   ///< 16 ms at 16 kHz
    static constexpr size_t   kChunkBytes      = kChunkSamples * sizeof(int16_t);  // 512
    static constexpr uint8_t  kMaxQueuedTones  = 32U;
    static constexpr uint8_t  kDefaultAmp      = 128U;
    static constexpr uint16_t kDefaultFadeMs   = 5U;
    static constexpr float    kTwoPi           = 6.283185307179586f;
    static constexpr float    kAmplitudeScale  = 32767.0f / 255.0f;

    // ========================================================================
    // Member variables
    // ========================================================================

    bool       m_initialized;
    PlayState  m_state;

    // Ring-buffer tone queue
    SoundTone  m_queue[kMaxQueuedTones];
    uint8_t    m_queueHead;
    uint8_t    m_queueTail;
    uint8_t    m_queueCount;

    // Active tone generation state
    SoundTone  m_activeTone;
    uint32_t   m_activeSampleOffset;   ///< Sample position within the active tone
    uint32_t   m_activeTotalSamples;   ///< Total samples for the active tone

    // Loop mode (thinking, reminder)
    bool       m_loopEnabled;
    SoundTone  m_loopTone;

    // Sample scratch buffer (heap-friendly, allocated once)
    int16_t    m_sampleBuffer[kChunkSamples];
};

/**
 * @brief Global sound manager instance.
 */
extern SoundManager soundManager;

#endif // AURA_SOUND_MANAGER_H