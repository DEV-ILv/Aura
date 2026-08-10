#include "wifi_manager.h"
#include "config.h"
#include "logger.h"
#include "error_manager.h"
#include <ESPmDNS.h>
#include <inttypes.h>

// Global instance
WifiManager wifiManager;

// Configuration constants
namespace {
  constexpr uint32_t CONNECTION_TIMEOUT_MS = 15000;      // 15 seconds
  constexpr uint32_t RECONNECT_DELAY_MS = 5000;          // 5 seconds
  constexpr uint32_t CONNECTION_CHECK_INTERVAL_MS = 1000; // 1 second
  constexpr uint32_t TIME_SYNC_INTERVAL_MS = 3600000;    // 1 hour
  constexpr uint32_t TIME_SYNC_TIMEOUT_MS = 30000;       // 30 seconds for NTP
  constexpr uint8_t MAX_CONNECTION_ATTEMPTS = 5;
  constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
  constexpr const char* NTP_SERVER_2 = "time.nist.gov";
  constexpr const char* PREFERENCES_NAMESPACE = "aura_wifi";
  constexpr const char* SSID_KEY = "ssid";
  constexpr const char* PASSWORD_KEY = "password";
}

/**
 * @brief Constructor
 */
WifiManager::WifiManager() noexcept
    : m_currentState(WifiState::DISCONNECTED),
      m_connectionTimer(0),
      m_reconnectTimer(0),
      m_lastRSSI(0),
      m_connectionAttempts(0),
      m_lastError(0),
      m_timeSynced(false),
      m_accessPointMode(false),
      m_connecting(false),
      m_mdnsStarted(false),
      m_lastConnectionCheck(0),
      m_lastTimeSyncAttempt(0) {
  memset(m_ssid, 0, sizeof(m_ssid));
  memset(m_password, 0, sizeof(m_password));
  memset(m_hostname, 0, sizeof(m_hostname));
  
  // Set default hostname from MAC address
  snprintf(m_hostname, sizeof(m_hostname), "aura-%06" PRIx32, 
           ESP.getEfuseMac() & 0xFFFFFF);
}

/**
 * @brief Destructor
 */
WifiManager::~WifiManager() noexcept {
  disconnect();
  stopAccessPoint();
  stopMDNS();
}

/**
 * @brief Initialize the Wi-Fi manager
 */
bool WifiManager::initialize() noexcept {
  Logger::info("WifiManager", "Initializing");
  
  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(m_hostname);
  
  // Load stored credentials
  if (!loadCredentials()) {
    Logger::debug("WifiManager", "No stored credentials found");
  }
  
  // Start mDNS
  if (!startMDNS()) {
    Logger::warning("WifiManager", "Failed to start mDNS");
  }
  
  changeState(WifiState::DISCONNECTED);
  resetTimers();
  
  Logger::info("WifiManager", "Initialization complete");
  return true;
}

/**
 * @brief Update Wi-Fi manager state machine
 */
void WifiManager::update() noexcept {
  uint32_t currentMillis = millis();
  
  // Check connection status periodically
  if (currentMillis - m_lastConnectionCheck >= CONNECTION_CHECK_INTERVAL_MS) {
    m_lastConnectionCheck = currentMillis;
    checkConnection();
  }

  // Process WiFi events
  handleEvents();
  
  // Handle state machine
  switch (m_currentState) {
    case WifiState::DISCONNECTED:
      if (m_connectionAttempts > 0 && currentMillis - m_reconnectTimer >= RECONNECT_DELAY_MS) {
        handleReconnect();
      }
      break;
      
    case WifiState::CONNECTING:
      handleConnection();
      break;
      
    case WifiState::CONNECTED:
      // Connection status is checked in checkConnection() above
      break;
      
    case WifiState::ACCESS_POINT:
      // AP mode is stable
      break;
      
    case WifiState::ERROR:
      handleReconnect();
      break;
  }
  
  // Periodic time sync attempt
  if (isConnected() && !m_timeSynced && (currentMillis - m_lastTimeSyncAttempt >= TIME_SYNC_INTERVAL_MS)) {
    syncTime();
  }
}

