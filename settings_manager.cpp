#include "settings_manager.h"
#include "config.h"
#include "logger.h"
#include <cstring>

// Global instance
SettingsManager settingsManager;

// Settings namespace and keys
namespace {
  constexpr const char* PREFERENCES_NAMESPACE = "aura";
  constexpr const char* VERSION_KEY = "version";
  constexpr uint16_t SETTINGS_VERSION = 1;
  
  // Device settings keys
  constexpr const char* DEVICE_NAME_KEY = "device_name";
  constexpr const char* HOSTNAME_KEY = "hostname";
  
  // WiFi settings keys
  constexpr const char* WIFI_AUTO_CONNECT_KEY = "wifi_auto";
  
  // Display settings keys
  constexpr const char* SCREEN_BRIGHTNESS_KEY = "screen_bright";
  constexpr const char* SCREEN_TIMEOUT_KEY = "screen_timeout";
  
  // LED settings keys
  constexpr const char* LED_BRIGHTNESS_KEY = "led_bright";
  constexpr const char* LED_ENABLED_KEY = "led_enabled";
  
  // Audio settings keys
  constexpr const char* VOLUME_KEY = "volume";
  constexpr const char* MIC_GAIN_KEY = "mic_gain";
  
  // Wake word settings keys
  constexpr const char* WAKE_WORD_ENABLED_KEY = "wake_enabled";
  constexpr const char* WAKE_WORD_KEY = "wake_word";

  // Push-to-talk settings keys
  constexpr const char* CONTINUOUS_LISTENING_KEY = "cont_listen";
  
  // Localization settings keys
  constexpr const char* LANGUAGE_KEY = "language";
  constexpr const char* TIMEZONE_KEY = "timezone";
  constexpr const char* CLOCK_24H_KEY = "clock_24h";
  
  // Reminder settings keys
  constexpr const char* REMINDER_ENABLED_KEY = "reminder";
  
  // Debug/system settings keys
  constexpr const char* DEBUG_LOGGING_KEY = "debug_log";
  constexpr const char* OTA_ENABLED_KEY = "ota_enabled";
  constexpr const char* AUTO_UPDATE_KEY = "auto_update";

  // Interaction refinement keys
  constexpr const char* SILENT_MODE_KEY = "silent";
  constexpr const char* PRIVACY_MODE_KEY = "privacy";
  constexpr const char* NIGHT_MODE_ENABLED_KEY = "night_en";
  constexpr const char* NIGHT_START_HOUR_KEY = "night_sh";
  constexpr const char* NIGHT_START_MIN_KEY = "night_sm";
  constexpr const char* NIGHT_END_HOUR_KEY = "night_eh";
  constexpr const char* NIGHT_END_MIN_KEY = "night_em";
  constexpr const char* AUTO_SLEEP_KEY = "auto_sleep";
  
  // Auto-save interval (5 seconds)
  constexpr uint32_t AUTO_SAVE_INTERVAL_MS = 5000;
  
  // Default values
  constexpr const char* DEFAULT_DEVICE_NAME = "AURA";
  constexpr const char* DEFAULT_HOSTNAME = "aura";
  constexpr bool DEFAULT_WIFI_AUTO_CONNECT = true;
  constexpr uint8_t DEFAULT_SCREEN_BRIGHTNESS = 180;
  constexpr uint16_t DEFAULT_SCREEN_TIMEOUT = 60;
  constexpr uint8_t DEFAULT_LED_BRIGHTNESS = 120;
  constexpr bool DEFAULT_LED_ENABLED = true;
  constexpr uint8_t DEFAULT_VOLUME = 70;
  constexpr uint8_t DEFAULT_MIC_GAIN = 60;
  constexpr bool DEFAULT_WAKE_WORD_ENABLED = true;
  constexpr const char* DEFAULT_WAKE_WORD = "Aura";
  constexpr bool DEFAULT_CONTINUOUS_LISTENING = false;
  constexpr bool DEFAULT_SILENT_MODE = false;
  constexpr bool DEFAULT_PRIVACY_MODE = false;
  constexpr bool DEFAULT_NIGHT_MODE = false;
  constexpr uint8_t DEFAULT_NIGHT_START_HOUR = 21;
  constexpr uint8_t DEFAULT_NIGHT_START_MIN = 0;
  constexpr uint8_t DEFAULT_NIGHT_END_HOUR = 7;
  constexpr uint8_t DEFAULT_NIGHT_END_MIN = 0;
  constexpr uint16_t DEFAULT_AUTO_SLEEP_MIN = 15;
  constexpr const char* DEFAULT_LANGUAGE = "en-US";
  constexpr int32_t DEFAULT_TIMEZONE = 0;
  constexpr bool DEFAULT_CLOCK_24H = false;
  constexpr bool DEFAULT_REMINDER_ENABLED = true;
  constexpr bool DEFAULT_DEBUG_LOGGING = false;
  constexpr bool DEFAULT_OTA_ENABLED = true;
  constexpr bool DEFAULT_AUTO_UPDATE = false;
}

