#ifndef AURA_AUDIO_MANAGER_H
#define AURA_AUDIO_MANAGER_H

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include "config.h"
#include "logger.h"

/**
 * @enum AudioState
 * @brief Enumeration of audio manager states
 */
enum class AudioState : uint8_t {
  IDLE,         ///< No active audio operation
  RECORDING,    ///< Currently recording from microphone
  PLAYING,      ///< Currently playing to speaker
  ERROR         ///< Error state
};
/**
 * @enum AudioFormat
 * @brief Supported PCM audio formats.
 */
enum class AudioFormat : uint8_t {
    PCM16_MONO,
    PCM16_STEREO
};

// Audio configuration constants
namespace {
  constexpr uint32_t DEFAULT_SAMPLE_RATE = 16000;      ///< 16 kHz sample rate
  constexpr uint8_t DEFAULT_VOLUME = 70;               ///< Default speaker volume (0-100)
  constexpr uint8_t DEFAULT_MIC_GAIN = 60;             ///< Default microphone gain (0-100)
  constexpr size_t DEFAULT_DMA_BUFFER_SIZE = 1024;     ///< Default DMA buffer size in bytes
  constexpr uint8_t DEFAULT_DMA_BUFFER_COUNT = 4;      ///< Number of DMA buffers
  constexpr uint8_t MAX_VOLUME = 100;                  ///< Maximum volume level
  constexpr uint8_t MAX_MIC_GAIN = 100;                ///< Maximum microphone gain
}

/**
 * @class AudioManager
 * @brief Manages all audio hardware for AURA AI Desktop Assistant
 * 
 * This class is the single authority for:
 * - I2S microphone (INMP441) capture
 * - I2S speaker (MAX98357A) playback
 * - Audio buffer management with DMA
 * - Volume and gain control
 * - Audio state management
 * - Sample rate configuration
 * 
 * Non-blocking and ESP32-optimized for production use.
 */
class AudioManager {
public:
  /**
   * @brief Constructor
   */
  AudioManager() noexcept;

  /**
   * @brief Destructor
   */
  ~AudioManager() noexcept;

  // Delete copy semantics
  AudioManager(const AudioManager&) = delete;
  AudioManager& operator=(const AudioManager&) = delete;

  // Delete move semantics
  AudioManager(AudioManager&&) = delete;
  AudioManager& operator=(AudioManager&&) = delete;

  /**
   * @brief Initialize the audio manager
   * @return true if initialization successful, false otherwise
   * @note Should be called once during setup()
   */
  [[nodiscard]] bool initialize() noexcept;

  /**
   * @brief Scheduler-compatible update method
   * @note For compatibility with task schedulers
   */
  void run() noexcept;

  /**
   * @brief Update audio manager state
   * @note Should be called regularly from loop()
   */
  void update() noexcept;

  /**
   * @brief Start recording from microphone
   * @return true if recording started successfully, false otherwise
   */
  [[nodiscard]] bool startRecording() noexcept;

  /**
   * @brief Stop recording from microphone
   * @return true if recording stopped successfully, false otherwise
   */
  [[nodiscard]] bool stopRecording() noexcept;

  /**
   * @brief Start playback to speaker
   * @return true if playback started successfully, false otherwise
   */
  [[nodiscard]] bool startPlayback() noexcept;

  /**
   * @brief Stop playback to speaker
   * @return true if playback stopped successfully, false otherwise
   */
  [[nodiscard]] bool stopPlayback() noexcept;

  /**
   * @brief Record audio data from microphone
   * @param buffer Output buffer for audio data
   * @param bufferSize Size of output buffer in bytes
   * @param bytesRead Output parameter for actual bytes read
   * @return true if read successful, false otherwise
   */
  [[nodiscard]] bool record(uint8_t* buffer, size_t bufferSize, size_t& bytesRead) noexcept;

  /**
   * @brief Play audio data to speaker
   * @param buffer Input buffer containing audio data
   * @param bufferSize Size of audio data in bytes
   * @param bytesWritten Output parameter for actual bytes written
   * @return true if write successful, false otherwise
   */
  [[nodiscard]] bool play(const uint8_t* buffer, size_t bufferSize, size_t& bytesWritten) noexcept;

