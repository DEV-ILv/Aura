#ifndef AURA_SARVAM_TTS_H
#define AURA_SARVAM_TTS_H

#include <Arduino.h>
#include <deque>
#include "tts_provider.h"
#include "sarvam_client.h"

// ============================================================================
// Sarvam AI Text-to-Speech provider (production implementation).
//
// Flow: speak() enqueues TTSQueueItems (priority inserts at the front). The
// active item is synthesized asynchronously by SarvamClient, whose streaming
// base64 decoder emits PCM chunks through a PcmChunkCallback. Those bytes are
// buffered in a small ring and fed to AudioManager::play() (16 kHz mono) as
// the DA's playback pipe drains, so long utterances stream without buffering
// the whole file. pause()/resume()/stop() map to the TTS contract.
// ============================================================================

class SarvamTextToSpeech : public TextToSpeechProvider {
public:
    SarvamTextToSpeech() noexcept = default;
    ~SarvamTextToSpeech() noexcept override;

    bool initialize() noexcept override;
    void run() noexcept override;
    void update() noexcept override;
    bool speak(const String& text, bool priority = false) noexcept override;
    void stop() noexcept override;
    void pause() noexcept override;
    void resume() noexcept override;
    void clearQueue() noexcept override;
    void setVoice(const String& voice) noexcept override;
    void setLanguage(const String& language) noexcept override;
    void setSpeed(float speed) noexcept override;
    void setPitch(float pitch) noexcept override;
    void setVolume(uint8_t volume) noexcept override;
    void setApiKey(const String& apiKey) noexcept override;
    void setApiEndpoint(const String& endpoint) noexcept override;
    void setRootCA(const String& caCert) noexcept override;
    void enableStreaming() noexcept override;
    void disableStreaming() noexcept override;

    bool isBusy() const noexcept override;
    bool isPlaying() const noexcept override;
    bool isInitialized() const noexcept override;
    TTSState getState() const noexcept override;
    TTSError getError() const noexcept override;
    const TTSResponse& getResponse() const noexcept override;
    size_t getQueueSize() const noexcept override;

    /**
     * @brief Clear the latched error state after a failure has been handled.
     * @note Used by the conversation error recovery so the pipeline can leave
     *       the ERROR state instead of being trapped by a stale error latch.
     */
    void clearError() noexcept;

    TTSProvider providerType() const noexcept override { return TTSProvider::SARVAM; }
    const char* providerName() const noexcept override { return "Sarvam TTS"; }

private:
    static bool pcmChunkCallback(const uint8_t* data, size_t len, void* user) noexcept;
    void beginNext() noexcept;
    void drainPlayback() noexcept;
    bool onPcm(const uint8_t* data, size_t len) noexcept;
    void mapHttpError() noexcept;
    void finishItem(bool ok, TTSError err) noexcept;
    void ringPut(const uint8_t* data, size_t len) noexcept;
    size_t ringPeek(uint8_t* out, size_t cap) noexcept;
    void ringAdvance(size_t n) noexcept;
    size_t ringCount() const noexcept;

    TTSResponse m_result;
    TTSState m_state{TTSState::IDLE};
    TTSError m_error{TTSError::NONE};
    std::deque<TTSQueueItem> m_queue;
    TTSQueueItem m_current;

    bool m_initialized{false};
    bool m_streaming{true};
    bool m_paused{false};
    bool m_srvActive{false};
    bool m_draining{false};
    bool m_playStarted{false};

    String m_voice{SARVAM_VOICE};
    String m_language{SARVAM_LANGUAGE};
    float  m_speed{1.0f};
    float  m_pitch{0.0f};
    uint8_t m_volume{100};
    String m_apiKey;
    String m_endpoint;
    String m_rootCA;

    // PCM ring from the decode callback -> AudioManager::play().
    static constexpr size_t kRingCap = 32768;
    uint8_t m_ring[kRingCap];
    uint32_t m_ringHead{0};
    uint32_t m_ringTail{0};
    uint32_t m_ringCount{0};

    size_t m_audioPushedBytes{0};
    size_t m_ringDropped{0};

    // Drain-completion tracking. audioManager.isPlaying() alone can never
    // signal "finished" (nothing clears it after startPlayback), so drain is
    // completed by a settle timer after the ring empties, with a hard timeout
    // so a wedged speaker/DMA can never hang the pipeline in PLAYING forever.
    static constexpr unsigned long kDrainSettleMs = 500UL;
    static constexpr unsigned long kDrainTimeoutMs = 30000UL;
    unsigned long m_drainStartMs{0};
    unsigned long m_lastPushMs{0};
};

extern SarvamTextToSpeech textToSpeech;

#endif // AURA_SARVAM_TTS_H