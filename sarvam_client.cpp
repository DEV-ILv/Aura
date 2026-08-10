#include "sarvam_client.h"
#include <WiFi.h>

SarvamClient sarvamClient;

// ============================================================================
// SarvamHttp - TLS socket wrapper
// ============================================================================

bool SarvamHttp::configure(const String& url, const String& rootCA, unsigned long timeoutMs) noexcept {
    m_rootCA = rootCA;
    m_connectTimeoutMs = (timeoutMs > 0U) ? timeoutMs : SARVAM_TIMEOUT_MS;
    m_socketTimeoutMs = (m_connectTimeoutMs / 1000UL) + 1UL;

    String host = url;
    const int proto = host.indexOf("://");
    if (proto >= 0) host = host.substring(proto + 3);
    const int slash = host.indexOf('/');
    if (slash >= 0) host = host.substring(0, slash);

    m_port = 443;
    const int colon = host.indexOf(':');
    if (colon >= 0) {
        m_port = host.substring(colon + 1).toInt();
        host = host.substring(0, colon);
    }
    m_host = host;
    return !m_host.isEmpty();
}

bool SarvamHttp::begin() noexcept {
    if (m_client.connected()) return true;

    if (m_rootCA.isEmpty()) {
        LOG_ERROR("SarvamClient", "No root CA configured - TLS cannot be validated (failing closed)");
        return false;
    }
    m_client.setCACert(m_rootCA.c_str());
    m_client.setTimeout(m_socketTimeoutMs);

    m_lastIo = millis();
    const bool ok = m_client.connect(m_host.c_str(), m_port);
    if (!ok) {
        LOG_WARN("SarvamClient", "TLS connect failed to %s", m_host.c_str());
        m_client.stop();
    } else {
        m_lastIo = millis();
    }
    return ok;
}

void SarvamHttp::close() noexcept {
    if (m_client.connected()) m_client.stop();
}

size_t SarvamHttp::write(const uint8_t* data, size_t len) noexcept {
    if (!m_client.connected()) return 0;
    const size_t n = m_client.write(data, len);
    if (n != 0) m_lastIo = millis();
    return n;
}

int SarvamHttp::read(uint8_t* buf, size_t cap) noexcept {
    if (!m_client.connected() && !m_client.available()) return 0;
    const int n = m_client.read(buf, cap);
    if (n > 0) m_lastIo = millis();
    return n;
}

// ============================================================================
// Helpers
// ============================================================================

namespace {
struct B64Entry { unsigned char value; bool valid; };
B64Entry b64Table(unsigned char c) noexcept {
    B64Entry e{0, false};
    if (c >= 'A' && c <= 'Z') { e.value = static_cast<unsigned char>(c - 'A'); e.valid = true; }
    else if (c >= 'a' && c <= 'z') { e.value = static_cast<unsigned char>(c - 'a' + 26); e.valid = true; }
    else if (c >= '0' && c <= '9') { e.value = static_cast<unsigned char>(c - '0' + 52); e.valid = true; }
    else if (c == '+') { e.value = 62; e.valid = true; }
    else if (c == '/') { e.value = 63; e.valid = true; }
    return e;
}

void appendEscaped(String& out, const String& text) noexcept {
    const char* p = text.c_str();
    while (*p) {
        const char c = *p++;
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    char hex[8];
                    snprintf(hex, sizeof(hex), "\\u%04x", static_cast<uint8_t>(c));
                    out += hex;
                } else {
                    out += c;
                }
        }
    }
}

/** Extract the JSON string value for a given ASCII key (simple manual parser). */
bool extractJsonValue(const String& body, const char* key, String& out) noexcept {
    String k = "\"";
    k += key;
    k += "\"";
    const int keyIdx = body.indexOf(k);
    if (keyIdx < 0) return false;
    const int colon = body.indexOf(':', keyIdx + k.length());
    if (colon < 0) return false;
    const int q1 = body.indexOf('"', colon + 1);
    if (q1 < 0) return false;
    out.clear();
    for (int i = q1 + 1; i < (int)body.length(); ++i) {
        const char c = body[i];
        if (c == '"') return true;
        if (c == '\\') {
            if (i + 1 < (int)body.length()) {
                const char n = body[i + 1];
                if (n == 'u') {
                    // Keep the unicode escape as-is (rare in transcripts); skip \uXXXX.
                    i += 5;
                } else {
                    out += n;
                    ++i;
                }
            }
            continue;
        }
        out += c;
    }
    return true;
}
} // namespace

// ============================================================================
// SarvamClient public API
// ============================================================================

