#include "ota_manager.h"
#include <new>
#include <base64.h>
#include <cstring>

/// Global OtaManager instance
OtaManager otaManager;

// ============================================================================
// Anonymous Namespace - Internal Helpers
// ============================================================================

namespace {

constexpr size_t kChunkSize = 4096U;
constexpr unsigned long kDownloadTimeoutMs = 30000UL;
constexpr unsigned long kDefaultCheckIntervalMs = 3600000UL;  // 1 hour

/**
 * @brief Compare semantic versions (major.minor.patch)
 * @return true if v1 > v2
 */
bool versionGreater(const String& v1, const String& v2) noexcept {
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;

    sscanf(v1.c_str(), "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(v2.c_str(), "%d.%d.%d", &major2, &minor2, &patch2);

    if (major1 != major2) return major1 > major2;
    if (minor1 != minor2) return minor1 > minor2;
    return patch1 > patch2;
}

/**
 * @brief Base64 encode helper
 */
String base64Encode(const String& input) noexcept {
    size_t outputLen = ((input.length() + 2) / 3) * 4;
    char* output = new char[outputLen + 1];
    if (!output) return "";

    const unsigned char* in = reinterpret_cast<const unsigned char*>(input.c_str());
    char* out = output;
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (size_t i = 0; i < input.length(); i += 3) {
        uint32_t triple = (in[i] << 16);
        if (i + 1 < input.length()) triple |= (in[i + 1] << 8);
        if (i + 2 < input.length()) triple |= in[i + 2];

        *out++ = b64[(triple >> 18) & 0x3F];
        *out++ = b64[(triple >> 12) & 0x3F];
        *out++ = (i + 1 < input.length()) ? b64[(triple >> 6) & 0x3F] : '=';
        *out++ = (i + 2 < input.length()) ? b64[triple & 0x3F] : '=';
    }
    *out = '\0';

    String result(output);
    delete[] output;
    return result;
}

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

OtaManager::OtaManager() noexcept
    : m_initialized(false),
      m_currentState(OTAState::IDLE),
      m_lastError(OTAError::NONE),
      m_autoUpdateEnabled(false),
      m_checkIntervalMs(kDefaultCheckIntervalMs),
      m_lastCheckTime(0),
      m_stateStartTime(0),
      m_downloadedBytes(0),
      m_lastLoggedProgress(0) {
    mbedtls_sha256_init(&m_sha256Ctx);
    m_info.currentVersion = AURA_VERSION;
    m_info.currentVersion.trim();
}

OtaManager::~OtaManager() noexcept {
    cleanup();
}

// ============================================================================
// Public API - Lifecycle
// ============================================================================

bool OtaManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    Logger::info(kLogCategory, "Initializing OTA manager (current version: %s)",
        m_info.currentVersion.c_str());

    if (!wifiManager.isConnected()) {
        Logger::error(kLogCategory, "WiFi not connected");
        setError(OTAError::NO_WIFI);
        return false;
    }

    m_http.setTimeout(30000);

    m_initialized = true;
    m_currentState = OTAState::IDLE;
    m_lastError = OTAError::NONE;
    m_lastCheckTime = 0;
    m_downloadedBytes = 0;

    Logger::info(kLogCategory, "OTA manager initialized");
    return true;
}

void OtaManager::run() noexcept {
    update();
}

void OtaManager::update() noexcept {
    if (!m_initialized) return;

    unsigned long now = millis();

    // Auto-check for updates
    if (m_autoUpdateEnabled && now - m_lastCheckTime >= m_checkIntervalMs) {
        m_lastCheckTime = now;
        checkForUpdates();
    }

    // State machine
    switch (m_currentState) {
        case OTAState::IDLE:
        case OTAState::ERROR:
            // Waiting for user action
            break;

        case OTAState::CHECKING: {
            if (!connectServer()) {
                changeState(OTAState::ERROR);
                break;
            }

            String response = m_http.getString();
            m_http.end();

            if (!parseUpdateInfo(response)) {
                setError(OTAError::NETWORK);
                changeState(OTAState::ERROR);
                break;
            }

            if (!m_info.updateAvailable) {
                Logger::info(kLogCategory, "No update available");
                changeState(OTAState::IDLE);
                break;
            }

            Logger::info(kLogCategory, "Update available: %s -> %s (%u bytes)",
                m_info.currentVersion.c_str(), m_info.latestVersion.c_str(),
                static_cast<unsigned int>(m_info.firmwareSize));

            if (m_autoUpdateEnabled && m_info.mandatory) {
                startUpdate();
            } else {
                changeState(OTAState::IDLE);
            }
            break;
        }

        case OTAState::DOWNLOADING: {
            if (!downloadFirmware()) {
                cleanup();
                setError(OTAError::DOWNLOAD);
                changeState(OTAState::ERROR);
                break;
            }

            if (m_downloadedBytes >= m_info.firmwareSize) {
                m_http.end();
                changeState(OTAState::VERIFYING);
            }
            break;
        }

        case OTAState::VERIFYING: {
            if (!verifyFirmware()) {
                cleanup();
                setError(OTAError::VERIFY);
                changeState(OTAState::ERROR);
                break;
            }

            changeState(OTAState::INSTALLING);
            break;
        }

        case OTAState::INSTALLING: {
            if (!installFirmware()) {
                setError(OTAError::INSTALL);
                changeState(OTAState::ERROR);
                break;
            }

            changeState(OTAState::REBOOT_PENDING);
            break;
        }

        case OTAState::REBOOT_PENDING: {
            rebootDevice();
            break;
        }

        case OTAState::COMPLETED: {
            changeState(OTAState::IDLE);
            break;
        }
    }
}

// ============================================================================
// Public API - Update Operations
// ============================================================================

bool OtaManager::checkForUpdates() noexcept {
    if (!m_initialized) {
        setError(OTAError::UNKNOWN);
        return false;
    }

    if (!wifiManager.isConnected()) {
        Logger::error(kLogCategory, "Check failed: no WiFi");
        setError(OTAError::NO_WIFI);
        return false;
    }

    if (m_currentState != OTAState::IDLE && m_currentState != OTAState::ERROR) {
        Logger::warning(kLogCategory, "Check failed: busy (%d)", static_cast<int>(m_currentState));
        return false;
    }

    changeState(OTAState::CHECKING);
    m_lastCheckTime = millis();
    return true;
}

bool OtaManager::startUpdate() noexcept {
    if (!m_initialized) return false;

    if (!m_info.updateAvailable) {
        Logger::warning(kLogCategory, "No update available to start");
        return false;
    }

    if (m_currentState != OTAState::IDLE && m_currentState != OTAState::ERROR) {
        Logger::warning(kLogCategory, "Cannot start update: busy");
        return false;
    }

    cleanup();
    m_downloadedBytes = 0;
    m_info.downloadProgress = 0;

    if (!m_info.firmwareSize) {
        Logger::error(kLogCategory, "Firmware size unknown");
        return false;
    }

    changeState(OTAState::DOWNLOADING);
    Logger::info(kLogCategory, "Starting update to version %s", m_info.latestVersion.c_str());
    return true;
}

void OtaManager::cancelUpdate() noexcept {
    if (m_currentState == OTAState::IDLE || m_currentState == OTAState::COMPLETED || m_currentState == OTAState::ERROR) {
        return;
    }

    Logger::info(kLogCategory, "Cancelling update");
    cleanup();
    changeState(OTAState::IDLE);
}

void OtaManager::scheduleUpdate() noexcept {
    m_lastCheckTime = 0;
    checkForUpdates();
}

void OtaManager::enableAutoUpdate() noexcept {
    m_autoUpdateEnabled = true;
    m_lastCheckTime = 0;
    Logger::info(kLogCategory, "Auto-update enabled");
}

void OtaManager::disableAutoUpdate() noexcept {
    m_autoUpdateEnabled = false;
    Logger::info(kLogCategory, "Auto-update disabled");
}

void OtaManager::setUpdateServer(const String& url) noexcept {
    m_updateServer = url;
    if (m_updateServer.endsWith("/")) {
        m_updateServer.remove(m_updateServer.length() - 1);
    }
    Logger::info(kLogCategory, "Update server set to: %s", m_updateServer.c_str());
}

void OtaManager::setRootCA(const String& caCert) noexcept {
    if (caCert.isEmpty()) {
        Logger::warning(kLogCategory, "No CA cert provided — TLS connections will fail");
    } else {
        m_client.setCACert(caCert.c_str());
        Logger::info(kLogCategory, "Root CA certificate set");
    }
}

void OtaManager::setAuthentication(const String& username, const String& password) noexcept {
    m_authUsername = username;
    m_authPassword = password;
    if (!username.isEmpty()) {
        Logger::info(kLogCategory, "HTTP Basic auth configured");
    }
}

void OtaManager::setCheckInterval(unsigned long intervalMs) noexcept {
    if (intervalMs < 60000) intervalMs = 60000;  // Minimum 1 minute
    m_checkIntervalMs = intervalMs;
    Logger::info(kLogCategory, "Check interval set to %lu ms", m_checkIntervalMs);
}

// ============================================================================
// Public API - State Queries
// ============================================================================

bool OtaManager::isBusy() const noexcept {
    return m_currentState != OTAState::IDLE && m_currentState != OTAState::ERROR && m_currentState != OTAState::COMPLETED;
}

bool OtaManager::isUpdating() const noexcept {
    return m_currentState == OTAState::DOWNLOADING ||
           m_currentState == OTAState::VERIFYING ||
           m_currentState == OTAState::INSTALLING ||
           m_currentState == OTAState::REBOOT_PENDING;
}

bool OtaManager::isInitialized() const noexcept {
    return m_initialized;
}

bool OtaManager::isUpdateAvailable() const noexcept {
    return m_info.updateAvailable;
}

uint8_t OtaManager::getProgress() const noexcept {
    if (m_info.firmwareSize == 0) return 0;
    return static_cast<uint8_t>((m_downloadedBytes * 100) / m_info.firmwareSize);
}

const OTAInfo& OtaManager::getInfo() const noexcept {
    return m_info;
}

OTAState OtaManager::getState() const noexcept {
    return m_currentState;
}

OTAError OtaManager::getError() const noexcept {
    return m_lastError;
}

// ============================================================================
// Private Methods
// ============================================================================

void OtaManager::changeState(OTAState newState) noexcept {
    if (m_currentState == newState) return;

    static constexpr bool validTransition[8][8] = {
        // IDLE, CHECKING, DOWNLOADING, VERIFYING, INSTALLING, REBOOT_PENDING, COMPLETED, ERROR
        {0, 1, 0, 0, 0, 0, 1, 1},    // IDLE
        {1, 0, 1, 0, 0, 0, 0, 1},    // CHECKING
        {1, 0, 0, 1, 0, 0, 0, 1},    // DOWNLOADING
        {1, 0, 0, 0, 1, 0, 0, 1},    // VERIFYING
        {0, 0, 0, 0, 0, 1, 0, 1},    // INSTALLING
        {0, 0, 0, 0, 0, 0, 0, 1},    // REBOOT_PENDING
        {1, 1, 0, 0, 0, 0, 0, 1},    // COMPLETED
        {1, 0, 0, 0, 0, 0, 0, 0}     // ERROR
    };

    if (!validTransition[static_cast<uint8_t>(m_currentState)][static_cast<uint8_t>(newState)]) {
        Logger::warning(kLogCategory, "Invalid state transition %d -> %d",
            static_cast<int>(m_currentState), static_cast<int>(newState));
        return;
    }

    Logger::debug(kLogCategory, "State: %d -> %d",
        static_cast<int>(m_currentState), static_cast<int>(newState));
    m_currentState = newState;
    m_stateStartTime = millis();
}

void OtaManager::setError(OTAError error) noexcept {
    if (m_lastError == error) return;
    m_lastError = error;
    m_info.updateAvailable = false;
    Logger::error(kLogCategory, "Error: %d", static_cast<int>(error));
}

bool OtaManager::connectServer() noexcept {
    if (m_updateServer.isEmpty()) {
        Logger::error(kLogCategory, "Update server not configured");
        return false;
    }

    String url = m_updateServer + "/api/ota/check";
    url += "?version=" + m_info.currentVersion;
    url += "&device=" + String(AURA_NAME);

    m_http.begin(m_client, url);

    if (!m_authUsername.isEmpty()) {
        String auth = m_authUsername + ":" + m_authPassword;
        String encoded = base64Encode(auth);
        m_http.addHeader("Authorization", "Basic " + encoded);
    }

    m_http.addHeader("User-Agent", String(AURA_NAME) + "/" + AURA_VERSION);
    m_http.setTimeout(30000);

    int httpCode = m_http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Logger::error(kLogCategory, "Server request failed: %d", httpCode);
        if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == HTTP_CODE_FORBIDDEN) {
            setError(OTAError::AUTHENTICATION);
        }
        m_http.end();
        return false;
    }

    return true;
}