/**
 * @brief Constructor
 */
SettingsManager::SettingsManager() noexcept
    : m_initialized(false),
      m_dirty(false),
      m_version(0),
      m_lastSaveTime(0) {
  loadDefaults();
}

/**
 * @brief Destructor
 */
SettingsManager::~SettingsManager() noexcept {
  if (m_dirty) {
    saveAll();
  }
}

/**
 * @brief Initialize the settings manager
 */
bool SettingsManager::initialize() noexcept {
  Logger::info("SettingsManager", "Initializing");
  
  // Load version
  m_version = loadVersion();
  
  // Load settings
  if (!load()) {
    Logger::warning("SettingsManager", "Failed to load settings, using defaults");
    loadDefaults();
  }
  
  m_initialized = true;
  m_dirty = false;
  m_lastSaveTime = millis();
  
  Logger::info("SettingsManager", "Initialization complete");
  return true;
}

/**
 * @brief Scheduler-compatible update method
 */
void SettingsManager::run() noexcept {
  update();
}

/**
 * @brief Update settings manager state
 */
void SettingsManager::update() noexcept {
  if (!m_initialized) {
    return;
  }
  
  // Auto-save dirty settings
  if (m_dirty && (millis() - m_lastSaveTime >= AUTO_SAVE_INTERVAL_MS)) {
    if (saveAll()) {
      m_dirty = false;
      m_lastSaveTime = millis();
    }
  }
}

/**
 * @brief Load all settings from persistent storage
 */