SarvamClient::~SarvamClient() noexcept {
    cancel();
}

void SarvamClient::setApiKey(const String& key) noexcept { m_apiKey = key; }
void SarvamClient::setEndpoint(const String& url) noexcept { m_cfg.endpoint = url; }
void SarvamClient::setRootCA(const String& caCert) noexcept { m_rootCA = caCert; }

bool SarvamClient::isAvailable() const noexcept {
    return !m_apiKey.isEmpty() && !m_cfg.endpoint.isEmpty();
}

void SarvamClient::cancel() noexcept {
    m_phase = Phase::IDLE;
    m_kind = Kind::NONE;
    m_finalized = false;
    m_http.close();
    m_body = nullptr;
    m_bodyLen = 0;
    m_bodySent = 0;
    m_headerSent = 0;
    m_ttsJson.clear();
    m_headersDone = false;
    m_recvBuffer.clear();
    m_errorBuffer.clear();
    m_inBase64 = false;
    m_prefix.clear();
    m_b64n = 0;
    m_audioDone = false;
}

// ---- STT -------------------------------------------------------------------

void SarvamClient::startTranscription(const uint8_t* body, size_t bodyLen) noexcept {
    cancel();

    if (!isAvailable()) {
        m_httpErr = SarvamHttpError::NETWORK;
        m_stt.clear();
        m_stt.ok = false;
        m_finalized = true;
        return;
    }

    m_kind = Kind::STT;
    m_body = body;
    m_bodyLen = bodyLen;
    m_stt.clear();
    m_httpErr = SarvamHttpError::NONE;
    m_attempt = 0;
    m_startedAt = millis();
    m_lastActivity = millis();

    m_header = "POST ";
    m_header += SARVAM_STT_PATH;
    m_header += " HTTP/1.1\r\nHost: api.sarvam.ai\r\n";
    m_header += "api-subscription-key: ";
    m_header += m_apiKey;
    m_header += "\r\nAuthorization: Bearer ";
    m_header += m_apiKey;
    m_header += "\r\nContent-Type: multipart/form-data; boundary=BOUNDARY101\r\n";
    m_header += "Content-Length: ";
    m_header += String(bodyLen);
    m_header += "\r\n\r\n";

    m_phase = Phase::CONNECTING;
}

bool SarvamClient::sttInProgress() const noexcept { return m_kind == Kind::STT && m_phase != Phase::IDLE; }
bool SarvamClient::sttDone() const noexcept { return m_finalized && m_kind == Kind::STT && m_phase == Phase::IDLE; }
const SarvamSttResult& SarvamClient::sttResult() const noexcept { return m_stt; }

// ---- TTS -------------------------------------------------------------------

void SarvamClient::startSynthesis(const String& text, PcmChunkCallback cb, void* user) noexcept {
    cancel();

    m_pcmCb = cb;
    m_pcmUser = user;
    m_httpErr = SarvamHttpError::NONE;
    m_audioBytes = 0;
    m_audioDone = false;
    m_ttsOk = false;
    m_ttsLatency = 0;
    m_inBase64 = false;
    m_prefix.clear();
    m_prefix.reserve(96);
    m_b64n = 0;

    if (text.isEmpty() || !isAvailable()) {
        m_httpErr = text.isEmpty() ? SarvamHttpError::HTTP_ERROR : SarvamHttpError::NETWORK;
        m_kind = Kind::NONE;
        m_phase = Phase::IDLE;
        m_finalized = true;
        return;
    }

    m_ttsJson = "{\"texts\":[\"";
    appendEscaped(m_ttsJson, text.c_str());
    m_ttsJson += "\"],\"target_language_code\":\"";
    m_ttsJson += m_cfg.language;
    m_ttsJson += "\",\"model\":\"";
    m_ttsJson += m_cfg.ttsModel;
    m_ttsJson += "\",\"speaker\":\"";
    m_ttsJson += m_cfg.voice;
    m_ttsJson += "\",\"sample_rate\":";
    m_ttsJson += String(m_cfg.ttsSampleRate);
    m_ttsJson += "}";

    m_body = reinterpret_cast<const uint8_t*>(m_ttsJson.c_str());
    m_bodyLen = m_ttsJson.length();
    m_bodySent = 0;

    m_header = "POST ";
    m_header += SARVAM_TTS_PATH;
    m_header += " HTTP/1.1\r\nHost: api.sarvam.ai\r\n";
    m_header += "Content-Type: application/json\r\n";
    m_header += "api-subscription-key: ";
    m_header += m_apiKey;
    m_header += "\r\nAuthorization: Bearer ";
    m_header += m_apiKey;
    m_header += "\r\nContent-Length: ";
    m_header += String(m_bodyLen);
    m_header += "\r\n\r\n";

    m_kind = Kind::TTS;
    m_attempt = 0;
    m_startedAt = millis();
    m_lastActivity = millis();
    m_phase = Phase::CONNECTING;
}