bool OtaManager::downloadFirmware() noexcept {
    if (m_downloadedBytes == 0) {
        // First chunk - initialize hash and start download
        sha256Init();
        String url = m_updateServer + "/api/ota/download";
        url += "?version=" + m_info.latestVersion;

        m_http.begin(m_client, url);

        if (!m_authUsername.isEmpty()) {
            String auth = m_authUsername + ":" + m_authPassword;
            String encoded = base64Encode(auth);
            m_http.addHeader("Authorization", "Basic " + encoded);
        }

        int httpCode = m_http.GET();
        if (httpCode != HTTP_CODE_OK) {
            Logger::error(kLogCategory, "Download request failed: %d", httpCode);
            m_http.end();
            return false;
        }

        m_info.totalBytes = m_http.getSize();
        if (m_info.totalBytes > 0) {
            m_info.firmwareSize = m_info.totalBytes;
        }

        // Start Update
        if (!Update.begin(m_info.firmwareSize)) {
            Logger::error(kLogCategory, "Update.begin() failed: %s", Update.errorString());
            m_http.end();
            return false;
        }
    }

    // Read chunk
    WiFiClient* stream = m_http.getStreamPtr();
    if (!stream) {
        Logger::error(kLogCategory, "No stream available");
        return false;
    }

    uint8_t buffer[kChunkSize];
    size_t bytesRead = 0;
    unsigned long chunkStart = millis();

    while (stream->available() && bytesRead < kChunkSize) {
        if (millis() - chunkStart > kDownloadTimeoutMs) {
            Logger::error(kLogCategory, "Download chunk timeout");
            return false;
        }
        int b = stream->read();
        if (b < 0) break;
        buffer[bytesRead++] = static_cast<uint8_t>(b);
    }

    if (bytesRead == 0 && !stream->available() && m_http.connected()) {
        // Wait for more data
        return true;
    }

    // Write to Update
    if (bytesRead > 0) {
        if (Update.write(buffer, bytesRead) != bytesRead) {
            Logger::error(kLogCategory, "Update.write() failed: %s", Update.errorString());
            return false;
        }
        sha256Update(buffer, bytesRead);
        m_downloadedBytes += bytesRead;
        m_info.downloadProgress = m_downloadedBytes;

        // Log progress every 10%
        uint8_t progress = getProgress();
        if (progress >= m_lastLoggedProgress + 10) {
            Logger::info(kLogCategory, "Download progress: %u%% (%u/%u bytes)",
                progress, static_cast<unsigned int>(m_downloadedBytes),
                static_cast<unsigned int>(m_info.firmwareSize));
            m_lastLoggedProgress = progress;
        }
    }

    // Check if complete
    if (m_downloadedBytes >= m_info.firmwareSize) {
        m_lastLoggedProgress = 0;  // Reset for next download
        return true;
    }

    return true;  // Continue downloading
}

