#ifndef AURA_WIFI_MANAGER_H
#define AURA_WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <IPAddress.h>


#include "config.h"
#include "logger.h"

/**
 * @enum WifiState
 * @brief Enumeration of Wi-Fi connection states
 */
enum class WifiState : uint8_t {
  DISCONNECTED,  ///< Wi-Fi is disconnected
  CONNECTING,    ///< Wi-Fi is attempting to connect
  CONNECTED,     ///< Wi-Fi is connected
  ACCESS_POINT,  ///< In Access Point mode
  ERROR          ///< Error state
};

/**
 * @class WifiManager
 * @brief Manages all Wi-Fi functionality for AURA AI Desktop Assistant
 * 
 * This class serves as the single authority for:
 * - Wi-Fi connection and disconnection
 * - Auto-reconnection logic
 * - Connection monitoring and state management
 * - Wi-Fi credentials storage and retrieval
 * - Access Point setup mode
 * - NTP time synchronization
 * - Network information retrieval
 * - Hostname and mDNS management
 * 
 * Non-blocking and ESP32-optimized for production use.
 */
class WifiManager {
public:
  /**
   * @brief Constructor
   */
  WifiManager() noexcept;

  /**
   * @brief Destructor
   */
  ~WifiManager() noexcept;

  // Delete copy semantics
  WifiManager(const WifiManager&) = delete;
  WifiManager& operator=(const WifiManager&) = delete;

  // Delete move semantics
  WifiManager(WifiManager&&) = delete;
  WifiManager& operator=(WifiManager&&) = delete;

  /**
   * @brief Initialize the Wi-Fi manager
   * @return true if initialization successful, false otherwise
   * @note Should be called once during setup()
   */
  [[nodiscard]] bool initialize() noexcept;

  /**
   * @brief Update Wi-Fi manager state machine
   * @note Should be called regularly from loop()
   */
  void update() noexcept;

  /**
   * @brief Scheduler alias for update()
   * @note Provided for compatibility with task schedulers
   */
  void run() noexcept;

  /**
   * @brief Attempt to connect to a Wi-Fi network
   * @param ssid Network SSID
   * @param password Network password
   * @return true if connection initiated, false otherwise
   */
  [[nodiscard]] bool connect(const char* ssid, const char* password) noexcept;

  /**
   * @brief Disconnect from current Wi-Fi network
   * @return true if disconnection successful, false otherwise
   */
  [[nodiscard]] bool disconnect() noexcept;

  /**
   * @brief Initiate automatic reconnection
   * @return true if reconnection initiated, false otherwise
   */
  [[nodiscard]] bool reconnect() noexcept;

  /**
   * @brief Start Access Point mode for setup
   * @param ssid Access Point SSID
   * @param password Access Point password (optional)
   * @return true if Access Point started successfully, false otherwise
   */
  [[nodiscard]] bool startAccessPoint(const char* ssid, const char* password = nullptr) noexcept;

  /**
   * @brief Stop Access Point mode
   * @return true if Access Point stopped successfully, false otherwise
   */
  [[nodiscard]] bool stopAccessPoint() noexcept;

  /**
   * @brief Check if device is connected to a Wi-Fi network
   * @return true if connected, false otherwise
   */
  [[nodiscard]] bool isConnected() const noexcept;

  /**
   * @brief Check if device is in Access Point mode
   * @return true if in Access Point mode, false otherwise
   */
  [[nodiscard]] bool isAccessPointMode() const noexcept;

  /**
   * @brief Check if device is currently connecting
   * @return true if connecting, false otherwise
   */
  [[nodiscard]] bool isConnecting() const noexcept;

  /**
   * @brief Check if stored Wi-Fi credentials exist
   * @return true if credentials are available, false otherwise
   */
  [[nodiscard]] bool hasCredentials() const noexcept;

  /**
   * @brief Get current connected SSID
   * @return Pointer to SSID string, or nullptr if not connected
   */
  [[nodiscard]] const char* getSSID() const noexcept;

  /**
   * @brief Get current IP address
   * @return IPAddress object representing current IP, or 0.0.0.0 if not connected
   */
  [[nodiscard]] IPAddress getIPAddress() const noexcept;

  /**
   * @brief Get current Received Signal Strength Indicator
   * @return RSSI value in dBm, or 0 if not connected
   */
  [[nodiscard]] int32_t getRSSI() const noexcept;

  /**
   * @brief Get device hostname
   * @return Pointer to hostname string
   */
  [[nodiscard]] const char* getHostname() const noexcept;

  /**
   * @brief Set device hostname
   * @param hostname New hostname to set
   * @return true if hostname set successfully, false otherwise
   */
  [[nodiscard]] bool setHostname(const char* hostname) noexcept;

  /**
   * @brief Get current Wi-Fi state
   * @return Current WifiState value
   */
  [[nodiscard]] WifiState getState() const noexcept;

  /**
   * @brief Truthful human-readable label for the current Wi-Fi state.
   *
   * Reflects the actual radio condition rather than a stale/default label:
   *   "SETUP_AP"       - Access Point active (AURA_Setup portal)
   *   "AP_STA"         - Access Point active AND station connected
   *   "STA_CONNECTING" - attempting to join a saved network
   *   "STA_CONNECTED"  - station joined a network
   *   "DISCONNECTED"   - radio up, no link
   *   "ERROR"          - bounded-retry backoff state
   * @return Stable C string (never null).
   */
  [[nodiscard]] const char* getStateString() const noexcept;