bool SettingsManager::load() noexcept {
  Logger::debug("SettingsManager", "Loading settings");
  
  if (!beginPreferences(true)) {
    Logger::error("SettingsManager", "Failed to open preferences for reading");
    return false;
  }
  
  // Check version and migrate if necessary
  uint16_t storedVersion = m_preferences.getUShort(VERSION_KEY, 0);
  if (storedVersion != SETTINGS_VERSION && storedVersion != 0) {
    Logger::info("SettingsManager", "Migrating from version %u to %u", storedVersion, SETTINGS_VERSION);
    if (!migrate(storedVersion)) {
      Logger::warning("SettingsManager", "Migration failed, using defaults");
      endPreferences();
      loadDefaults();
      return false;
    }
  }
  
  // Load device settings
  m_preferences.getString(DEVICE_NAME_KEY, m_settings.deviceName, sizeof(m_settings.deviceName));
  m_preferences.getString(HOSTNAME_KEY, m_settings.hostname, sizeof(m_settings.hostname));
  
  // Load WiFi settings
  m_settings.wifiAutoConnect = m_preferences.getBool(WIFI_AUTO_CONNECT_KEY, DEFAULT_WIFI_AUTO_CONNECT);
  
  // Load display settings
  m_settings.screenBrightness = m_preferences.getUChar(SCREEN_BRIGHTNESS_KEY, DEFAULT_SCREEN_BRIGHTNESS);
  m_settings.screenTimeout = m_preferences.getUShort(SCREEN_TIMEOUT_KEY, DEFAULT_SCREEN_TIMEOUT);
  
  // Load LED settings
  m_settings.ledBrightness = m_preferences.getUChar(LED_BRIGHTNESS_KEY, DEFAULT_LED_BRIGHTNESS);
  m_settings.ledEnabled = m_preferences.getBool(LED_ENABLED_KEY, DEFAULT_LED_ENABLED);
  
  // Load audio settings
  m_settings.volume = m_preferences.getUChar(VOLUME_KEY, DEFAULT_VOLUME);
  m_settings.microphoneGain = m_preferences.getUChar(MIC_GAIN_KEY, DEFAULT_MIC_GAIN);
  
  // Load wake word settings
  m_settings.wakeWordEnabled = m_preferences.getBool(WAKE_WORD_ENABLED_KEY, DEFAULT_WAKE_WORD_ENABLED);
  m_preferences.getString(WAKE_WORD_KEY, m_settings.wakeWord, sizeof(m_settings.wakeWord));
  
  // Load push-to-talk settings
  m_settings.continuousListeningEnabled = m_preferences.getBool(CONTINUOUS_LISTENING_KEY, DEFAULT_CONTINUOUS_LISTENING);
  
  // Load interaction refinement settings
  m_settings.silentMode = m_preferences.getBool(SILENT_MODE_KEY, DEFAULT_SILENT_MODE);
  m_settings.privacyMode = m_preferences.getBool(PRIVACY_MODE_KEY, DEFAULT_PRIVACY_MODE);
  m_settings.nightModeEnabled = m_preferences.getBool(NIGHT_MODE_ENABLED_KEY, DEFAULT_NIGHT_MODE);
  m_settings.nightModeStartHour = m_preferences.getUChar(NIGHT_START_HOUR_KEY, DEFAULT_NIGHT_START_HOUR);
  m_settings.nightModeStartMinute = m_preferences.getUChar(NIGHT_START_MIN_KEY, DEFAULT_NIGHT_START_MIN);
  m_settings.nightModeEndHour = m_preferences.getUChar(NIGHT_END_HOUR_KEY, DEFAULT_NIGHT_END_HOUR);
  m_settings.nightModeEndMinute = m_preferences.getUChar(NIGHT_END_MIN_KEY, DEFAULT_NIGHT_END_MIN);
  m_settings.autoSleepTimeoutMin = m_preferences.getUShort(AUTO_SLEEP_KEY, DEFAULT_AUTO_SLEEP_MIN);
  
  // Load localization settings
  m_preferences.getString(LANGUAGE_KEY, m_settings.language, sizeof(m_settings.language));
  m_settings.timezone = m_preferences.getInt(TIMEZONE_KEY, DEFAULT_TIMEZONE);
  m_settings.use24HourClock = m_preferences.getBool(CLOCK_24H_KEY, DEFAULT_CLOCK_24H);
  
  // Load reminder settings
  m_settings.reminderEnabled = m_preferences.getBool(REMINDER_ENABLED_KEY, DEFAULT_REMINDER_ENABLED);
  
  // Load debug/system settings
  m_settings.debugLogging = m_preferences.getBool(DEBUG_LOGGING_KEY, DEFAULT_DEBUG_LOGGING);
  m_settings.otaEnabled = m_preferences.getBool(OTA_ENABLED_KEY, DEFAULT_OTA_ENABLED);
  m_settings.autoUpdate = m_preferences.getBool(AUTO_UPDATE_KEY, DEFAULT_AUTO_UPDATE);
  
  endPreferences();
  
  // Validate all settings
  if (!validate()) {
    Logger::warning("SettingsManager", "Settings validation failed, using defaults");
    loadDefaults();
    return false;
  }
  
  Logger::debug("SettingsManager", "Settings loaded successfully");
  return true;
}

/**
 * @brief Save a single setting to persistent storage
 */
bool SettingsManager::save() noexcept {
  return saveAll();
}

/**
 * @brief Save all settings to persistent storage
 */