/**
 * @brief Scheduler alias for update()
 */
void WifiManager::run() noexcept {
  update();
}

/**
 * @brief Attempt to connect to a Wi-Fi network
 */
bool WifiManager::connect(const char* ssid, const char* password) noexcept {
  if (!ssid || strlen(ssid) == 0) {
    Logger::error("WifiManager", "Invalid SSID");
    m_lastError = 1;
    return false;
  }
  
  Logger::info("WifiManager", "Connecting to SSID: %s", ssid);
  
  // Stop AP mode if active
  if (m_accessPointMode) {
    stopAccessPoint();
  }
  
  // Store credentials
  strncpy(m_ssid, ssid, sizeof(m_ssid) - 1);
  m_ssid[sizeof(m_ssid) - 1] = '\0';
  
  if (password) {
    strncpy(m_password, password, sizeof(m_password) - 1);
    m_password[sizeof(m_password) - 1] = '\0';
  } else {
    memset(m_password, 0, sizeof(m_password));
  }

  // Persist credentials to NVS so the device reconnects automatically after
  // a reboot or power loss. Without this, a reboot loses the network and the
  // user is forced to reconfigure via the setup portal.
  saveCredentials(m_ssid, m_password);
  
  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(m_hostname);
  
  // Initiate connection
  if (strlen(m_password) > 0) {
    WiFi.begin(m_ssid, m_password);
  } else {
    WiFi.begin(m_ssid);
  }
  
  changeState(WifiState::CONNECTING);
  m_connectionTimer = millis();
  m_connecting = true;
  m_connectionAttempts = 1;
  m_timeSynced = false;
  
  return true;
}

/**
 * @brief Disconnect from current Wi-Fi network
 */
bool WifiManager::disconnect() noexcept {
  Logger::info("WifiManager", "Disconnecting from Wi-Fi");
  
  WiFi.disconnect(true); // Turn off WiFi radio
  changeState(WifiState::DISCONNECTED);
  m_connecting = false;
  m_connectionAttempts = 0;
  m_timeSynced = false;
  resetTimers();
  
  return true;
}

/**
 * @brief Initiate automatic reconnection
 */
bool WifiManager::reconnect() noexcept {
  if (strlen(m_ssid) == 0) {
    Logger::warning("WifiManager", "No credentials to reconnect");
    m_lastError = 2;
    return false;
  }
  
  Logger::info("WifiManager", "Reconnecting to %s", m_ssid);
  m_connectionAttempts = 0;
  m_reconnectTimer = millis();
  
  return attemptConnection();
}

/**
 * @brief Start Access Point mode for setup
 */
bool WifiManager::startAccessPoint(const char* ssid, const char* password) noexcept {
  if (!ssid || strlen(ssid) == 0) {
    Logger::error("WifiManager", "Invalid AP SSID");
    m_lastError = 3;
    return false;
  }
  
  Logger::info("WifiManager", "Starting Access Point: %s", ssid);
  
  // Disconnect from any existing connection
  WiFi.disconnect(false);
  
  // Set AP mode
  WiFi.mode(WIFI_AP);
  
  bool success;
  if (password && strlen(password) > 0) {
    success = WiFi.softAP(ssid, password);
  } else {
    success = WiFi.softAP(ssid);
  }
  
  if (!success) {
    Logger::error("WifiManager", "Failed to start AP");
    m_lastError = 4;
    return false;
  }
  
  changeState(WifiState::ACCESS_POINT);
  m_accessPointMode = true;
  m_connecting = false;
  
  Logger::info("WifiManager", "AP started at %s", WiFi.softAPIP().toString().c_str());
  
  return true;
}

/**
 * @brief Stop Access Point mode
 */