bool OtaManager::verifyFirmware() noexcept {
    // Verify firmware was written completely
    if (!Update.isFinished()) {
        Logger::error(kLogCategory, "Update not finished");
        return false;
    }

    // Verify no errors
    if (Update.hasError()) {
        Logger::error(kLogCategory, "Update error: %s", Update.errorString());
        return false;
    }

    // Verify size matches
    if (m_downloadedBytes != m_info.firmwareSize) {
        Logger::error(kLogCategory, "Size mismatch: got %u, expected %u",
            static_cast<unsigned int>(m_downloadedBytes),
            static_cast<unsigned int>(m_info.firmwareSize));
        return false;
    }

    // Verify ECDSA signature before finalizing
    uint8_t hash[32];
    sha256Final(hash);
    if (!verifyFirmwareSignature(hash)) {
        Logger::error(kLogCategory, "Firmware signature verification failed");
        setError(OTAError::VERIFY);
        return false;
    }

    // Finalize update (calculates and verifies MD5)
    if (!Update.end(true)) {
        Logger::error(kLogCategory, "Update.end() failed: %s", Update.errorString());
        return false;
    }

    return true;
}

bool OtaManager::installFirmware() noexcept {
    // Update.end(true) in verifyFirmware() already finalized
    // Here we just confirm the update is ready for reboot

    if (!Update.isFinished()) {
        Logger::error(kLogCategory, "Install: update not finished");
        return false;
    }

    if (Update.hasError()) {
        Logger::error(kLogCategory, "Install error: %s", Update.errorString());
        return false;
    }

    Logger::info(kLogCategory, "Firmware installed successfully");
    return true;
}

