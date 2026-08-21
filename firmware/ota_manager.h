#ifndef AURA_OTA_MANAGER_H
#define AURA_OTA_MANAGER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pk.h>
#include "wifi_manager.h"
#include "storage_manager.h"
#include "web_portal.h"
#include "config.h"
#include "logger.h"
#include "firmware_keys.h"

/**
 * @enum OTAState
 * @brief OTA manager operational states
 */
enum class OTAState : uint8_t {
    IDLE,           ///< No OTA operation in progress
    CHECKING,       ///< Checking for updates
    DOWNLOADING,    ///< Downloading firmware
    VERIFYING,      ///< Verifying firmware integrity
    INSTALLING,     ///< Installing firmware
    REBOOT_PENDING, ///< Installation complete, awaiting reboot
    COMPLETED,      ///< Update completed successfully
    ERROR           ///< Error occurred
};

/**
 * @enum OTAError
 * @brief OTA manager error codes
 */
enum class OTAError : uint8_t {
    NONE,           ///< No error
    NO_WIFI,        ///< WiFi not connected
    NETWORK,        ///< Network error
    DOWNLOAD,       ///< Download failed
    VERIFY,         ///< Verification failed
    INSTALL,        ///< Installation failed
    AUTHENTICATION, ///< Server authentication failed
    INVALID_VERSION,///< Version validation failed
    TIMEOUT,        ///< Operation timed out
    TLS_ERROR,      ///< TLS certificate validation failed
    UNKNOWN         ///< Unspecified error
};

/**
 * @struct OTAInfo
 * @brief OTA update information
 */
struct OTAInfo {
    String currentVersion;            ///< Current firmware version
    String latestVersion;             ///< Latest available version
    String firmwareURL;               ///< Firmware download URL
    size_t firmwareSize;              ///< Firmware size in bytes
    bool updateAvailable;             ///< Update available flag
    bool mandatory;                   ///< Mandatory update flag
    unsigned long downloadProgress;   ///< Download progress in bytes
    unsigned long totalBytes;         ///< Total firmware size in bytes
    String firmwareSignatureHex;      ///< Hex-encoded DER ECDSA signature (from server)
    unsigned long timestamp;          ///< Last check timestamp (millis)

    OTAInfo() noexcept
        : currentVersion(""), latestVersion(""), firmwareURL(""),
          firmwareSize(0), updateAvailable(false), mandatory(false),
          downloadProgress(0), totalBytes(0), firmwareSignatureHex(""),
          timestamp(0) {}
};

/**
 * @class OtaManager
 * @brief Single authority for Over-The-Air firmware updates
 *
 * Manages:
 * - Secure HTTPS firmware downloads with certificate validation
 * - Version checking and validation (rejects downgrades)
 * - Firmware integrity verification before installation
 * - Safe installation using ESP32 Update library
 * - Automatic and manual update modes
 * - Progress tracking and state management
 * - Non-blocking operation with millis()
 *
 * Thread-safe for main loop access. ESP32-optimized.
 */
class OtaManager {
public:
    /**
     * @brief Constructor
     */
    OtaManager() noexcept;

    /**
     * @brief Destructor
     */
    ~OtaManager() noexcept;

    // Delete copy semantics
    OtaManager(const OtaManager&) = delete;
    OtaManager& operator=(const OtaManager&) = delete;

    // Delete move semantics
    OtaManager(OtaManager&&) = delete;
    OtaManager& operator=(OtaManager&&) = delete;

    /**
     * @brief Initialize OTA manager
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
     * @brief Update OTA manager state machine
     * @note Should be called regularly from loop()
     */
    void update() noexcept;

    /**
     * @brief Check for available updates
     * @return true if check initiated, false otherwise
     */
    [[nodiscard]] bool checkForUpdates() noexcept;

    /**
     * @brief Start firmware update
     * @return true if update started, false otherwise
     * @note Requires checkForUpdates() to have found an update
     */
    [[nodiscard]] bool startUpdate() noexcept;