bool WifiManager::stopAccessPoint() noexcept {
  if (!m_accessPointMode) {
    return true;
  }
  
  Logger::info("WifiManager", "Stopping AP mode");
  
  WiFi.softAPdisconnect(true);
  m_accessPointMode = false;
  changeState(WifiState::DISCONNECTED);
  
  return true;
}

/**
 * @brief Check if device is connected to a Wi-Fi network
 */
bool WifiManager::isConnected() const noexcept {
  return m_currentState == WifiState::CONNECTED && WiFi.isConnected();
}

/**
 * @brief Check if device is in Access Point mode
 */
bool WifiManager::isAccessPointMode() const noexcept {
  return m_accessPointMode;
}

/**
 * @brief Check if device is currently connecting
 */
bool WifiManager::isConnecting() const noexcept {
  return m_connecting || m_currentState == WifiState::CONNECTING;
}

/**
 * @brief Check if stored Wi-Fi credentials exist
 */
bool WifiManager::hasCredentials() const noexcept {
  return strlen(m_ssid) > 0;
}

/**
 * @brief Get current connected SSID
 */
const char* WifiManager::getSSID() const noexcept {
  if (isConnected()) {
    return WiFi.SSID().c_str();
  }
  return m_ssid;
}

/**
 * @brief Get current IP address
 */
IPAddress WifiManager::getIPAddress() const noexcept {
  if (m_accessPointMode) {
    return WiFi.softAPIP();
  }
  return WiFi.localIP();
}

/**
 * @brief Get current Received Signal Strength Indicator
 */
int32_t WifiManager::getRSSI() const noexcept {
  return m_lastRSSI;
}

/**
 * @brief Get device hostname
 */
const char* WifiManager::getHostname() const noexcept {
  return m_hostname;
}

/**
 * @brief Set device hostname
 */
bool WifiManager::setHostname(const char* hostname) noexcept {
  if (!hostname || strlen(hostname) == 0 || strlen(hostname) >= sizeof(m_hostname)) {
    Logger::error("WifiManager", "Invalid hostname");
    m_lastError = 5;
    return false;
  }
  
  strncpy(m_hostname, hostname, sizeof(m_hostname) - 1);
  m_hostname[sizeof(m_hostname) - 1] = '\0';
  
  if (isConnected() || m_accessPointMode) {
    WiFi.setHostname(m_hostname);
  }
  
  Logger::info("WifiManager", "Hostname set to %s", m_hostname);
  
  return true;
}

/**
 * @brief Get current Wi-Fi state
 */
WifiState WifiManager::getState() const noexcept {
  return m_currentState;
}

/**
 * @brief Synchronize time with NTP server
 */
bool WifiManager::syncTime(int32_t timezone) noexcept {
  if (!isConnected()) {
    Logger::debug("WifiManager", "Cannot sync time: not connected");
    return false;
  }
  
  Logger::info("WifiManager", "Syncing time with NTP");
  
  m_lastTimeSyncAttempt = millis();
  
  // Configure NTP
  configTime(timezone, 0, NTP_SERVER_1, NTP_SERVER_2);
  
  return true;
}

/**
 * @brief Check if system time has been synchronized
 */
bool WifiManager::isTimeSynced() const noexcept {
  // Check if we have marked time as synced
  if (m_timeSynced) {
    return true;
  }
  
  // Verify time is reasonable (after 2020)
  time_t now = time(nullptr);
  struct tm timeinfo = *localtime(&now);
  
  return timeinfo.tm_year > 120; // 2020 is year 120 in struct tm
}

/**
 * @brief Load Wi-Fi credentials from persistent storage
 */