void OtaManager::cleanup() noexcept {
    if (m_http.connected()) {
        m_http.end();
    }
    Update.end();
    mbedtls_sha256_free(&m_sha256Ctx);
    mbedtls_sha256_init(&m_sha256Ctx);
    m_downloadedBytes = 0;
    m_info.downloadProgress = 0;
    m_info.totalBytes = 0;
}

void OtaManager::rebootDevice() noexcept {
    Logger::info(kLogCategory, "Rebooting to apply update...");
    delay(100);  // Allow logs to flush
    ESP.restart();
}

bool OtaManager::parseUpdateInfo(const String& json) noexcept {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Logger::error(kLogCategory, "JSON parse failed: %s", err.c_str());
        return false;
    }

    m_info.latestVersion = doc["version"] | "";
    m_info.firmwareURL = doc["url"] | "";
    m_info.firmwareSize = doc["size"] | 0;
    m_info.mandatory = doc["mandatory"] | false;
    m_info.firmwareSignatureHex = doc["signature"] | "";

    if (m_info.latestVersion.isEmpty()) {
        Logger::error(kLogCategory, "No version in response");
        return false;
    }

    if (!validateVersion(m_info.latestVersion)) {
        m_info.updateAvailable = false;
        Logger::info(kLogCategory, "Version %s not newer than %s",
            m_info.latestVersion.c_str(), m_info.currentVersion.c_str());
        return true;
    }

    if (m_info.firmwareURL.isEmpty()) {
        Logger::error(kLogCategory, "No firmware URL in response");
        return false;
    }

    m_info.updateAvailable = true;
    m_info.timestamp = millis();
    return true;
}