bool SettingsManager::saveAll() noexcept {
  Logger::debug("SettingsManager", "Saving all settings");
  
  if (!beginPreferences(false)) {
    Logger::error("SettingsManager", "Failed to open preferences for writing");
    return false;
  }
  
  // Save version
  m_preferences.putUShort(VERSION_KEY, SETTINGS_VERSION);
  
  // Save device settings
  m_preferences.putString(DEVICE_NAME_KEY, m_settings.deviceName);
  m_preferences.putString(HOSTNAME_KEY, m_settings.hostname);
  
  // Save WiFi settings
  m_preferences.putBool(WIFI_AUTO_CONNECT_KEY, m_settings.wifiAutoConnect);
  
  // Save display settings
  m_preferences.putUChar(SCREEN_BRIGHTNESS_KEY, m_settings.screenBrightness);
  m_preferences.putUShort(SCREEN_TIMEOUT_KEY, m_settings.screenTimeout);
  
  // Save LED settings
  m_preferences.putUChar(LED_BRIGHTNESS_KEY, m_settings.ledBrightness);
  m_preferences.putBool(LED_ENABLED_KEY, m_settings.ledEnabled);
  
  // Save audio settings
  m_preferences.putUChar(VOLUME_KEY, m_settings.volume);
  m_preferences.putUChar(MIC_GAIN_KEY, m_settings.microphoneGain);
  
  // Save wake word settings
  m_preferences.putBool(WAKE_WORD_ENABLED_KEY, m_settings.wakeWordEnabled);
  m_preferences.putString(WAKE_WORD_KEY, m_settings.wakeWord);
  
  // Save push-to-talk settings
  m_preferences.putBool(CONTINUOUS_LISTENING_KEY, m_settings.continuousListeningEnabled);
  
  // Save interaction refinement settings
  m_preferences.putBool(SILENT_MODE_KEY, m_settings.silentMode);
  m_preferences.putBool(PRIVACY_MODE_KEY, m_settings.privacyMode);
  m_preferences.putBool(NIGHT_MODE_ENABLED_KEY, m_settings.nightModeEnabled);
  m_preferences.putUChar(NIGHT_START_HOUR_KEY, m_settings.nightModeStartHour);
  m_preferences.putUChar(NIGHT_START_MIN_KEY, m_settings.nightModeStartMinute);
  m_preferences.putUChar(NIGHT_END_HOUR_KEY, m_settings.nightModeEndHour);
  m_preferences.putUChar(NIGHT_END_MIN_KEY, m_settings.nightModeEndMinute);
  m_preferences.putUShort(AUTO_SLEEP_KEY, m_settings.autoSleepTimeoutMin);
  
  // Save localization settings
  m_preferences.putString(LANGUAGE_KEY, m_settings.language);
  m_preferences.putInt(TIMEZONE_KEY, m_settings.timezone);
  m_preferences.putBool(CLOCK_24H_KEY, m_settings.use24HourClock);
  
  // Save reminder settings
  m_preferences.putBool(REMINDER_ENABLED_KEY, m_settings.reminderEnabled);
  
  // Save debug/system settings
  m_preferences.putBool(DEBUG_LOGGING_KEY, m_settings.debugLogging);
  m_preferences.putBool(OTA_ENABLED_KEY, m_settings.otaEnabled);
  m_preferences.putBool(AUTO_UPDATE_KEY, m_settings.autoUpdate);
  
  endPreferences();
  
  Logger::debug("SettingsManager", "Settings saved successfully");
  return true;
}

/**
 * @brief Restore settings to default values
 */
bool SettingsManager::restoreDefaults() noexcept {
  Logger::info("SettingsManager", "Restoring default settings");
  
  loadDefaults();
  m_dirty = true;
  
  return saveAll();
}

/**
 * @brief Perform factory reset (erase all settings)
 */
bool SettingsManager::factoryReset() noexcept {
  Logger::warning("SettingsManager", "Performing factory reset");
  
  if (!beginPreferences(false)) {
    Logger::error("SettingsManager", "Failed to open preferences for factory reset");
    return false;
  }
  
  m_preferences.clear();
  endPreferences();
  
  loadDefaults();
  m_dirty = false;
  
  Logger::info("SettingsManager", "Factory reset complete");
  return true;
}

/**
 * @brief Check if settings manager is initialized
 */
bool SettingsManager::isInitialized() const noexcept {
  return m_initialized;
}

/**
 * @brief Check if settings have been modified since last save
 */
bool SettingsManager::isDirty() const noexcept {
  return m_dirty;
}

// ============================================================================
// DEVICE SETTINGS GETTERS/SETTERS
// ============================================================================

const char* SettingsManager::getDeviceName() const noexcept {
  return m_settings.deviceName;
}

bool SettingsManager::setDeviceName(const char* name) noexcept {
  if (!name || strlen(name) == 0 || strlen(name) >= sizeof(m_settings.deviceName)) {
    Logger::warning("SettingsManager", "Invalid device name");
    return false;
  }
  
  if (strcmp(m_settings.deviceName, name) != 0) {
    strncpy(m_settings.deviceName, name, sizeof(m_settings.deviceName) - 1);
    m_settings.deviceName[sizeof(m_settings.deviceName) - 1] = '\0';
    m_dirty = true;
  }
  
  return true;
}