bool WifiManager::loadCredentials() noexcept {
  Logger::debug("WifiManager", "Loading credentials");
  
  if (!m_preferences.begin(PREFERENCES_NAMESPACE, true)) {
    Logger::error("WifiManager", "Failed to open preferences");
    m_lastError = 6;
    return false;
  }
  
  size_t ssidLen = m_preferences.getString(SSID_KEY, m_ssid, sizeof(m_ssid));
  m_preferences.getString(PASSWORD_KEY, m_password, sizeof(m_password));
  
  m_preferences.end();
  
  if (ssidLen > 0) {
    Logger::debug("WifiManager", "Credentials loaded");
    return true;
  }
  
  return false;
}

/**
 * @brief Save Wi-Fi credentials to persistent storage
 */
bool WifiManager::saveCredentials(const char* ssid, const char* password) noexcept {
  if (!ssid || strlen(ssid) == 0) {
    Logger::error("WifiManager", "Invalid SSID");
    m_lastError = 7;
    return false;
  }
  
  Logger::info("WifiManager", "Saving credentials");
  
  if (!m_preferences.begin(PREFERENCES_NAMESPACE, false)) {
    Logger::error("WifiManager", "Failed to open preferences for writing");
    m_lastError = 8;
    return false;
  }
  
  m_preferences.putString(SSID_KEY, ssid);
  if (password && strlen(password) > 0) {
    m_preferences.putString(PASSWORD_KEY, password);
  } else {
    m_preferences.putString(PASSWORD_KEY, "");
  }
  
  m_preferences.end();
  
  // Update in-memory credentials
  strncpy(m_ssid, ssid, sizeof(m_ssid) - 1);
  m_ssid[sizeof(m_ssid) - 1] = '\0';
  
  if (password) {
    strncpy(m_password, password, sizeof(m_password) - 1);
    m_password[sizeof(m_password) - 1] = '\0';
  } else {
    memset(m_password, 0, sizeof(m_password));
  }
  
  Logger::debug("WifiManager", "Credentials saved");
  
  return true;
}

/**
 * @brief Clear stored Wi-Fi credentials
 */
bool WifiManager::clearCredentials() noexcept {
  Logger::info("WifiManager", "Clearing credentials");
  
  if (!m_preferences.begin(PREFERENCES_NAMESPACE, false)) {
    Logger::error("WifiManager", "Failed to open preferences");
    m_lastError = 9;
    return false;
  }
  
  m_preferences.remove(SSID_KEY);
  m_preferences.remove(PASSWORD_KEY);
  m_preferences.end();
  
  memset(m_ssid, 0, sizeof(m_ssid));
  memset(m_password, 0, sizeof(m_password));
  
  Logger::debug("WifiManager", "Credentials cleared");
  
  return true;
}

/**
 * @brief Scan for available Wi-Fi networks
 */
int16_t WifiManager::scanNetworks() noexcept {
  Logger::debug("WifiManager", "Scanning networks");
  
  int16_t networkCount = WiFi.scanNetworks();
  
  if (networkCount < 0) {
    Logger::error("WifiManager", "Scan failed");
    m_lastError = 10;
    return -1;
  }
  
  Logger::info("WifiManager", "Found %d networks", networkCount);
  
  return networkCount;
}

/**
 * @brief Get information about a scanned network
 */
bool WifiManager::getNetworkInfo(uint16_t index, char* ssid, size_t ssidLen,
                                  int32_t& rssi, uint8_t& channel, bool& isOpen) const noexcept {
  if (!ssid || ssidLen == 0) {
    return false;
  }
  
  String ssidStr = WiFi.SSID(index);
  if (ssidStr.length() == 0 || ssidStr.length() >= ssidLen) {
    return false;
  }
  
  strncpy(ssid, ssidStr.c_str(), ssidLen - 1);
  ssid[ssidLen - 1] = '\0';
  
  rssi = WiFi.RSSI(index);
  channel = WiFi.channel(index);
  isOpen = (WiFi.encryptionType(index) == WIFI_AUTH_OPEN);
  
  return true;
}

/**
 * @brief Get the last connection error
 */
