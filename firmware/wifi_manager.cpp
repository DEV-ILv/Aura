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
  constexpr uint32_t ERROR_RETRY_DELAY_MS = 30000;       // 30s backoff after budget exhausted
  constexpr uint32_t CONNECTION_CHECK_INTERVAL_MS = 1000; // 1 second
  constexpr uint32_t TIME_SYNC_INTERVAL_MS = 3600000;    // 1 hour
  constexpr uint32_t TIME_SYNC_TIMEOUT_MS = 30000;       // 30 seconds for NTP
  constexpr uint32_t WAIT_CONNECT_DELAY_MS = 5000;       // 5 seconds wait before connect
  constexpr uint8_t MAX_CONNECTION_ATTEMPTS = 5;
  constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
  constexpr const char* NTP_SERVER_2 = "time.nist.gov";
  constexpr const char* PREFERENCES_NAMESPACE = "aura_wifi";
  constexpr const char* SSID_KEY = "ssid";
  constexpr const char* PASSWORD_KEY = "password";
  constexpr const char* LAST_STATE_KEY = "last_state";
  constexpr const char* LAST_RECONNECT_KEY = "last_reconnect";
}

/**
 * @brief Constructor
 */
WifiManager::WifiManager() noexcept
    : m_currentState(WifiState::DISCONNECTED),
      m_connectionTimer(0),
      m_reconnectTimer(0),
      m_waitConnectTimer(0),
      m_lastRSSI(0),
      m_connectionAttempts(0),
      m_lastError(0),
      m_timeSynced(false),
      m_accessPointMode(false),
      m_connecting(false),
      m_mdnsStarted(false),
      m_provisioning(false),
      m_lastConnectionCheck(0),
      m_lastTimeSyncAttempt(0),
      m_reconnectCount(0),
      m_currentChannel(0),
      m_lastEvent(WL_IDLE_STATUS),
      m_lastPersistedState(WifiState::DISCONNECTED),
      m_lastPersistedReconnect(0),
      m_scanState(ScanState::IDLE),
      m_scanResultCount(0),
      m_scanStartedMs(0),
      m_scanDurationMs(0) {
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
  ensureMode(WIFI_STA);
  WiFi.setHostname(m_hostname);

  // Event-driven diagnostics (non-flooding): log AP station connect/disconnect
  // so the setup portal and companion app can be traced. Read-only; never
  // mutates the radio.
  WiFi.onEvent([](arduino_event_t* event) {
    switch (event->event_id) {
      case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Logger::info("WifiManager", "AP station connected");
        break;
      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Logger::info("WifiManager", "AP station disconnected");
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        // IP address changed (initial connect or DHCP renewal) -> mDNS auto-updates
        Logger::info("WifiManager", "STA got IP: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
        break;
      default:
        break;
    }
  });
  
  // Load stored credentials
  if (!loadCredentials()) {
    Logger::debug("WifiManager", "No stored credentials found");
  }
  
  // Load persisted diagnostics (last Wi-Fi state/reconnect count from the
  // previous session) so a restart can report what the radio was doing.
  loadDiagnostics();
  
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

  // mDNS is handled automatically by the ESP-IDF stack; no periodic update needed.
  
  // Process WiFi events
  handleEvents();
  
  // Advance the asynchronous scan job (non-blocking)
  pollScan();
  
  // Handle state machine
  switch (m_currentState) {
    case WifiState::DISCONNECTED:
      if (m_connectionAttempts > 0 && currentMillis - m_reconnectTimer >= RECONNECT_DELAY_MS) {
        handleReconnect();
      }
      break;
      
    case WifiState::WAITING_TO_CONNECT:
      // Wait for 5 seconds before attempting connection (non-blocking)
      if (m_provisioning && currentMillis - m_waitConnectTimer >= WAIT_CONNECT_DELAY_MS) {
        Logger::info("WifiManager", "Wait complete, initiating connection to %s", m_ssid);
        m_provisioning = false;
        m_connectionAttempts = 0;  // Reset attempts for fresh connection
        attemptConnection();
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
      // Bounded-rate recovery owned by WifiManager: retry the saved network at
      // the slow backoff first so the device recovers automatically when the
      // network returns (e.g. a router restart). Only after the per-cycle
      // attempt budget is exhausted do we fall back to AP mode so the user can
      // re-provision without a power cycle. attemptConnection() guards
      // WiFi.mode() (ensureMode) so the radio is not re-initialized.
      if (currentMillis - m_reconnectTimer >= ERROR_RETRY_DELAY_MS) {
        if (strlen(m_ssid) > 0 && m_connectionAttempts < MAX_CONNECTION_ATTEMPTS) {
          // Still have attempt budget, try connecting again
          m_reconnectTimer = currentMillis;
          attemptConnection();
        } else if (strlen(m_ssid) > 0) {
          // Attempt budget exhausted: fall back to AP mode for re-provisioning
          Logger::info("WifiManager", "Error state retry exhausted, starting AP mode for re-provisioning");
          startAccessPoint(Secrets::AP_SSID, nullptr);
          changeState(WifiState::ACCESS_POINT);
        } else {
          // No credentials: stay in ERROR and retry at the slow backoff
          m_reconnectTimer = currentMillis;
          attemptConnection();
        }
      }
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
  ensureMode(WIFI_STA);
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
  m_reconnectCount++;
  m_timeSynced = false;
  
  return true;
}

/**
 * @brief Start waiting-to-connect state for provisioning
 * 
 * Enters WAITING_TO_CONNECT state where credentials are saved and a 5-second
 * non-blocking delay occurs before the actual connection attempt. This allows
 * the Flutter app to show a "Connecting..." state before the ESP32 reboots
 * its radio.
 * @param ssid Network SSID
 * @param password Network password
 * @return true if waiting state initiated, false otherwise
 */
bool WifiManager::startWaitingToConnect(const char* ssid, const char* password) noexcept {
  if (!ssid || strlen(ssid) == 0) {
    Logger::error("WifiManager", "Invalid SSID for waiting connect");
    m_lastError = 1;
    return false;
  }
  
  Logger::info("WifiManager", "Provisioning: waiting 5s before connect to %s", ssid);
  
  // Stop AP mode if active
  if (m_accessPointMode) {
    stopAccessPoint();
  }
  
  // Store credentials in memory
  strncpy(m_ssid, ssid, sizeof(m_ssid) - 1);
  m_ssid[sizeof(m_ssid) - 1] = '\0';
  
  if (password) {
    strncpy(m_password, password, sizeof(m_password) - 1);
    m_password[sizeof(m_password) - 1] = '\0';
  } else {
    memset(m_password, 0, sizeof(m_password));
  }
  
  // Persist credentials to NVS immediately so they survive reboot even if
  // the connection attempt fails or the device is power-cycled during the wait.
  saveCredentials(m_ssid, m_password);
  
  // Enter waiting state - the actual connection will be attempted after the delay
  m_provisioning = true;
  m_waitConnectTimer = millis();
  changeState(WifiState::WAITING_TO_CONNECT);
  
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
  m_provisioning = false;  // abort any in-flight 5s provisioning wait
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

  // WifiManager owns reconnection through its bounded state machine
  // (CONNECTING -> DISCONNECTED backoff -> ERROR bounded-rate retry). External
  // triggers (health tick, resilience check, low-power exit) must not fight
  // that machine: forcing reconnect() while a bounded attempt is in flight (or
  // a backoff is pending) re-issues WiFi.begin() — repeated begin()
  // re-initializes the radio and is a documented crash vector on ESP32 — and
  // resetting the attempt budget would starve the AP-fallback path, bypass the
  // 5s provisioning wait, or tear down the setup AP. Only an idle radio (no
  // pending retry) is a true fresh-start.
  const WifiState s = m_currentState;
  if (s == WifiState::CONNECTING || s == WifiState::WAITING_TO_CONNECT ||
      s == WifiState::ACCESS_POINT || s == WifiState::ERROR ||
      (s == WifiState::DISCONNECTED && m_connectionAttempts > 0)) {
    Logger::debug("WifiManager", "Reconnect requested while the state machine is managing recovery; ignoring duplicate");
    return true;
  }
  
  Logger::info("WifiManager", "Reconnecting to %s", m_ssid);
  m_connectionAttempts = 0;
  m_reconnectTimer = millis();
  
  return attemptConnection();
}

/**
 * @brief Resolve the effective AP password for setup mode.
 *
 * Single source of truth for the setup-AP password: Secrets::AP_PASSWORD when
 * set; otherwise a development-only default in dev builds, or a MAC-derived
 * password in production. The MAC derivation mirrors the boot path so every AP
 * startup path uses identical credentials.
 *
 * @return The effective AP password (never empty).
 */
String WifiManager::getAccessPointPassword() const noexcept {
  if (Secrets::AP_PASSWORD[0] != '\0') {
    return String(Secrets::AP_PASSWORD);
  }
#if AURA_DEVELOPMENT_MODE
  return String(AURA_DEV_AP_PASSWORD);
#else
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "aura-%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
#endif
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

  // Centralized password selection: an empty password would otherwise open an
  // unsecured AP via WiFi.softAP(ssid). Every AP startup path resolves the
  // effective password through getAccessPointPassword().
  String resolvedPassword;
  if (password == nullptr || password[0] == '\0') {
    resolvedPassword = getAccessPointPassword();
    password = resolvedPassword.c_str();
  }

  Logger::info("WifiManager", "Starting Access Point: %s", ssid);
  
  // Disconnect from any existing connection
  WiFi.disconnect(false);
  
  // Set AP mode
  ensureMode(WIFI_AP);
  
  bool success;
  if (password && strlen(password) > 0) {
    success = WiFi.softAP(ssid, password);
  } else {
    success = WiFi.softAP(ssid);
  }
  
  if (!success) {
    Logger::error("WifiManager", "Failed to start AP");
    m_lastError = 4;
    errorManager.report(AuraErrorSeverity::ERROR, "WiFi", "WIFI_AP_START_FAIL",
                        "Access point failed to start",
                        "The setup AP could not be brought up on the radio");
    return false;
  }
  
  changeState(WifiState::ACCESS_POINT);
  m_accessPointMode = true;
  m_connecting = false;
  m_currentChannel = 1;  // SoftAP default channel (no forced override)
  
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
  // WiFi.SSID() returns a temporary String; .c_str() on it would dangle.
  // m_ssid holds the currently connected/configured SSID, which is stable.
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
 * @brief Get the active Wi-Fi channel (single authority).
 */
uint8_t WifiManager::getChannel() const noexcept {
  return m_currentChannel;
}

/**
 * @brief Get the number of connection attempts made since boot.
 */
uint16_t WifiManager::getReconnectCount() const noexcept {
  return m_reconnectCount;
}

/**
 * @brief Truthful human-readable label for the current Wi-Fi state.
 */
const char* WifiManager::getStateString() const noexcept {
  if (m_accessPointMode) {
    return WiFi.isConnected() ? "AP_STA" : "SETUP_AP";
  }
  switch (m_currentState) {
    case WifiState::CONNECTING:        return "STA_CONNECTING";
    case WifiState::WAITING_TO_CONNECT: return "WAITING_TO_CONNECT";
    case WifiState::CONNECTED:         return "STA_CONNECTED";
    case WifiState::ERROR:             return "ERROR";
    case WifiState::ACCESS_POINT:      return "SETUP_AP";
    case WifiState::DISCONNECTED:
    default:                           return "DISCONNECTED";
  }
}

/**
 * @brief Get the last Wi-Fi event code observed.
 */
wl_status_t WifiManager::getLastEvent() const noexcept {
  return m_lastEvent;
}

/**
 * @brief Get the Wi-Fi state persisted at the previous shutdown/restart.
 */
WifiState WifiManager::getLastPersistedState() const noexcept {
  return m_lastPersistedState;
}

/**
 * @brief Get the reconnect counter persisted at the previous shutdown/restart.
 */
uint16_t WifiManager::getLastPersistedReconnectCount() const noexcept {
  return m_lastPersistedReconnect;
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
 * @deprecated Blocking; prefer startScan() + getScanState().
 */
int16_t WifiManager::scanNetworks() noexcept {
  Logger::debug("WifiManager", "Scanning networks (legacy blocking path)");
  
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
 * @brief Start an asynchronous Wi-Fi scan.
 *
 * Uses the driver's async scan mode so the calling loop never blocks: the
 * scan runs in the driver and results are collected via pollScan() inside
 * update(). Scans are serialized — a second start while one is running is
 * refused so two scan jobs can never overlap on the shared radio.
 */
bool WifiManager::startScan() noexcept {
  if (m_scanState == ScanState::RUNNING) {
    Logger::debug("WifiManager", "Scan already running; refusing duplicate");
    return false;
  }

  // Free any previous result buffer so a fresh scan starts clean.
  WiFi.scanDelete();

  const uint32_t heapBefore = ESP.getFreeHeap();
  const int16_t result = WiFi.scanNetworks(true /* async */);
  if (result == WIFI_SCAN_FAILED) {
    Logger::error("WifiManager", "Async scan failed to start");
    m_lastError = 10;
    m_scanState = ScanState::FAILED;
    return false;
  }

  m_scanState = ScanState::RUNNING;
  m_scanResultCount = 0;
  m_scanStartedMs = millis();
  m_scanDurationMs = 0;
  Logger::info("WifiManager", "Scan started (async) heap=%u maxalloc=%u mode=%u channel=%u",
               heapBefore, ESP.getMaxAllocHeap(), static_cast<unsigned>(WiFi.getMode()), getChannel());
  return true;
}

/**
 * @brief Get the current state of the asynchronous scan job.
 */
WifiManager::ScanState WifiManager::getScanState() const noexcept {
  return m_scanState;
}

/**
 * @brief Get the number of networks found by the last completed scan.
 */
int16_t WifiManager::getScanResultCount() const noexcept {
  return m_scanResultCount;
}

/**
 * @brief Consume and release the last scan results.
 */
void WifiManager::consumeScanResults() noexcept {
  WiFi.scanDelete();
  m_scanState = ScanState::IDLE;
  m_scanResultCount = 0;
  m_scanDurationMs = 0;
}

/**
 * @brief Advance the asynchronous scan job (called from update()).
 *
 * Polls the driver for completion once per loop. On completion the result
 * count is cached and the state transitions to DONE (or FAILED). The driver
 * result buffer stays alive until consumeScanResults() or the next
 * startScan(), so getNetworkInfo() can read it after DONE.
 */
void WifiManager::pollScan() noexcept {
  if (m_scanState != ScanState::RUNNING) {
    return;
  }

  const int16_t status = WiFi.scanComplete();
  if (status == WIFI_SCAN_RUNNING) {
    return;  // still in flight
  }

  m_scanDurationMs = millis() - m_scanStartedMs;

  if (status == WIFI_SCAN_FAILED) {
    Logger::warning("WifiManager", "Scan failed after %lu ms", (unsigned long)m_scanDurationMs);
    m_lastError = 10;
    m_scanState = ScanState::FAILED;
    return;
  }

  m_scanResultCount = status;
  m_scanState = ScanState::DONE;
  Logger::info("WifiManager", "Scan complete: %d networks in %lu ms (heap=%u maxalloc=%u)",
               status, (unsigned long)m_scanDurationMs, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
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
      Logger::error("WifiManager", "Max attempts reached, falling back to AP mode for re-provisioning");
      m_connectionAttempts = 0;
      errorManager.report(AuraErrorSeverity::ERROR, "WiFi", "WIFI_CONN_TIMEOUT",
                          "WiFi connection timed out",
                          "Repeated attempts to join the saved network failed; starting AP mode for re-provisioning");
      // Automatically start AP mode so user can re-provision without power cycle
      if (!m_accessPointMode) {
        startAccessPoint(Secrets::AP_SSID, nullptr);
      }
      changeState(WifiState::ACCESS_POINT);
    }
    return;
  }
  
  // Check connection status
  if (status == WL_CONNECTED) {
    Logger::info("WifiManager", "Connected to %s", m_ssid);
    changeState(WifiState::CONNECTED);
    handleConnected();
  }
}

/**
 * @brief Complete a successful connection.
 *
 * Called from every CONNECTED transition (handleConnection, handleEvents,
 * checkConnection) so the state machine always performs the full completion:
 * clears the connecting flag, resets the attempt budget, refreshes link
 * telemetry, resolves WiFi errors, and (re)synchronizes the system clock via
 * NTP. Previously only the (usually bypassed) handleConnection path attempted
 * NTP sync, so the clock stayed unset until the 1-hour periodic retry and
 * HTTPS calls to cloud providers failed TLS validation.
 */
void WifiManager::handleConnected() noexcept {
  m_connecting = false;
  m_connectionAttempts = 0;
  m_reconnectCount = 0;  // Reset total reconnect counter on successful connection
  m_lastRSSI = WiFi.RSSI();
  m_currentChannel = WiFi.channel();

  // Validate that a valid IP address was assigned (not 0.0.0.0)
  IPAddress localIP = WiFi.localIP();
  if (localIP == IPAddress(0, 0, 0, 0)) {
    Logger::error("WifiManager", "Connected but no IP assigned (DHCP failed)");
    m_lastError = static_cast<uint8_t>(WL_CONNECT_FAILED);
    m_connecting = false;
    m_connectionAttempts = 0;
    m_reconnectTimer = millis();
    changeState(WifiState::DISCONNECTED);
    return;
  }

  errorManager.resolve("WiFi", "WIFI_CONN_TIMEOUT");
  errorManager.resolve("WiFi", "WIFI_CONN_FAILED");
  errorManager.resolve("WiFi", "WIFI_NO_SSID");
  errorManager.resolve("WiFi", "WIFI_LINK_LOST");

  // Attempt NTP sync
  syncTime();
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
      // Budget exhausted for this cycle; hand control to the ERROR state which
      // performs bounded-rate retries instead of giving up permanently.
      Logger::error("WifiManager", "Reconnect budget exhausted; entering bounded retry");
      changeState(WifiState::ERROR);
      m_connectionAttempts = 0;
      m_reconnectTimer = currentMillis;
    }
  }
}

/**
 * @brief Handle Wi-Fi events
 */
void WifiManager::handleEvents() noexcept {
    wl_status_t status = WiFi.status();
    m_lastEvent = status;   // record last observed link event for diagnostics

    switch (status) {
        case WL_CONNECTED:
            // While provisioning, the still-active (old) STA connection must
            // not pre-empt the 5s WAITING_TO_CONNECT delay; only after that
            // delay does attemptConnection() switch the radio to the new creds.
            if (!m_provisioning && m_currentState != WifiState::CONNECTED && !m_accessPointMode) {
                Logger::info("WifiManager", "WiFi connected to %s", WiFi.SSID().c_str());
                changeState(WifiState::CONNECTED);
                handleConnected();
            }
            break;

        case WL_DISCONNECTED:
            if (!m_provisioning && m_currentState == WifiState::CONNECTED) {
                Logger::warning("WifiManager", "WiFi connection lost");
                changeState(WifiState::DISCONNECTED);
                // Seed the state machine so update() reconnects after the
                // bounded backoff. WifiManager owns reconnection; no other
                // module should force WiFi.begin()/reconnect().
                m_connectionAttempts = 1;
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
  if (m_accessPointMode || m_provisioning) {
    // m_provisioning: the WAITING_TO_CONNECT delay is in progress; the
    // still-active (old) STA link must not flip the state back to CONNECTED
    // before attemptConnection() switches the radio to the new credentials.
    return;
  }
  
  wl_status_t status = WiFi.status();
  
  if (status == WL_CONNECTED) {
    if (m_currentState != WifiState::CONNECTED) {
      changeState(WifiState::CONNECTED);
      handleConnected();
    }
    // Update RSSI for connected state
    m_lastRSSI = WiFi.RSSI();
    m_currentChannel = WiFi.channel();
  } else {
    if (m_currentState == WifiState::CONNECTED) {
      Logger::warning("WifiManager", "Connection lost");
      changeState(WifiState::DISCONNECTED);
      // Seed the reconnect state machine (bounded backoff), consistent with
      // handleEvents WL_DISCONNECTED. Reconnect is WifiManager-owned.
      m_connectionAttempts = 1;
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
  
  ensureMode(WIFI_STA);
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
  m_reconnectCount++;
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
  m_waitConnectTimer = millis();
  m_lastConnectionCheck = millis();
}

/**
 * @brief Change state
 */
void WifiManager::changeState(WifiState newState) noexcept {
  if (m_currentState != newState) {
    m_currentState = newState;
    persistDiagnostics();
  }
}

/**
 * @brief Set the Wi-Fi radio mode only when it actually differs from the
 *        current mode.
 *
 * Calling WiFi.mode() on every transition re-initializes the ESP32 network
 * interface and can trigger a reset if done repeatedly (the root cause this
 * change mitigates). Guarding the call keeps the radio in a stable mode and
 * makes WifiManager the single owner of WiFi.mode().
 */
void WifiManager::ensureMode(wifi_mode_t mode) noexcept {
  if (WiFi.getMode() != mode) {
    WiFi.mode(mode);
  }
}

/**
 * @brief Load Wi-Fi diagnostics persisted at the previous shutdown.
 */
void WifiManager::loadDiagnostics() noexcept {
  if (!m_preferences.begin(PREFERENCES_NAMESPACE, true)) {
    m_lastPersistedState = WifiState::DISCONNECTED;
    m_lastPersistedReconnect = 0;
    return;
  }
  uint8_t rawState = m_preferences.getUChar(LAST_STATE_KEY, 0xFF);
  m_lastPersistedReconnect = m_preferences.getUShort(LAST_RECONNECT_KEY, 0);
  m_preferences.end();

  // Clamp to the valid enum range.
  if (rawState > static_cast<uint8_t>(WifiState::ERROR)) {
    m_lastPersistedState = WifiState::DISCONNECTED;
  } else {
    m_lastPersistedState = static_cast<WifiState>(rawState);
  }
}

/**
 * @brief Persist Wi-Fi diagnostics at each state change.
 *
 * NVS writes are guarded to fire only when the state actually changes, so the
 * flash write rate stays bounded (state transitions are rare) and the next
 * boot can report what the radio was doing before the restart.
 */
void WifiManager::persistDiagnostics() noexcept {
  if (!m_preferences.begin(PREFERENCES_NAMESPACE, false)) {
    return;
  }
  m_preferences.putUChar(LAST_STATE_KEY, static_cast<uint8_t>(m_currentState));
  m_preferences.putUShort(LAST_RECONNECT_KEY, m_reconnectCount);
  m_preferences.end();
}