const char* SettingsManager::getHostname() const noexcept {
  return m_settings.hostname;
}

bool SettingsManager::setHostname(const char* hostname) noexcept {
  if (!hostname || strlen(hostname) == 0 || strlen(hostname) >= sizeof(m_settings.hostname)) {
    Logger::warning("SettingsManager", "Invalid hostname");
    return false;
  }
  
  if (strcmp(m_settings.hostname, hostname) != 0) {
    strncpy(m_settings.hostname, hostname, sizeof(m_settings.hostname) - 1);
    m_settings.hostname[sizeof(m_settings.hostname) - 1] = '\0';
    m_dirty = true;
  }
  
  return true;
}

// ============================================================================
// WIFI SETTINGS GETTERS/SETTERS
// ============================================================================

bool SettingsManager::getWifiAutoConnect() const noexcept {
  return m_settings.wifiAutoConnect;
}

bool SettingsManager::setWifiAutoConnect(bool enabled) noexcept {
  if (m_settings.wifiAutoConnect != enabled) {
    m_settings.wifiAutoConnect = enabled;
    m_dirty = true;
  }
  return true;
}

// ============================================================================
// DISPLAY SETTINGS GETTERS/SETTERS
// ============================================================================

uint8_t SettingsManager::getScreenBrightness() const noexcept {
  return m_settings.screenBrightness;
}

bool SettingsManager::setScreenBrightness(uint8_t brightness) noexcept {
  // Brightness is 0-255, no clamping needed
  if (m_settings.screenBrightness != brightness) {
    m_settings.screenBrightness = brightness;
    m_dirty = true;
  }
  return true;
}

uint16_t SettingsManager::getScreenTimeout() const noexcept {
  return m_settings.screenTimeout;
}

bool SettingsManager::setScreenTimeout(uint16_t seconds) noexcept {
  if (m_settings.screenTimeout != seconds) {
    m_settings.screenTimeout = seconds;
    m_dirty = true;
  }
  return true;
}

// ============================================================================
// LED SETTINGS GETTERS/SETTERS
// ============================================================================

uint8_t SettingsManager::getLedBrightness() const noexcept {
  return m_settings.ledBrightness;
}

bool SettingsManager::setLedBrightness(uint8_t brightness) noexcept {
  if (m_settings.ledBrightness != brightness) {
    m_settings.ledBrightness = brightness;
    m_dirty = true;
  }
  return true;
}

bool SettingsManager::getLedEnabled() const noexcept {
  return m_settings.ledEnabled;
}

bool SettingsManager::setLedEnabled(bool enabled) noexcept {
  if (m_settings.ledEnabled != enabled) {
    m_settings.ledEnabled = enabled;
    m_dirty = true;
  }
  return true;
}

// ============================================================================
// AUDIO SETTINGS GETTERS/SETTERS
// ============================================================================

uint8_t SettingsManager::getVolume() const noexcept {
  return m_settings.volume;
}

bool SettingsManager::setVolume(uint8_t volume) noexcept {
  // Clamp to 0-100
  uint8_t clampedVolume = volume > 100 ? 100 : volume;
  
  if (m_settings.volume != clampedVolume) {
    m_settings.volume = clampedVolume;
    m_dirty = true;
  }
  return true;
}

uint8_t SettingsManager::getMicrophoneGain() const noexcept {
  return m_settings.microphoneGain;
}

bool SettingsManager::setMicrophoneGain(uint8_t gain) noexcept {
  // Clamp to 0-100
  uint8_t clampedGain = gain > 100 ? 100 : gain;
  
  if (m_settings.microphoneGain != clampedGain) {
    m_settings.microphoneGain = clampedGain;
    m_dirty = true;
  }
  return true;
}

// ============================================================================
// WAKE WORD SETTINGS GETTERS/SETTERS
// ============================================================================

bool SettingsManager::getWakeWordEnabled() const noexcept {
  return m_settings.wakeWordEnabled;
}

bool SettingsManager::setWakeWordEnabled(bool enabled) noexcept {
  if (m_settings.wakeWordEnabled != enabled) {
    m_settings.wakeWordEnabled = enabled;
    m_dirty = true;
  }
  return true;
}