uint8_t WifiManager::getLastError() const noexcept {
  return m_lastError;
}

// ============================================================================
// PRIVATE IMPLEMENTATION
// ============================================================================

/**
 * @brief Handle connection state
 */
void WifiManager::handleConnection() noexcept {
  uint32_t currentMillis = millis();
  wl_status_t status = WiFi.status();
  
  // Check for timeout
  if (currentMillis - m_connectionTimer > CONNECTION_TIMEOUT_MS) {
    Logger::warning("WifiManager", "Connection timeout");
    m_lastError = static_cast<uint8_t>(status);
    m_connecting = false;
    
    if (m_connectionAttempts < MAX_CONNECTION_ATTEMPTS) {
      m_connectionAttempts++;
      m_reconnectTimer = currentMillis;
      changeState(WifiState::DISCONNECTED);
    } else {
      Logger::error("WifiManager", "Max attempts reached");
      changeState(WifiState::ERROR);
      m_connectionAttempts = 0;
      errorManager.report(AuraErrorSeverity::ERROR, "WiFi", "WIFI_CONN_TIMEOUT",
                          "WiFi connection timed out",
                          "Repeated attempts to join the saved network failed");
    }
    return;
  }
  
  // Check connection status
  if (status == WL_CONNECTED) {
    Logger::info("WifiManager", "Connected to %s", m_ssid);
    changeState(WifiState::CONNECTED);
    m_connecting = false;
    m_connectionAttempts = 0;
    m_lastRSSI = WiFi.RSSI();
    errorManager.resolve("WiFi", "WIFI_CONN_TIMEOUT");
    errorManager.resolve("WiFi", "WIFI_CONN_FAILED");
    errorManager.resolve("WiFi", "WIFI_NO_SSID");
    errorManager.resolve("WiFi", "WIFI_LINK_LOST");
    
    // Attempt NTP sync
    syncTime();
  }
}

/**
 * @brief Handle reconnect logic
 */
void WifiManager::handleReconnect() noexcept {
  uint32_t currentMillis = millis();
  
  if (currentMillis - m_reconnectTimer >= RECONNECT_DELAY_MS) {
    if (m_connectionAttempts < MAX_CONNECTION_ATTEMPTS) {
      attemptConnection();
    } else {
      Logger::error("WifiManager", "Reconnect failed");
      changeState(WifiState::DISCONNECTED);
      m_connectionAttempts = 0;
    }
  }
}

/**
 * @brief Handle Wi-Fi events
 */
void WifiManager::handleEvents() noexcept {
    wl_status_t status = WiFi.status();

    switch (status) {
        case WL_CONNECTED:
            if (m_currentState != WifiState::CONNECTED && !m_accessPointMode) {
                Logger::info("WifiManager", "WiFi connected to %s", WiFi.SSID().c_str());
                changeState(WifiState::CONNECTED);
                m_connectionAttempts = 0;
                m_lastRSSI = WiFi.RSSI();
                errorManager.resolve("WiFi", "WIFI_CONN_TIMEOUT");
                errorManager.resolve("WiFi", "WIFI_CONN_FAILED");
                errorManager.resolve("WiFi", "WIFI_NO_SSID");
                errorManager.resolve("WiFi", "WIFI_LINK_LOST");
            }
            break;

        case WL_DISCONNECTED:
            if (m_currentState == WifiState::CONNECTED) {
                Logger::warning("WifiManager", "WiFi connection lost");
                changeState(WifiState::DISCONNECTED);
                m_reconnectTimer = millis();
                errorManager.report(AuraErrorSeverity::WARNING, "WiFi", "WIFI_LINK_LOST",
                                    "WiFi connection lost",
                                    "The link dropped; the device will attempt to reconnect");
            }
            break;

        case WL_CONNECT_FAILED:
            Logger::error("WifiManager", "WiFi connection failed");
            changeState(WifiState::ERROR);
            errorManager.report(AuraErrorSeverity::ERROR, "WiFi", "WIFI_CONN_FAILED",
                                "WiFi connection failed",
                                "The network rejected the connection attempt");
            break;

        case WL_NO_SSID_AVAIL:
            Logger::warning("WifiManager", "WiFi SSID not available");
            changeState(WifiState::ERROR);
            errorManager.report(AuraErrorSeverity::WARNING, "WiFi", "WIFI_NO_SSID",
                                "WiFi network unavailable",
                                "The saved network SSID is not in range");
            break;

        default:
            break;
    }
}