    /**
     * @brief Cancel current update operation
     */
    void cancelUpdate() noexcept;

    /**
     * @brief Schedule automatic update check
     * @note Triggers checkForUpdates() on next interval
     */
    void scheduleUpdate() noexcept;

    /**
     * @brief Enable automatic update checks
     */
    void enableAutoUpdate() noexcept;

    /**
     * @brief Disable automatic update checks
     */
    void disableAutoUpdate() noexcept;

    /**
     * @brief Set update server URL
     * @param url Base URL for update server (e.g., "https://api.example.com")
     */
    void setUpdateServer(const String& url) noexcept;

    /**
     * @brief Set root CA certificate for HTTPS validation
     * @param caCert PEM-formatted certificate
     */
    void setRootCA(const String& caCert) noexcept;

    /**
     * @brief Set basic authentication credentials
     * @param username Username
     * @param password Password
     */
    void setAuthentication(const String& username, const String& password) noexcept;

    /**
     * @brief Set automatic check interval
     * @param intervalMs Interval in milliseconds (default: 1 hour)
     */
    void setCheckInterval(unsigned long intervalMs) noexcept;

    /**
     * @brief Check if manager is busy with update operation
     * @return true if not IDLE or COMPLETED
     */
    [[nodiscard]] bool isBusy() const noexcept;

    /**
     * @brief Check if currently downloading/installing
     * @return true if DOWNLOADING, VERIFYING, or INSTALLING
     */
    [[nodiscard]] bool isUpdating() const noexcept;

    /**
     * @brief Check if manager is initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Check if update is available
     * @return true if update available
     */
    [[nodiscard]] bool isUpdateAvailable() const noexcept;

    /**
     * @brief Get download progress percentage
     * @return Progress 0-100
     */
    [[nodiscard]] uint8_t getProgress() const noexcept;

    /**
     * @brief Get current update information
     * @return Const reference to OTAInfo
     */
    [[nodiscard]] const OTAInfo& getInfo() const noexcept;

    /**
     * @brief Get current state
     * @return Current OTAState
     */
    [[nodiscard]] OTAState getState() const noexcept;

    /**
     * @brief Get last error code
     * @return Current OTAError
     */
    [[nodiscard]] OTAError getError() const noexcept;

private:
    // Constants
    static constexpr size_t kChunkSize = 4096U;
    static constexpr unsigned long kDownloadTimeoutMs = 30000UL;
    static constexpr unsigned long kCheckIntervalMs = 3600000UL;  // 1 hour
    static constexpr const char* kLogCategory = "OtaManager";

    // Private methods
    void changeState(OTAState newState) noexcept;
    void setError(OTAError error) noexcept;

    bool connectServer() noexcept;
    bool downloadFirmware() noexcept;
    bool verifyFirmware() noexcept;
    bool installFirmware() noexcept;
    void cleanup() noexcept;
    void rebootDevice() noexcept;

    bool parseUpdateInfo(const String& json) noexcept;
    bool validateVersion(const String& latest) const noexcept;

    // Firmware signing
    bool verifyFirmwareSignature(const uint8_t hash[32]) const noexcept;
    void sha256Init() noexcept;
    void sha256Update(const uint8_t* data, size_t len) noexcept;
    void sha256Final(uint8_t hash[32]) noexcept;

    // Member variables
    bool m_initialized;
    OTAState m_currentState;
    OTAError m_lastError;

    bool m_autoUpdateEnabled;
    unsigned long m_checkIntervalMs;
    unsigned long m_lastCheckTime;
    unsigned long m_stateStartTime;
    size_t m_downloadedBytes;
    uint8_t m_lastLoggedProgress;

    String m_updateServer;
    String m_authUsername;
    String m_authPassword;
    String m_rootCA;

    WiFiClientSecure m_client;
    HTTPClient m_http;

    mbedtls_sha256_context m_sha256Ctx;
    OTAInfo m_info;
};

/**
 * @brief Global OTA manager instance
 */
extern OtaManager otaManager;

#endif // AURA_OTA_MANAGER_H