bool SettingsManager::getContinuousListeningEnabled() const noexcept {
  return m_settings.continuousListeningEnabled;
}

bool SettingsManager::setContinuousListeningEnabled(bool enabled) noexcept {
  if (m_settings.continuousListeningEnabled != enabled) {
    m_settings.continuousListeningEnabled = enabled;
    m_dirty = true;
  }
  return true;
}

// ========================================================================
// Interaction refinement getters/setters
// ========================================================================

bool SettingsManager::getSilentMode() const noexcept {
  return m_settings.silentMode;
}

bool SettingsManager::setSilentMode(bool enabled) noexcept {
  if (m_settings.silentMode != enabled) {
    m_settings.silentMode = enabled;
    m_dirty = true;
  }
  return true;
}

bool SettingsManager::getPrivacyMode() const noexcept {
  return m_settings.privacyMode;
}

bool SettingsManager::setPrivacyMode(bool enabled) noexcept {
  if (m_settings.privacyMode != enabled) {
    m_settings.privacyMode = enabled;
    m_dirty = true;
  }
  return true;
}

bool SettingsManager::getNightModeEnabled() const noexcept {
  return m_settings.nightModeEnabled;
}

bool SettingsManager::setNightModeEnabled(bool enabled) noexcept {
  if (m_settings.nightModeEnabled != enabled) {
    m_settings.nightModeEnabled = enabled;
    m_dirty = true;
  }
  return true;
}

uint8_t SettingsManager::getNightModeStartHour() const noexcept {
  return m_settings.nightModeStartHour;
}

bool SettingsManager::setNightModeStartHour(uint8_t hour) noexcept {
  if (m_settings.nightModeStartHour != hour) {
    m_settings.nightModeStartHour = hour;
    m_dirty = true;
  }
  return true;
}

uint8_t SettingsManager::getNightModeStartMinute() const noexcept {
  return m_settings.nightModeStartMinute;
}

bool SettingsManager::setNightModeStartMinute(uint8_t minute) noexcept {
  if (m_settings.nightModeStartMinute != minute) {
    m_settings.nightModeStartMinute = minute;
    m_dirty = true;
  }
  return true;
}

uint8_t SettingsManager::getNightModeEndHour() const noexcept {
  return m_settings.nightModeEndHour;
}

bool SettingsManager::setNightModeEndHour(uint8_t hour) noexcept {
  if (m_settings.nightModeEndHour != hour) {
    m_settings.nightModeEndHour = hour;
    m_dirty = true;
  }
  return true;
}

uint8_t SettingsManager::getNightModeEndMinute() const noexcept {
  return m_settings.nightModeEndMinute;
}

bool SettingsManager::setNightModeEndMinute(uint8_t minute) noexcept {
  if (m_settings.nightModeEndMinute != minute) {
    m_settings.nightModeEndMinute = minute;
    m_dirty = true;
  }
  return true;
}

uint16_t SettingsManager::getAutoSleepTimeoutMin() const noexcept {
  return m_settings.autoSleepTimeoutMin;
}

bool SettingsManager::setAutoSleepTimeoutMin(uint16_t minutes) noexcept {
  if (m_settings.autoSleepTimeoutMin != minutes) {
    m_settings.autoSleepTimeoutMin = minutes;
    m_dirty = true;
  }
  return true;
}

const char* SettingsManager::getWakeWord() const noexcept {
  return m_settings.wakeWord;
}

bool SettingsManager::setWakeWord(const char* wakeWord) noexcept {
  if (!wakeWord || strlen(wakeWord) == 0 || strlen(wakeWord) >= sizeof(m_settings.wakeWord)) {
    Logger::warning("SettingsManager", "Invalid wake word");
    return false;
  }
  
  if (strcmp(m_settings.wakeWord, wakeWord) != 0) {
    strncpy(m_settings.wakeWord, wakeWord, sizeof(m_settings.wakeWord) - 1);
    m_settings.wakeWord[sizeof(m_settings.wakeWord) - 1] = '\0';
    m_dirty = true;
  }
  
  return true;
}

// ============================================================================
// LOCALIZATION SETTINGS GETTERS/SETTERS
// ============================================================================

const char* SettingsManager::getLanguage() const noexcept {
  return m_settings.language;
}