/**
 * @brief Check connection status
 */
void WifiManager::checkConnection() noexcept {
  if (m_accessPointMode) {
    return;
  }
  
  wl_status_t status = WiFi.status();
  
  if (status == WL_CONNECTED) {
    if (m_currentState != WifiState::CONNECTED) {
      changeState(WifiState::CONNECTED);
      errorManager.resolve("WiFi", "WIFI_CONN_TIMEOUT");
      errorManager.resolve("WiFi", "WIFI_CONN_FAILED");
      errorManager.resolve("WiFi", "WIFI_NO_SSID");
      errorManager.resolve("WiFi", "WIFI_LINK_LOST");
    }
    // Update RSSI for connected state
    m_lastRSSI = WiFi.RSSI();
  } else {
    if (m_currentState == WifiState::CONNECTED) {
      Logger::warning("WifiManager", "Connection lost");
      changeState(WifiState::DISCONNECTED);
      m_connectionAttempts = 0;
      m_reconnectTimer = millis();
      m_timeSynced = false;
      errorManager.report(AuraErrorSeverity::WARNING, "WiFi", "WIFI_LINK_LOST",
                          "WiFi connection lost",
                          "The link dropped; the device will attempt to reconnect");
    }
  }
}

/**
 * @brief Attempt connection
 */
bool WifiManager::attemptConnection() noexcept {
  if (strlen(m_ssid) == 0) {
    Logger::error("WifiManager", "No SSID to connect");
    m_lastError = 11;
    return false;
  }
  
  Logger::debug("WifiManager", "Attempting connection");
  
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(m_hostname);
  
  if (strlen(m_password) > 0) {
    WiFi.begin(m_ssid, m_password);
  } else {
    WiFi.begin(m_ssid);
  }
  
  changeState(WifiState::CONNECTING);
  m_connectionTimer = millis();
  m_connecting = true;
  m_connectionAttempts++;
  m_timeSynced = false;
  
  return true;
}

/**
 * @brief Start mDNS service
 */
bool WifiManager::startMDNS() noexcept {
  if (m_mdnsStarted) {
    return true;
  }
  
  if (!MDNS.begin(m_hostname)) {
    Logger::warning("WifiManager", "Failed to start mDNS");
    return false;
  }
  
  m_mdnsStarted = true;
  Logger::debug("WifiManager", "mDNS started");
  
  return true;
}

/**
 * @brief Stop mDNS service
 */
bool WifiManager::stopMDNS() noexcept {
  if (!m_mdnsStarted) {
    return true;
  }
  
  MDNS.end();
  m_mdnsStarted = false;
  
  return true;
}

/**
 * @brief Initialize NTP
 */
bool WifiManager::initializeNTP(int32_t timezone) noexcept {
  if (!isConnected()) {
    Logger::debug("WifiManager", "Cannot init NTP: not connected");
    return false;
  }
  
  configTime(timezone, 0, NTP_SERVER_1, NTP_SERVER_2);
  
  return true;
}

/**
 * @brief Reset timers
 */
void WifiManager::resetTimers() noexcept {
  m_connectionTimer = millis();
  m_reconnectTimer = millis();
  m_lastConnectionCheck = millis();
}

/**
 * @brief Change state
 */
void WifiManager::changeState(WifiState newState) noexcept {
  if (m_currentState != newState) {
    m_currentState = newState;
  }
}