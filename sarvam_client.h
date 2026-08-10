#ifndef AURA_SARVAM_CLIENT_H
#define AURA_SARVAM_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "logger.h"
#include "secrets.h"

// ============================================================================
// Sarvam AI REST client - production implementation.
//
// Provides the shared HTTPS/TLS plumbing for the Sarvam AI speech APIs:
//   * Speech-to-Text  -> SarvamClient::startTranscription() (multipart upload)
//   * Text-to-Speech  -> SarvamClient::startSynthesis()    (streaming audio)
//
// Both requests are driven asynchronously by run(). Callers (SarvamSpeechToText
// / SarvamTextToSpeech) invoke a "start" method, then call run() from their own
// update() loop until the matching "done" flag is set. All I/O is bounded and
// non-blocking per tick so the RTOS loop / watchdog are never starved.
//
// Security: the API key is kept private (setApiKey from Secrets / NVS). The
// key is never logged and the Authorization header is never printed. TLS is
// always validated against a configured root CA - there is no insecure mode.
// ============================================================================

/**
 * @struct SarvamConfig
 * @brief Runtime-tunable settings for the Sarvam API.
 */
struct SarvamConfig {
    String endpoint;        ///< Base URL, e.g. https://api.sarvam.ai
    String language;        ///< target_language_code for STT/TTS
    String voice;           ///< TTS speaker name
    String sttModel;        ///< STT model id
    String ttsModel;        ///< TTS model id
    uint32_t sampleRate;    ///< Input PCM sample rate (Hz)
    uint32_t ttsSampleRate; ///< Requested output sample rate (Hz)
    unsigned long timeoutMs;///< Per-request operation timeout
    uint8_t  retryCount;    ///< Max retries for transient failures
    float    speed;         ///< TTS speed (unsupported by API; stored for config)

    SarvamConfig() noexcept
        : endpoint(SARVAM_BASE_URL), language(SARVAM_LANGUAGE), voice(SARVAM_VOICE),
          sttModel(SARVAM_STT_MODEL), ttsModel(SARVAM_TTS_MODEL),
          sampleRate(SARVAM_SAMPLE_RATE), ttsSampleRate(SARVAM_TTS_SAMPLE_RATE),
          timeoutMs(SARVAM_TIMEOUT_MS), retryCount(SARVAM_RETRY_MAX),
          speed(1.0f) {}
};

/**
 * @struct SarvamSttResult
 * @brief Outcome of a completed Sarvam STT request.
 */
struct SarvamSttResult {
    bool ok;              ///< Request succeeded (may still be an empty transcript)
    String transcript;    ///< Final UTF-8 transcript
    float confidence;     ///< Confidence (0..1) if reported by the API
    unsigned long latencyMs;

    SarvamSttResult() noexcept : ok(false), confidence(0.0f), latencyMs(0) {}
    void clear() noexcept { ok = false; transcript.clear(); confidence = 0.0f; latencyMs = 0; }
};

/**
 * @enum SarvamHttpError
 * @brief Transport/parse errors surfaced to the providers.
 */
enum class SarvamHttpError : uint8_t {
    NONE, NETWORK, TIMEOUT, AUTHENTICATION, HTTP_ERROR, JSON_ERROR, TLS_ERROR, UNKNOWN
};

/**
 * @class SarvamHttp
 * @brief Minimal TLS HTTPS socket wrapper used by the client.
 *
 * Owns a WiFiClientSecure and a CONNECT/SEND/RECEIVE stream. Operation is
 * bounded per call so providers can pace the upload/download across ticks.
 */
class SarvamHttp {
public:
    SarvamHttp() noexcept = default;
    ~SarvamHttp() noexcept { close(); }

    SarvamHttp(const SarvamHttp&) = delete;
    SarvamHttp& operator=(const SarvamHttp&) = delete;

    /** Parse host/port from an https URL and configure the TLS root CA. */
    bool configure(const String& url, const String& rootCA, unsigned long timeoutMs) noexcept;

    /** Open the TLS connection. Returns true on success (blocks up to timeout). */
    bool begin() noexcept;

    bool isConnected() noexcept { return m_client.connected(); }
    void close() noexcept;

    /** Write raw bytes; returns the number accepted (may be partial). */
    size_t write(const uint8_t* data, size_t len) noexcept;

    /** Read available bytes; returns bytes read or -1 on error (0 if none). */
    int read(uint8_t* buf, size_t cap) noexcept;

    /** Number of bytes currently buffered in the TLS client. */
    int available() noexcept { return m_client.available(); }

    void touch() noexcept { m_lastIo = millis(); }
    unsigned long lastIoMs() const noexcept { return m_lastIo; }

private:
    WiFiClientSecure m_client;
    String m_host;
    int m_port{443};
    String m_rootCA;
    unsigned long m_connectTimeoutMs{4000UL};
    unsigned long m_socketTimeoutMs{5UL};
    unsigned long m_lastIo{0};
};

/**
 * @class SarvamClient
 * @brief Single async engine for Sarvam STT + streaming TTS.
 */
class SarvamClient {
public:
    SarvamClient() noexcept = default;
    ~SarvamClient() noexcept;