bool SettingsManager::setLanguage(const char* language) noexcept {
  if (!language || strlen(language) == 0 || strlen(language) >= sizeof(m_settings.language)) {
    Logger::warning("SettingsManager", "Invalid language");
    return false;
  }
  
  if (strcmp(m_settings.language, language) != 0) {
    strncpy(m_settings.language, language, sizeof(m_settings.language) - 1);
    m_settings.language[sizeof(m_settings.language) - 1] = '\0';
    m_dirty = true;
  }
  
  return true;
}

int32_t SettingsManager::getTimezone() const noexcept {
  return m_settings.timezone;
}

bool SettingsManager::setTimezone(int32_t timezone) noexcept {
  if (m_settings.timezone != timezone) {
    m_settings.timezone = timezone;
    m_dirty = true;
  }
  return true;
}

bool SettingsManager::getUse24HourClock() const noexcept {
  return m_settings.use24HourClock;
}

bool SettingsManager::setUse24HourClock(bool use24Hour) noexcept {
  if (m_settings.use24HourClock != use24Hour) {
    m_settings.use24HourClock = use24Hour;
    m_dirty = true;
  }
  return true;
}

// ============================================================================
// REMINDER SETTINGS GETTERS/SETTERS
// ============================================================================

bool SettingsManager::getReminderEnabled() const noexcept {
  return m_settings.reminderEnabled;
}

bool SettingsManager::setReminderEnabled(bool enabled) noexcept {
  if (m_settings.reminderEnabled != enabled) {
    m_settings.reminderEnabled = enabled;
    m_dirty = true;
  }
  return true;
}

// ============================================================================
// DEBUG AND SYSTEM SETTINGS GETTERS/SETTERS
// ============================================================================

bool SettingsManager::getDebugLogging() const noexcept {
  return m_settings.debugLogging;
}

bool SettingsManager::setDebugLogging(bool enabled) noexcept {
  if (m_settings.debugLogging != enabled) {
    m_settings.debugLogging = enabled;
    m_dirty = true;
  }
  return true;
}

bool SettingsManager::getOtaEnabled() const noexcept {
  return m_settings.otaEnabled;
}

bool SettingsManager::setOtaEnabled(bool enabled) noexcept {
  if (m_settings.otaEnabled != enabled) {
    m_settings.otaEnabled = enabled;
    m_dirty = true;
  }
  return true;
}

bool SettingsManager::getAutoUpdate() const noexcept {
  return m_settings.autoUpdate;
}

bool SettingsManager::setAutoUpdate(bool enabled) noexcept {
  if (m_settings.autoUpdate != enabled) {
    m_settings.autoUpdate = enabled;
    m_dirty = true;
  }
  return true;
}

/**
 * @brief Get access to raw settings structure
 */
const Settings& SettingsManager::getSettings() const noexcept {
  return m_settings;
}

// ============================================================================
// PRIVATE IMPLEMENTATION
// ============================================================================

/**
 * @brief Load default settings
 */