bool SarvamClient::ttsInProgress() const noexcept { return m_kind == Kind::TTS && m_phase != Phase::IDLE; }
bool SarvamClient::ttsDone() const noexcept { return m_finalized && m_kind == Kind::TTS && m_phase == Phase::IDLE; }
bool SarvamClient::ttsOk() const noexcept { return m_ttsOk; }
unsigned long SarvamClient::ttsLatencyMs() const noexcept { return m_ttsLatency; }

// ============================================================================
// Async engine
// ============================================================================

void SarvamClient::run() noexcept {
    if (m_phase == Phase::IDLE) return;

    if (m_phase == Phase::BACKOFF) {
        if (millis() < m_backoffUntil) return;
        m_lastActivity = millis();
        m_phase = Phase::CONNECTING;
        return;
    }

    // Timeouts apply to connecting/sending; receiving relies on the socket timeout
    // so that large TTS downloads are not cut short.
    if (m_phase == Phase::CONNECTING || m_phase == Phase::SENDING) {
        if ((unsigned long)(millis() - m_lastActivity) > m_cfg.timeoutMs) {
            abort(SarvamHttpError::TIMEOUT, true);
            return;
        }
    }

    switch (m_phase) {
        case Phase::CONNECTING: beginConnect(); break;
        case Phase::SENDING:    sendNextChunk(); break;
        case Phase::RECEIVING:  readIncoming(); break;
        default: break;
    }
}

void SarvamClient::beginConnect() noexcept {
    if (WiFi.status() != WL_CONNECTED) {
        abort(SarvamHttpError::NETWORK, true);
        return;
    }
    if (!m_http.configure(m_cfg.endpoint, m_rootCA, m_cfg.timeoutMs)) {
        abort(SarvamHttpError::TLS_ERROR, false);
        return;
    }
    m_http.close();
    if (!m_http.begin()) {
        abort(SarvamHttpError::TLS_ERROR, true);
        return;
    }
    m_headerSent = 0;
    m_bodySent = 0;
    m_lastActivity = millis();
    m_phase = Phase::SENDING;
}

void SarvamClient::sendNextChunk() noexcept {
    // Header first.
    if (m_headerSent < m_header.length()) {
        const size_t remain = m_header.length() - m_headerSent;
        const size_t want = (remain > 1024) ? 1024 : remain;
        const size_t n = m_http.write(
            reinterpret_cast<const uint8_t*>(m_header.c_str() + m_headerSent), want);
        if (n == 0) {
            if (!m_http.isConnected()) abort(SarvamHttpError::NETWORK, true);
            return;
        }
        m_headerSent += n;
        m_lastActivity = millis();
    }

    // Body next.
    if (m_body != nullptr && m_bodySent < m_bodyLen) {
        const size_t remain = m_bodyLen - m_bodySent;
        const size_t want = (remain > 2048) ? 2048 : remain;
        const size_t n = m_http.write(m_body + m_bodySent, want);
        if (n == 0) {
            if (!m_http.isConnected()) abort(SarvamHttpError::NETWORK, true);
            return;
        }
        m_bodySent += n;
        m_lastActivity = millis();
    }

    if (m_headerSent >= m_header.length() &&
        (m_body == nullptr || m_bodySent >= m_bodyLen)) {
        m_phase = Phase::RECEIVING;
        m_headersDone = false;
        m_recvBuffer.clear();
        m_bodyReceived = 0;
        m_lastActivity = millis();
        readIncoming();
    }
}