  /**
   * @brief Set speaker volume
   * @param volume Volume level (0-100)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setVolume(uint8_t volume) noexcept;

  /**
   * @brief Set microphone gain
   * @param gain Gain level (0-100)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setMicrophoneGain(uint8_t gain) noexcept;

  /**
   * @brief Mute audio output
   * @return true if mute successful, false otherwise
   */
  [[nodiscard]] bool mute() noexcept;

  /**
   * @brief Unmute audio output
   * @return true if unmute successful, false otherwise
   */
  [[nodiscard]] bool unmute() noexcept;

  /**
   * @brief Check if audio is muted
   * @return true if muted, false otherwise
   */
  [[nodiscard]] bool isMuted() const noexcept;

  /**
   * @brief Check if currently recording
   * @return true if recording, false otherwise
   */
  [[nodiscard]] bool isRecording() const noexcept;

  /**
   * @brief Check if currently playing
   * @return true if playing, false otherwise
   */
  [[nodiscard]] bool isPlaying() const noexcept;

  /**
   * @brief Check if audio manager is initialized
   * @return true if initialized, false otherwise
   */
  [[nodiscard]] bool isInitialized() const noexcept;

  /**
   * @brief Get current speaker volume
   * @return Volume level (0-100)
   */
  [[nodiscard]] uint8_t getVolume() const noexcept;

  /**
   * @brief Get current microphone gain
   * @return Gain level (0-100)
   */
  [[nodiscard]] uint8_t getMicrophoneGain() const noexcept;

  /**
   * @brief Get current sample rate
   * @return Sample rate in Hz
   */
  [[nodiscard]] uint32_t getSampleRate() const noexcept;

  /**
   * @brief Get DMA buffer size
   * @return Buffer size in bytes
   */
  [[nodiscard]] size_t getBufferSize() const noexcept;

  /**
   * @brief Get current audio state
   * @return Current AudioState value
   */
  [[nodiscard]] AudioState getState() const noexcept;

  /**
   * @brief Flush playback buffers
   * @return true if flush successful, false otherwise
   */
  [[nodiscard]] bool flushPlayback() noexcept;

  /**
   * @brief Compute RMS energy of the last recorded audio chunk
   * @return RMS energy as float (0.0 - 32768.0)
   */
  [[nodiscard]] float getAudioEnergy() const noexcept;

  /**
   * @brief Compute peak amplitude of the last recorded audio chunk
   * @return Peak amplitude as float (0.0 - 32768.0)
   */
  [[nodiscard]] float getAudioPeak() const noexcept;

  /**
   * @brief Sample a short burst of live microphone audio (non-blocking)
   * @param energyOut Receives RMS energy of the burst
   * @param peakOut Receives peak amplitude of the burst
   * @return true if at least one I2S frame was captured
   */
  [[nodiscard]] bool sampleMicLevel(float& energyOut, float& peakOut) noexcept;

  /**
   * @brief Get current noise floor estimate
   * @return Noise floor RMS level
   */
  [[nodiscard]] uint16_t getNoiseFloor() const noexcept;

  /**
   * @brief Calibrate noise floor from ambient audio
   * @param samples Number of samples to average
   * @return true if calibration successful
   */
  [[nodiscard]] bool calibrateNoiseFloor(size_t samples = 1600) noexcept;

  /**
   * @brief Adaptive noise floor update using exponential moving average
   * @param currentRms Current ambient RMS level
   * @param alpha Smoothing factor (0.0-1.0, default 0.1). Lower = smoother
   */
  void updateAdaptiveNoiseFloor(uint16_t currentRms, float alpha = 0.1f) noexcept;

  /**
   * @brief Set noise threshold for wake word detection
   * @param threshold RMS value above noise floor considered signal
   */
  void setNoiseThreshold(uint16_t threshold) noexcept;

  /**
   * @brief Get noise threshold
   * @return Current noise threshold
   */
  [[nodiscard]] uint16_t getNoiseThreshold() const noexcept;

  /**
   * @brief Whether voice activity is currently detected by the always-on VAD
   * @return true when live mic energy exceeds the speech threshold
   */
  [[nodiscard]] bool isVoiceActive() const noexcept;