bool OtaManager::validateVersion(const String& latest) const noexcept {
    // Reject if same or older
    if (latest == m_info.currentVersion) return false;
    return versionGreater(latest, m_info.currentVersion);
}

// ========================================================================
// Firmware Signing — SHA-256 + ECDSA P-256 Verification
// ========================================================================

void OtaManager::sha256Init() noexcept {
    mbedtls_sha256_starts(&m_sha256Ctx, 0);
}

void OtaManager::sha256Update(const uint8_t* data, size_t len) noexcept {
    if (data && len > 0) {
        mbedtls_sha256_update(&m_sha256Ctx, data, len);
    }
}

void OtaManager::sha256Final(uint8_t hash[32]) noexcept {
    mbedtls_sha256_finish(&m_sha256Ctx, hash);
}

bool OtaManager::verifyFirmwareSignature(const uint8_t hash[32]) const noexcept {
    // If no signing key is configured, skip verification (insecure fallback)
    if (kFirmwareSigningKeyLen == 0) {
        Logger::warning(kLogCategory, "No firmware signing key — skipping signature verification");
        return true;
    }

    // If server did not provide a signature, reject
    if (m_info.firmwareSignatureHex.isEmpty()) {
        Logger::error(kLogCategory, "Server did not provide a firmware signature");
        return false;
    }

    // Decode hex signature to raw DER bytes
    size_t hexLen = m_info.firmwareSignatureHex.length();
    if (hexLen % 2 != 0) {
        Logger::error(kLogCategory, "Invalid signature hex length");
        return false;
    }
    size_t sigLen = hexLen / 2;
    uint8_t* sigBuf = new (std::nothrow) uint8_t[sigLen];
    if (!sigBuf) {
        Logger::error(kLogCategory, "Failed to allocate signature buffer");
        return false;
    }
    for (size_t i = 0; i < sigLen; ++i) {
        char hi = m_info.firmwareSignatureHex[i * 2];
        char lo = m_info.firmwareSignatureHex[i * 2 + 1];
        uint8_t b = 0;
        if (hi >= '0' && hi <= '9') b = (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') b = (hi - 'a' + 10) << 4;
        else if (hi >= 'A' && hi <= 'F') b = (hi - 'A' + 10) << 4;
        if (lo >= '0' && lo <= '9') b |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') b |= (lo - 'a' + 10);
        else if (lo >= 'A' && lo <= 'F') b |= (lo - 'A' + 10);
        sigBuf[i] = b;
    }

    // Parse the embedded public key
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ret = mbedtls_pk_parse_public_key(&pk, kFirmwareSigningKey, kFirmwareSigningKeyLen);
    if (ret != 0) {
        Logger::error(kLogCategory, "Failed to parse firmware signing key: -0x%04x", static_cast<unsigned>(-ret));
        delete[] sigBuf;
        mbedtls_pk_free(&pk);
        return false;
    }

    // Verify the signature against the SHA-256 hash
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sigBuf, sigLen);
    delete[] sigBuf;
    mbedtls_pk_free(&pk);

    if (ret != 0) {
        Logger::error(kLogCategory, "Firmware signature INVALID: -0x%04x", static_cast<unsigned>(-ret));
        return false;
    }

    Logger::info(kLogCategory, "Firmware signature verified (ECDSA P-256)");
    return true;
}