void SettingsManager::loadDefaults() noexcept {
  // Device settings
  strncpy(m_settings.deviceName, DEFAULT_DEVICE_NAME, sizeof(m_settings.deviceName) - 1);
  m_settings.deviceName[sizeof(m_settings.deviceName) - 1] = '\0';
  
  strncpy(m_settings.hostname, DEFAULT_HOSTNAME, sizeof(m_settings.hostname) - 1);
  m_settings.hostname[sizeof(m_settings.hostname) - 1] = '\0';
  
  // WiFi settings
  m_settings.wifiAutoConnect = DEFAULT_WIFI_AUTO_CONNECT;
  
  // Display settings
  m_settings.screenBrightness = DEFAULT_SCREEN_BRIGHTNESS;
  m_settings.screenTimeout = DEFAULT_SCREEN_TIMEOUT;
  
  // LED settings
  m_settings.ledBrightness = DEFAULT_LED_BRIGHTNESS;
  m_settings.ledEnabled = DEFAULT_LED_ENABLED;
  
  // Audio settings
  m_settings.volume = DEFAULT_VOLUME;
  m_settings.microphoneGain = DEFAULT_MIC_GAIN;
  
  // Wake word settings
  m_settings.wakeWordEnabled = DEFAULT_WAKE_WORD_ENABLED;
  strncpy(m_settings.wakeWord, DEFAULT_WAKE_WORD, sizeof(m_settings.wakeWord) - 1);
  m_settings.wakeWord[sizeof(m_settings.wakeWord) - 1] = '\0';
  
  // Push-to-talk settings
  m_settings.continuousListeningEnabled = DEFAULT_CONTINUOUS_LISTENING;
  
  // Interaction refinement settings
  m_settings.silentMode = DEFAULT_SILENT_MODE;
  m_settings.privacyMode = DEFAULT_PRIVACY_MODE;
  m_settings.nightModeEnabled = DEFAULT_NIGHT_MODE;
  m_settings.nightModeStartHour = DEFAULT_NIGHT_START_HOUR;
  m_settings.nightModeStartMinute = DEFAULT_NIGHT_START_MIN;
  m_settings.nightModeEndHour = DEFAULT_NIGHT_END_HOUR;
  m_settings.nightModeEndMinute = DEFAULT_NIGHT_END_MIN;
  m_settings.autoSleepTimeoutMin = DEFAULT_AUTO_SLEEP_MIN;
  
  // Localization settings
  strncpy(m_settings.language, DEFAULT_LANGUAGE, sizeof(m_settings.language) - 1);
  m_settings.language[sizeof(m_settings.language) - 1] = '\0';
  
  m_settings.timezone = DEFAULT_TIMEZONE;
  m_settings.use24HourClock = DEFAULT_CLOCK_24H;
  
  // Reminder settings
  m_settings.reminderEnabled = DEFAULT_REMINDER_ENABLED;
  
  // Debug/system settings
  m_settings.debugLogging = DEFAULT_DEBUG_LOGGING;
  m_settings.otaEnabled = DEFAULT_OTA_ENABLED;
  m_settings.autoUpdate = DEFAULT_AUTO_UPDATE;
  
  m_version = SETTINGS_VERSION;
}

/**
 * @brief Validate all settings
 */
bool SettingsManager::validate() noexcept {
  // Validate brightness values (0-255)
  if (m_settings.screenBrightness > 255) {
    m_settings.screenBrightness = 255;
  }
  
  if (m_settings.ledBrightness > 255) {
    m_settings.ledBrightness = 255;
  }
  
  // Validate volume (0-100)
  if (m_settings.volume > 100) {
    m_settings.volume = 100;
  }
  
  // Validate microphone gain (0-100)
  if (m_settings.microphoneGain > 100) {
    m_settings.microphoneGain = 100;
  }
  
  // Validate strings are not empty
  if (strlen(m_settings.deviceName) == 0) {
    strncpy(m_settings.deviceName, DEFAULT_DEVICE_NAME, sizeof(m_settings.deviceName) - 1);
  }
  
  if (strlen(m_settings.hostname) == 0) {
    strncpy(m_settings.hostname, DEFAULT_HOSTNAME, sizeof(m_settings.hostname) - 1);
  }
  
  if (strlen(m_settings.wakeWord) == 0) {
    strncpy(m_settings.wakeWord, DEFAULT_WAKE_WORD, sizeof(m_settings.wakeWord) - 1);
  }
  
  if (strlen(m_settings.language) == 0) {
    strncpy(m_settings.language, DEFAULT_LANGUAGE, sizeof(m_settings.language) - 1);
  }
  
  return true;
}

/**
 * @brief Load version from preferences
 */
uint16_t SettingsManager::loadVersion() noexcept {
  if (!beginPreferences(true)) {
    return 0;
  }
  
  uint16_t version = m_preferences.getUShort(VERSION_KEY, 0);
  endPreferences();
  
  return version;
}

/**
 * @brief Migrate settings from old version
 */
bool SettingsManager::migrate(uint16_t oldVersion) noexcept {
  // Version 0 to 1 migration (initial implementation)
  if (oldVersion == 0) {
    Logger::debug("SettingsManager", "Migrating from version 0 to 1");
    // Load with defaults for any missing settings
    loadDefaults();
    return true;
  }
  
  Logger::warning("SettingsManager", "Unknown settings version %u", oldVersion);
  return false;
}

/**
 * @brief Begin preferences transaction
 */
bool SettingsManager::beginPreferences(bool readOnly) noexcept {
  return m_preferences.begin(PREFERENCES_NAMESPACE, readOnly);
}

/**
 * @brief End preferences transaction
 */
bool SettingsManager::endPreferences() noexcept {
  m_preferences.end();
  return true;
}