  /**
   * @brief Enable/disable the always-on voice-activity monitor.
   * @param enabled false disables the passive mic VAD (e.g. privacy mode)
   */
  void setVoiceActivityMonitoring(bool enabled) noexcept;

  /**
   * @brief Whether the always-on voice-activity monitor is enabled.
   */
  [[nodiscard]] bool isVoiceActivityMonitoringEnabled() const noexcept;

  // ========================================================================
  // Audio Queue & Speech Cancellation
  // ========================================================================

  /**
   * @brief Queue a text for TTS playback (prevents overlapping)
   * @param text Text to speak
   */
  void queueSpeech(const String& text) noexcept;

  /**
   * @brief Check if speech queue has pending items
   * @return true if queue is not empty
   */
  [[nodiscard]] bool hasQueuedSpeech() const noexcept;

  /**
   * @brief Clear all queued speech
   */
  void clearSpeechQueue() noexcept;

  /**
   * @brief Cancel current playback immediately (speech cancellation)
   */
  void cancelPlayback() noexcept;

  /**
   * @brief Play a short earcon (pre-defined confirmation tone)
   * @param earcon Earcon type
   */
  void playEarcon(uint8_t earcon) noexcept;

  /**
   * @brief Process speech queue — call regularly from loop
   */
  void processSpeechQueue() noexcept;

private:
  // Private helper methods
  bool configureInput() noexcept;
  bool configureOutput() noexcept;
  bool initializeI2S() noexcept;
  bool releaseI2S() noexcept;
  bool readMicrophone(uint8_t* buffer, size_t bufferSize, size_t& bytesRead) noexcept;
  bool writeSpeaker(const uint8_t* buffer, size_t bufferSize, size_t& bytesWritten) noexcept;
  void clearBuffers() noexcept;
  void resetState() noexcept;
  void updateGain() noexcept;
  void updateVolume() noexcept;
  void runVoiceActivityDetection() noexcept;

  // Private member variables
  AudioState m_state;                   ///< Current audio manager state
  bool m_initialized;                   ///< Initialization flag
  bool m_recording;                     ///< Recording in progress flag
  bool m_playing;                       ///< Playback in progress flag
  bool m_muted;                         ///< Mute state flag
  uint8_t m_volume;                     ///< Speaker volume (0-100)
  uint8_t m_microphoneGain;             ///< Microphone gain (0-100)
  uint32_t m_sampleRate;                ///< Sample rate in Hz
  size_t m_dmaBufferSize;               ///< DMA buffer size in bytes
  size_t m_bufferSize;                  ///< Total buffer size in bytes
  uint8_t* m_recordBuffer;              ///< Recording buffer pointer
  uint8_t* m_playbackBuffer;            ///< Playback buffer pointer
  i2s_chan_handle_t m_microphoneHandle;  ///< I2S channel handle for microphone
  i2s_chan_handle_t m_speakerHandle;     ///< I2S channel handle for speaker
  uint32_t m_lastUpdateTime;            ///< Timestamp of last update
  bool m_i2sInitialized;                ///< I2S hardware initialized flag

  // Wake word audio support
  float m_lastAudioEnergy;              ///< RMS energy of last chunk
  float m_lastAudioPeak;                ///< Peak amplitude of last chunk
  uint16_t m_noiseFloor;                ///< Estimated noise floor (RMS)
  uint16_t m_noiseThreshold;            ///< Noise threshold for signal detection

  // Voice activity detection (idle always-on monitoring)
  bool m_voiceActive;                   ///< VAD currently latched on
  bool m_vadMonitoring;                 ///< Always-on VAD monitor enabled
  uint8_t m_voiceDebounce;              ///< Frames below threshold before release
  unsigned long m_lastVadSampleTime;    ///< Timestamp of last VAD mic sample
  static constexpr uint32_t kVadIdleIntervalMs = 90UL;
  static constexpr uint32_t kVadActiveIntervalMs = 18UL;
  static constexpr uint8_t kVadReleaseFrames = 2;

  // Audio queue
  std::vector<String> m_speechQueue;
  static constexpr size_t kMaxSpeechQueue = 16;
  bool m_queueProcessing;
};

/**
 * @brief Global audio manager instance
 */
extern AudioManager audioManager;

#endif // AURA_AUDIO_MANAGER_H