  /**
   * @brief Get the active Wi-Fi channel.
   * @return Channel of the connected STA, the SoftAP channel while in AP mode,
   *         or 0 when the radio is not active.
   * @note Single-authority accessor: ESP-NOW and diagnostics must read the
   *       channel from here instead of calling WiFi.channel() themselves.
   */
  [[nodiscard]] uint8_t getChannel() const noexcept;

  /**
   * @brief Get the number of connection attempts made since boot.
   * @return Monotonic reconnect/connect attempt counter.
   */
  [[nodiscard]] uint16_t getReconnectCount() const noexcept;

  /**
   * @brief Get the last Wi-Fi event code observed.
   * @return wl_status_t of the most recent Wi-Fi link event.
   */
  [[nodiscard]] wl_status_t getLastEvent() const noexcept;

  /**
   * @brief Get the Wi-Fi state persisted at the previous shutdown/restart.
   * @return Persisted WifiState (byte), or DISCONNECTED when unavailable.
   */
  [[nodiscard]] WifiState getLastPersistedState() const noexcept;

  /**
   * @brief Get the reconnect counter persisted at the previous shutdown/restart.
   * @return Reconnect/connect attempt count from the previous session.
   */
  [[nodiscard]] uint16_t getLastPersistedReconnectCount() const noexcept;

  /**
   * @brief Synchronize time with NTP server
   * @param timezone Timezone offset in seconds from UTC
   * @return true if time synchronization initiated, false otherwise
   */
  [[nodiscard]] bool syncTime(int32_t timezone = 0) noexcept;

  /**
   * @brief Check if system time has been synchronized
   * @return true if time is synchronized, false otherwise
   */
  [[nodiscard]] bool isTimeSynced() const noexcept;

  /**
   * @brief Load Wi-Fi credentials from persistent storage
   * @return true if credentials loaded successfully, false otherwise
   */
  [[nodiscard]] bool loadCredentials() noexcept;

  /**
   * @brief Save Wi-Fi credentials to persistent storage
   * @param ssid Network SSID to save
   * @param password Network password to save
   * @return true if credentials saved successfully, false otherwise
   */
  [[nodiscard]] bool saveCredentials(const char* ssid, const char* password) noexcept;

  /**
   * @brief Clear stored Wi-Fi credentials
   * @return true if credentials cleared successfully, false otherwise
   */
  [[nodiscard]] bool clearCredentials() noexcept;

  /**
   * @brief Scan for available Wi-Fi networks
   * @return Number of networks found, or -1 on error
   * @note Results can be accessed via getNetworkInfo()
   */
  [[nodiscard]] int16_t scanNetworks() noexcept;

  /**
   * @brief Get information about a scanned network
   * @param index Index of network in scan results
   * @param ssid Output buffer for SSID
   * @param ssidLen Length of SSID buffer
   * @param rssi Output parameter for RSSI value
   * @param channel Output parameter for channel number
   * @param isOpen Output parameter for open network flag
   * @return true if network info retrieved, false on error
   */
  [[nodiscard]] bool getNetworkInfo(uint16_t index, char* ssid, size_t ssidLen,
                                     int32_t& rssi, uint8_t& channel, bool& isOpen) const noexcept;

  /**
   * @brief Get the last connection error
   * @return Error code from last connection attempt
   */
  [[nodiscard]] uint8_t getLastError() const noexcept;

private:
  // Private helper methods
  void handleConnection() noexcept;
  void handleReconnect() noexcept;
  void handleEvents() noexcept;
  void checkConnection() noexcept;
  bool attemptConnection() noexcept;
  bool startMDNS() noexcept;
  bool stopMDNS() noexcept;
  bool initializeNTP(int32_t timezone) noexcept;
  void resetTimers() noexcept;
  void changeState(WifiState newState) noexcept;
  void ensureMode(wifi_mode_t mode) noexcept;
  void persistDiagnostics() noexcept;
  void loadDiagnostics() noexcept;

  // Private member variables
  WifiState m_currentState;
  char m_ssid[33];                    ///< Max SSID length + null terminator
  char m_password[64];                ///< Max password length + null terminator
  char m_hostname[32];                ///< Device hostname
  uint32_t m_connectionTimer;         ///< Connection timeout timer
  uint32_t m_reconnectTimer;          ///< Reconnection attempt timer
  int32_t m_lastRSSI;                 ///< Last RSSI value
  uint8_t m_connectionAttempts;       ///< Current connection attempt count
  uint8_t m_lastError;                ///< Last connection error code
  Preferences m_preferences;          ///< Persistent storage
  bool m_timeSynced;                  ///< Time synchronization flag
  bool m_accessPointMode;             ///< Access Point mode flag
  bool m_connecting;                  ///< Connection in progress flag
  bool m_mdnsStarted;                 ///< mDNS service started flag
  uint32_t m_lastConnectionCheck;     ///< Timestamp of last connection check
  uint32_t m_lastTimeSyncAttempt;     ///< Timestamp of last time sync attempt
  uint16_t m_reconnectCount;          ///< Total connection attempts since boot
  uint8_t m_currentChannel;           ///< Active channel (STA or SoftAP)
  wl_status_t m_lastEvent;            ///< Last Wi-Fi link event observed
  WifiState m_lastPersistedState;     ///< Wi-Fi state persisted at shutdown
  uint16_t m_lastPersistedReconnect;  ///< Reconnect count persisted at shutdown
};

/**
 * @brief Global Wi-Fi manager instance.
 */
extern WifiManager wifiManager;

#endif // AURA_WIFI_MANAGER_H