void SarvamClient::readIncoming() noexcept {
    if (m_audioDone) {
        finalizeResponse();
        return;
    }
    if (!m_headersDone) {
        readHeaders();
        if (!m_headersDone) return;
        if (m_status != 200) {
            // Drain the error body up to a cap, then report.
            finalizeResponse();
            return;
        }
    }

    uint8_t buf[1024];
    const int n = m_http.read(buf, sizeof(buf));
    if (n < 0) {
        abort(SarvamHttpError::NETWORK, true);
        return;
    }
    if (n == 0) {
        if (!m_http.isConnected() && m_http.available() == 0) {
            if (m_contentLength >= 0 && (long)m_bodyReceived >= m_contentLength) {
                finalizeResponse();
            } else if (m_kind == Kind::TTS && m_audioDone) {
                finalizeResponse();
            } else {
                abort(SarvamHttpError::NETWORK, true);
            }
        }
        return;
    }

    m_lastActivity = millis();
    m_bodyReceived += (size_t)n;

    if (m_kind == Kind::STT) {
        if (m_recvBuffer.length() + n > kSttRecvCap) {
            abort(SarvamHttpError::JSON_ERROR, false);
            return;
        }
        m_recvBuffer.concat((const char*)buf, (size_t)n);
        if (m_contentLength >= 0 && (long)m_bodyReceived >= m_contentLength) {
            finalizeResponse();
        }
    } else {
        for (int i = 0; i < n; ++i) feedBase64(buf[i]);
        if (m_audioDone) finalizeResponse();
    }
}

void SarvamClient::readHeaders() noexcept {
    uint8_t buf[512];
    const int n = m_http.read(buf, sizeof(buf));
    if (n <= 0) return;

    m_recvBuffer.concat((const char*)buf, (size_t)n);

    const int hdrEnd = m_recvBuffer.indexOf("\r\n\r\n");
    if (hdrEnd < 0) {
        if (m_recvBuffer.length() > 4096) abort(SarvamHttpError::UNKNOWN, false);
        return;
    }

    const String head = m_recvBuffer.substring(0, hdrEnd);

    m_status = 0;
    const int sp1 = head.indexOf(' ');
    const int sp2 = (sp1 >= 0) ? head.indexOf(' ', sp1 + 1) : -1;
    if (sp1 >= 0 && sp2 >= 0) m_status = head.substring(sp1 + 1, sp2).toInt();

    m_contentLength = -1;
    int ci = head.indexOf("Content-Length:");
    if (ci < 0) ci = head.indexOf("content-length:");
    if (ci >= 0) {
        const int lineEnd = head.indexOf("\r\n", ci);
        String clv = head.substring(ci + 15, (lineEnd < 0) ? (int)head.length() : lineEnd);
        clv.trim();
        m_contentLength = clv.toInt();
    }

    m_headersDone = true;
    const int bodyStart = hdrEnd + 4;
    if ((size_t)bodyStart < m_recvBuffer.length()) {
        m_recvBuffer = m_recvBuffer.substring(bodyStart);
        m_bodyReceived = m_recvBuffer.length();
    } else {
        m_recvBuffer.clear();
        m_bodyReceived = 0;
    }
    m_lastActivity = millis();
}

void SarvamClient::feedBase64(unsigned char c) noexcept {
    if (!m_inBase64) {
        if (c == '"') {
            String w = m_prefix;
            w.trim();
            if (w.endsWith("\"audio_base64\":")) {
                m_inBase64 = true;
                m_b64n = 0;
                m_audioBytes = 0;
                return;
            }
        }
        m_prefix += (char)c;
        if (m_prefix.length() > 80) m_prefix = m_prefix.substring(m_prefix.length() - 40);
        return;
    }

    if (c == '"') {
        if (m_b64n > 0) flushB64();
        m_audioDone = true;
        return;
    }
    const B64Entry e = b64Table(c);
    if (!e.valid) return;
    m_b64[m_b64n++] = e.value;
    if (m_b64n == 4) {
        m_b64n = 0;
        const unsigned char out[3] = {
            static_cast<unsigned char>((m_b64[0] << 2) | (m_b64[1] >> 4)),
            static_cast<unsigned char>((m_b64[1] << 4) | (m_b64[2] >> 2)),
            static_cast<unsigned char>((m_b64[2] << 6) | m_b64[3])
        };
        m_audioBytes += 3;
        if (m_pcmCb) {
            if (!m_pcmCb(out, 3, m_pcmUser)) {
                m_audioDone = true;
            }
        }
    }
}

void SarvamClient::flushB64() noexcept {
    if (m_b64n < 2) { m_b64n = 0; return; }
    const uint8_t count = (m_b64n == 2) ? 1 : 2;
    uint8_t out[2];
    out[0] = static_cast<uint8_t>((m_b64[0] << 2) | (m_b64[1] >> 4));
    if (count == 2) out[1] = static_cast<uint8_t>((m_b64[1] << 4) | (m_b64[2] >> 2));
    m_audioBytes += count;
    if (m_pcmCb) {
        if (!m_pcmCb(out, count, m_pcmUser)) m_audioDone = true;
    }
    m_b64n = 0;
}