    SarvamClient(const SarvamClient&) = delete;
    SarvamClient& operator=(const SarvamClient&) = delete;

    void setApiKey(const String& key) noexcept;
    void setEndpoint(const String& url) noexcept;
    void setRootCA(const String& caCert) noexcept;

    [[nodiscard]] const String& apiKey() const noexcept { return m_apiKey; }
    [[nodiscard]] const String& endpoint() const noexcept { return m_cfg.endpoint; }
    [[nodiscard]] const SarvamConfig& config() const noexcept { return m_cfg; }
    SarvamConfig& config() noexcept { return m_cfg; }

    /** True once a key and endpoint are configured (request may still fail). */
    [[nodiscard]] bool isAvailable() const noexcept;

    // ---- STT -----------------------------------------------------------------
    /** Begin an async STT upload from an already-built multipart body buffer. */
    void startTranscription(const uint8_t* body, size_t bodyLen) noexcept;
    bool sttInProgress() const noexcept;
    bool sttDone() const noexcept;
    [[nodiscard]] const SarvamSttResult& sttResult() const noexcept;

    // ---- TTS -----------------------------------------------------------------
    /** Chunk callback invoked during run() as decoded PCM bytes arrive. */
    using PcmChunkCallback = bool (*)(const uint8_t* data, size_t len, void* user);
    void startSynthesis(const String& text, PcmChunkCallback cb, void* user) noexcept;
    bool ttsInProgress() const noexcept;
    bool ttsDone() const noexcept;
    bool ttsOk() const noexcept;
    unsigned long ttsLatencyMs() const noexcept;

    [[nodiscard]] SarvamHttpError lastError() const noexcept { return m_httpErr; }

    /** Drive the active request. Call frequently from a provider update(). */
    void run() noexcept;

    /** Abort any in-flight request (e.g. user cancellation). */
    void cancel() noexcept;

    // ---- Shared helpers -------------------------------------------------------
    /** Build a canonical 44-byte PCM16 mono WAV header. */
    static size_t buildWavHeader(uint8_t* out, size_t cap, uint32_t sampleRate, uint32_t dataBytes) noexcept;

    /** Build a multipart/form-data STT body into `out`; returns total bytes. */
    static size_t buildMultipartStt(uint8_t* out, size_t cap, const uint8_t* pcm, size_t pcmBytes,
                                    uint32_t sampleRate, const String& lang, const String& model) noexcept;

    /** Replace all but the last 4 chars of a key for safe logging. */
    static void maskKey(const String& key, String& masked) noexcept;

private:
    void beginConnect() noexcept;
    void sendNextChunk() noexcept;
    void readIncoming() noexcept;
    void readHeaders() noexcept;
    void feedBase64(unsigned char c) noexcept;
    void flushB64() noexcept;
    void finalizeResponse() noexcept;
    void abort(SarvamHttpError err, bool transient) noexcept;
    void finalizeStt() noexcept;
    void finalizeTts() noexcept;
    bool shouldRetry() const noexcept;
    void scheduleBackoff() noexcept;
    void trimErrorBuffer() noexcept;

    // --- config
    SarvamConfig m_cfg;
    String m_apiKey;
    String m_rootCA;
    SarvamHttp m_http;

    // --- phase engine
    enum class Phase : uint8_t { IDLE, CONNECTING, SENDING, RECEIVING, BACKOFF };
    enum class Kind : uint8_t { NONE, STT, TTS };
    Phase m_phase{Phase::IDLE};
    Kind m_kind{Kind::NONE};
    bool m_finalized{false};  ///< Set whenever a terminal (success or failure) outcome is ready.
    uint8_t m_attempt{0};
    unsigned long m_backoffUntil{0};
    unsigned long m_startedAt{0};
    unsigned long m_lastActivity{0};

    // --- request buffers
    String m_header;
    size_t m_headerSent{0};
    const uint8_t* m_body{nullptr};
    size_t m_bodyLen{0};
    size_t m_bodySent{0};
    String m_ttsJson;          ///< owned body for TTS so raw pointer stays alive

    // --- response
    bool m_headersDone{false};
    int m_status{0};
    long m_contentLength{-1};
    size_t m_bodyReceived{0};
    String m_recvBuffer;
    String m_errorBuffer;

    // --- base64 decode (TTS)
    bool m_inBase64{false};
    String m_prefix;
    unsigned char m_b64[4]{0};
    uint8_t m_b64n{0};
    bool m_audioDone{false};
    size_t m_audioBytes{0};

    // --- outcomes
    SarvamSttResult m_stt;
    PcmChunkCallback m_pcmCb{nullptr};
    void* m_pcmUser{nullptr};
    bool m_ttsOk{false};
    unsigned long m_ttsLatency{0};
    SarvamHttpError m_httpErr{SarvamHttpError::NONE};

    static constexpr size_t kSttRecvCap = 16384;
    static constexpr size_t kErrRecvCap = 4096;
};

/**
 * @brief Global Sarvam AI client instance.
 */
extern SarvamClient sarvamClient;

#endif // AURA_SARVAM_CLIENT_H