void SarvamClient::finalizeResponse() noexcept {
    m_phase = Phase::IDLE;
    m_finalized = true;
    m_http.close();
    if (m_kind == Kind::STT) finalizeStt();
    else finalizeTts();
}

void SarvamClient::finalizeStt() noexcept {
    m_stt.latencyMs = (unsigned long)(millis() - m_startedAt);
    if (m_status == 200) {
        m_stt.ok = true;
        String t;
        if (extractJsonValue(m_recvBuffer, "transcript", t) && !t.isEmpty()) {
            m_stt.transcript = t;
        }
    }
}

void SarvamClient::finalizeTts() noexcept {
    m_ttsLatency = (unsigned long)(millis() - m_startedAt);
    m_ttsOk = (m_status == 200) && (m_audioBytes > 0);
}

void SarvamClient::abort(SarvamHttpError err, bool transient) noexcept {
    m_http.close();
    if (transient && m_attempt < m_cfg.retryCount) {
        ++m_attempt;
        m_httpErr = err;
        const unsigned long delay = 500UL * (1UL << (m_attempt - 1));
        m_backoffUntil = millis() + delay;
        m_phase = Phase::BACKOFF;
        m_headerSent = 0;
        m_bodySent = 0;
        m_headersDone = false;
        m_recvBuffer.clear();
        return;
    }
    m_httpErr = err;
    m_phase = Phase::IDLE;
    m_finalized = true;
    if (m_kind == Kind::STT) m_stt.ok = false;
}

void SarvamClient::maskKey(const String& key, String& masked) noexcept {
    masked = F("***");
    const int len = key.length();
    if (len <= 8) {
        masked += key.substring(0, 2);
        return;
    }
    masked += key.substring(len - 4);
}

size_t SarvamClient::buildWavHeader(uint8_t* out, size_t cap, uint32_t sampleRate, uint32_t dataBytes) noexcept {
    if (!out || cap < 44) return 0;
    memset(out, 0, 44);
    out[0] = 'R'; out[1] = 'I'; out[2] = 'F'; out[3] = 'F';
    const uint32_t riff = 36 + dataBytes;
    out[4] = riff & 0xFF; out[5] = (riff >> 8) & 0xFF; out[6] = (riff >> 16) & 0xFF; out[7] = (riff >> 24) & 0xFF;
    out[8] = 'W'; out[9] = 'A'; out[10] = 'V'; out[11] = 'E';
    out[12] = 'f'; out[13] = 'm'; out[14] = 't'; out[15] = ' ';
    out[16] = 16; out[17] = 0; out[18] = 0; out[19] = 0;
    out[20] = 1; out[21] = 0;
    out[22] = 1; out[23] = 0;
    out[24] = sampleRate & 0xFF; out[25] = (sampleRate >> 8) & 0xFF;
    out[26] = (sampleRate >> 16) & 0xFF; out[27] = (sampleRate >> 24) & 0xFF;
    const uint32_t byteRate = sampleRate * 2;
    out[28] = byteRate & 0xFF; out[29] = (byteRate >> 8) & 0xFF;
    out[30] = (byteRate >> 16) & 0xFF; out[31] = (byteRate >> 24) & 0xFF;
    out[32] = 2; out[33] = 0;
    out[34] = 16; out[35] = 0;
    out[36] = 'd'; out[37] = 'a'; out[38] = 't'; out[39] = 'a';
    out[40] = dataBytes & 0xFF; out[41] = (dataBytes >> 8) & 0xFF;
    out[42] = (dataBytes >> 16) & 0xFF; out[43] = (dataBytes >> 24) & 0xFF;
    return 44;
}

size_t SarvamClient::buildMultipartStt(uint8_t* out, size_t cap, const uint8_t* pcm, size_t pcmBytes,
                                       uint32_t sampleRate, const String& lang, const String& model) noexcept {
    (void)lang;
    (void)model;
    if (!out || !pcm) return 0;

    const char* head =
        "--BOUNDARY101\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const char* tail = "\r\n--BOUNDARY101--\r\n";

    const size_t headLen = strlen(head);
    const size_t tailLen = strlen(tail);
    const size_t total = headLen + 44 + pcmBytes + tailLen;
    if (cap < total) return 0;

    size_t o = 0;
    memcpy(out + o, head, headLen); o += headLen;
    uint8_t wav[44];
    buildWavHeader(wav, sizeof(wav), sampleRate, (uint32_t)pcmBytes);
    memcpy(out + o, wav, 44); o += 44;
    memcpy(out + o, pcm, pcmBytes); o += pcmBytes;
    memcpy(out + o, tail, tailLen); o += tailLen;
    return o;
}