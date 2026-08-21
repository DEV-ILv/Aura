#ifndef AURA_SETTINGS_MANAGER_H
#define AURA_SETTINGS_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <cstdint>

/**
 * @struct Settings
 * @brief Container for all persistent user settings
 */
struct Settings {
  // Device identification
  char deviceName[32];              ///< User-friendly device name
  char hostname[32];                ///< Network hostname
  
  // WiFi settings
  bool wifiAutoConnect;             ///< Auto-connect to saved networks
  
  // Display settings
  uint8_t screenBrightness;         ///< Screen brightness (0-255)
  uint16_t screenTimeout;           ///< Screen timeout in seconds (0 = never)
  
  // LED settings
  uint8_t ledBrightness;            ///< LED brightness (0-255)
  bool ledEnabled;                  ///< Enable/disable LEDs
  uint8_t discoBrightness;          ///< Disco mode brightness (10-100)
  
  // Audio settings
  uint8_t volume;                   ///< Speaker volume (0-100)
  uint8_t microphoneGain;           ///< Microphone gain (0-100)
  
  // Wake word settings
  bool wakeWordEnabled;             ///< Enable/disable wake word detection
  char wakeWord[32];                ///< Wake word phrase

  // Push-to-talk settings
  bool continuousListeningEnabled;  ///< Enable continuous listening mode

  // Interaction refinement settings
  bool silentMode;                  ///< Silent mode (no TTS, display only)
  bool privacyMode;                 ///< Privacy mode (no mic, no network)

  // Output routing settings
  bool outputToOled;                ///< Route assistant responses to the OLED display
  bool outputToSpeaker;             ///< Route assistant responses to the speaker (TTS)
  bool outputToCompanion;           ///< Route assistant responses to the Companion App
  bool nightModeEnabled;            ///< Enable night mode schedule
  uint8_t nightModeStartHour;       ///< Night mode start hour (0-23)
  uint8_t nightModeStartMinute;     ///< Night mode start minute (0-59)
  uint8_t nightModeEndHour;         ///< Night mode end hour (0-23)
  uint8_t nightModeEndMinute;       ///< Night mode end minute (0-59)
  uint16_t autoSleepTimeoutMin;     ///< Auto-sleep timeout in minutes (default 15)
  
  // Localization
  char language[8];                 ///< Language code (e.g., "en-US")
  int32_t timezone;                 ///< Timezone offset in seconds from UTC
  bool use24HourClock;              ///< Use 24-hour time format
  
  // Reminder settings
  bool reminderEnabled;             ///< Enable/disable reminders
  
  // Debug and system
  bool debugLogging;                ///< Enable/disable debug logging
  bool otaEnabled;                  ///< Enable/disable OTA updates
  bool autoUpdate;                  ///< Enable/disable automatic updates
};

/**
 * @class SettingsManager
 * @brief Manages all persistent user settings for AURA
 * 
 * This class serves as the single authority for:
 * - Loading and saving all persistent settings
 * - Validating setting values
 * - Providing runtime access to settings
 * - Supporting factory reset and settings restore
 * - Handling firmware version migration
 * 
 * Thread-safe for read operations from main loop.
 */
class SettingsManager {
public:
  /**
   * @brief Constructor
   */
  SettingsManager() noexcept;

  /**
   * @brief Destructor
   */
  ~SettingsManager() noexcept;

  // Delete copy semantics
  SettingsManager(const SettingsManager&) = delete;
  SettingsManager& operator=(const SettingsManager&) = delete;

  // Delete move semantics
  SettingsManager(SettingsManager&&) = delete;
  SettingsManager& operator=(SettingsManager&&) = delete;

  /**
   * @brief Initialize the settings manager
   * @return true if initialization successful, false otherwise
   * @note Should be called once during setup()
   */
  [[nodiscard]] bool initialize() noexcept;

  /**
   * @brief Load all settings from persistent storage
   * @return true if load successful, false otherwise
   */
  [[nodiscard]] bool load() noexcept;

  /**
   * @brief Save a single setting to persistent storage
   * @return true if save successful, false otherwise
   */
  [[nodiscard]] bool save() noexcept;

  /**
   * @brief Save all settings to persistent storage
   * @return true if save successful, false otherwise
   */
  [[nodiscard]] bool saveAll() noexcept;

  /**
   * @brief Restore settings to default values
   * @return true if restore successful, false otherwise
   * @note Does not erase persistent storage
   */
  [[nodiscard]] bool restoreDefaults() noexcept;

  /**
   * @brief Perform factory reset (erase all settings)
   * @return true if factory reset successful, false otherwise
   */
  [[nodiscard]] bool factoryReset() noexcept;

  /**
   * @brief Scheduler-compatible update method
   * @note For compatibility with task schedulers
   */
  void run() noexcept;

  /**
   * @brief Update settings manager state
   * @note Should be called regularly from loop()
   */
  void update() noexcept;

  /**
   * @brief Check if settings manager is initialized
   * @return true if initialized, false otherwise
   */
  [[nodiscard]] bool isInitialized() const noexcept;

  /**
   * @brief Check if settings have been modified since last save
   * @return true if settings are dirty, false otherwise
   */
  [[nodiscard]] bool isDirty() const noexcept;

  // ========================================================================
  // DEVICE SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Get device name
   * @return Pointer to device name string
   */
  [[nodiscard]] const char* getDeviceName() const noexcept;

  /**
   * @brief Set device name
   * @param name New device name
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setDeviceName(const char* name) noexcept;

  /**
   * @brief Get hostname
   * @return Pointer to hostname string
   */
  [[nodiscard]] const char* getHostname() const noexcept;

  /**
   * @brief Set hostname
   * @param hostname New hostname
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setHostname(const char* hostname) noexcept;

  // ========================================================================
  // WIFI SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Check if WiFi auto-connect is enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getWifiAutoConnect() const noexcept;

  /**
   * @brief Set WiFi auto-connect
   * @param enabled Enable or disable auto-connect
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setWifiAutoConnect(bool enabled) noexcept;

  // ========================================================================
  // DISPLAY SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Get screen brightness
   * @return Brightness value (0-255)
   */
  [[nodiscard]] uint8_t getScreenBrightness() const noexcept;

  /**
   * @brief Set screen brightness
   * @param brightness Brightness value (0-255)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setScreenBrightness(uint8_t brightness) noexcept;

  /**
   * @brief Get screen timeout
   * @return Timeout in seconds (0 = never)
   */
  [[nodiscard]] uint16_t getScreenTimeout() const noexcept;

  /**
   * @brief Set screen timeout
   * @param seconds Timeout in seconds (0 = never)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setScreenTimeout(uint16_t seconds) noexcept;

  // ========================================================================
  // LED SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Get LED brightness
   * @return Brightness value (0-255)
   */
  [[nodiscard]] uint8_t getLedBrightness() const noexcept;

  /**
   * @brief Set LED brightness
   * @param brightness Brightness value (0-255)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setLedBrightness(uint8_t brightness) noexcept;

  /**
   * @brief Check if LEDs are enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getLedEnabled() const noexcept;

  /**
   * @brief Set LED enable/disable
   * @param enabled Enable or disable LEDs
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setLedEnabled(bool enabled) noexcept;

  /**
   * @brief Get Disco mode brightness
   * @return Brightness value (10-100)
   */
  [[nodiscard]] uint8_t getDiscoBrightness() const noexcept;

  /**
   * @brief Set Disco mode brightness
   * @param brightness Brightness value (10-100)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setDiscoBrightness(uint8_t brightness) noexcept;

  // ========================================================================
  // AUDIO SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Get speaker volume
   * @return Volume value (0-100)
   */
  [[nodiscard]] uint8_t getVolume() const noexcept;

  /**
   * @brief Set speaker volume
   * @param volume Volume value (0-100)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setVolume(uint8_t volume) noexcept;

  /**
   * @brief Get microphone gain
   * @return Gain value (0-100)
   */
  [[nodiscard]] uint8_t getMicrophoneGain() const noexcept;

  /**
   * @brief Set microphone gain
   * @param gain Gain value (0-100)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setMicrophoneGain(uint8_t gain) noexcept;

  // ========================================================================
  // WAKE WORD SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Check if wake word detection is enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getWakeWordEnabled() const noexcept;

  /**
   * @brief Set wake word detection
   * @param enabled Enable or disable wake word
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setWakeWordEnabled(bool enabled) noexcept;

  // ========================================================================
  // PUSH-TO-TALK SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Check if continuous listening mode is enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getContinuousListeningEnabled() const noexcept;

  /**
   * @brief Set continuous listening mode
   * @param enabled Enable or disable continuous listening
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setContinuousListeningEnabled(bool enabled) noexcept;

  /**
   * @brief Get silent mode state
   * @return true if silent mode is active
   */
  [[nodiscard]] bool getSilentMode() const noexcept;

  /**
   * @brief Set silent mode
   * @param enabled Enable or disable silent mode
   * @return true if set successfully
   */
  [[nodiscard]] bool setSilentMode(bool enabled) noexcept;

  // ========================================================================
  // OUTPUT ROUTING GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Whether assistant responses are routed to the OLED display
   * @return true if OLED output is enabled
   */
  [[nodiscard]] bool getOutputToOled() const noexcept;

  /**
   * @brief Set OLED output routing
   * @param enabled Enable or disable OLED output
   * @return true if set successfully
   */
  [[nodiscard]] bool setOutputToOled(bool enabled) noexcept;

  /**
   * @brief Whether assistant responses are routed to the speaker (TTS)
   * @return true if speaker output is enabled
   */
  [[nodiscard]] bool getOutputToSpeaker() const noexcept;

  /**
   * @brief Set speaker output routing
   * @param enabled Enable or disable speaker output
   * @return true if set successfully
   */
  [[nodiscard]] bool setOutputToSpeaker(bool enabled) noexcept;

  /**
   * @brief Whether assistant responses are routed to the Companion App
   * @return true if Companion App output is enabled
   */
  [[nodiscard]] bool getOutputToCompanion() const noexcept;

  /**
   * @brief Set Companion App output routing
   * @param enabled Enable or disable Companion App output
   * @return true if set successfully
   */
  [[nodiscard]] bool setOutputToCompanion(bool enabled) noexcept;

  /**
   * @brief Get privacy mode state
   * @return true if privacy mode is active
   */
  [[nodiscard]] bool getPrivacyMode() const noexcept;

  /**
   * @brief Set privacy mode
   * @param enabled Enable or disable privacy mode
   * @return true if set successfully
   */
  [[nodiscard]] bool setPrivacyMode(bool enabled) noexcept;

  /**
   * @brief Get night mode state
   * @return true if night mode schedule is enabled
   */
  [[nodiscard]] bool getNightModeEnabled() const noexcept;

  /**
   * @brief Set night mode schedule
   * @param enabled Enable or disable night mode
   * @return true if set successfully
   */
  [[nodiscard]] bool setNightModeEnabled(bool enabled) noexcept;

  /**
   * @brief Get night mode start hour
   * @return Hour (0-23)
   */
  [[nodiscard]] uint8_t getNightModeStartHour() const noexcept;

  /**
   * @brief Set night mode start hour
   * @param hour Hour (0-23)
   * @return true if set successfully
   */
  [[nodiscard]] bool setNightModeStartHour(uint8_t hour) noexcept;

  /**
   * @brief Get night mode start minute
   * @return Minute (0-59)
   */
  [[nodiscard]] uint8_t getNightModeStartMinute() const noexcept;

  /**
   * @brief Set night mode start minute
   * @param minute Minute (0-59)
   * @return true if set successfully
   */
  [[nodiscard]] bool setNightModeStartMinute(uint8_t minute) noexcept;

  /**
   * @brief Get night mode end hour
   * @return Hour (0-23)
   */
  [[nodiscard]] uint8_t getNightModeEndHour() const noexcept;

  /**
   * @brief Set night mode end hour
   * @param hour Hour (0-23)
   * @return true if set successfully
   */
  [[nodiscard]] bool setNightModeEndHour(uint8_t hour) noexcept;

  /**
   * @brief Get night mode end minute
   * @return Minute (0-59)
   */
  [[nodiscard]] uint8_t getNightModeEndMinute() const noexcept;

  /**
   * @brief Set night mode end minute
   * @param minute Minute (0-59)
   * @return true if set successfully
   */
  [[nodiscard]] bool setNightModeEndMinute(uint8_t minute) noexcept;

  /**
   * @brief Get auto-sleep timeout
   * @return Timeout in minutes
   */
  [[nodiscard]] uint16_t getAutoSleepTimeoutMin() const noexcept;

  /**
   * @brief Set auto-sleep timeout
   * @param minutes Timeout in minutes (0 = disabled)
   * @return true if set successfully
   */
  [[nodiscard]] bool setAutoSleepTimeoutMin(uint16_t minutes) noexcept;

  /**
   * @brief Get wake word phrase
   * @return Pointer to wake word string
   */
  [[nodiscard]] const char* getWakeWord() const noexcept;

  /**
   * @brief Set wake word phrase
   * @param wakeWord New wake word phrase
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setWakeWord(const char* wakeWord) noexcept;

  // ========================================================================
  // LOCALIZATION SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Get language code
   * @return Pointer to language string (e.g., "en-US")
   */
  [[nodiscard]] const char* getLanguage() const noexcept;

  /**
   * @brief Set language code
   * @param language Language code (e.g., "en-US")
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setLanguage(const char* language) noexcept;

  /**
   * @brief Get timezone offset
   * @return Timezone offset in seconds from UTC
   */
  [[nodiscard]] int32_t getTimezone() const noexcept;

  /**
   * @brief Set timezone offset
   * @param timezone Timezone offset in seconds from UTC
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setTimezone(int32_t timezone) noexcept;

  /**
   * @brief Check if 24-hour clock format is enabled
   * @return true if 24-hour format, false for 12-hour format
   */
  [[nodiscard]] bool getUse24HourClock() const noexcept;

  /**
   * @brief Set clock format
   * @param use24Hour Use 24-hour format (true) or 12-hour format (false)
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setUse24HourClock(bool use24Hour) noexcept;

  // ========================================================================
  // REMINDER SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Check if reminders are enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getReminderEnabled() const noexcept;

  /**
   * @brief Set reminder enable/disable
   * @param enabled Enable or disable reminders
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setReminderEnabled(bool enabled) noexcept;

  // ========================================================================
  // DEBUG AND SYSTEM SETTINGS GETTERS/SETTERS
  // ========================================================================

  /**
   * @brief Check if debug logging is enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getDebugLogging() const noexcept;

  /**
   * @brief Set debug logging
   * @param enabled Enable or disable debug logging
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setDebugLogging(bool enabled) noexcept;

  /**
   * @brief Check if OTA updates are enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getOtaEnabled() const noexcept;

  /**
   * @brief Set OTA enable/disable
   * @param enabled Enable or disable OTA
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setOtaEnabled(bool enabled) noexcept;

  /**
   * @brief Check if automatic updates are enabled
   * @return true if enabled, false otherwise
   */
  [[nodiscard]] bool getAutoUpdate() const noexcept;

  /**
   * @brief Set automatic update
   * @param enabled Enable or disable automatic updates
   * @return true if set successfully, false otherwise
   */
  [[nodiscard]] bool setAutoUpdate(bool enabled) noexcept;

  /**
   * @brief Get access to raw settings structure
   * @return Const reference to settings structure
   */
  [[nodiscard]] const Settings& getSettings() const noexcept;

private:
  // Private helper methods
  void loadDefaults() noexcept;
  bool validate() noexcept;
  uint16_t loadVersion() noexcept;
  bool migrate(uint16_t oldVersion) noexcept;
  bool beginPreferences(bool readOnly = true) noexcept;
  bool endPreferences() noexcept;

  // Private member variables
  Settings m_settings;                  ///< Current settings
  Preferences m_preferences;            ///< Persistent storage
  bool m_initialized;                   ///< Initialization flag
  bool m_dirty;                         ///< Settings modified flag
  uint16_t m_version;                   ///< Settings version
  uint32_t m_lastSaveTime;              ///< Timestamp of last save
};

/**
 * @brief Global settings manager instance
 */
extern SettingsManager settingsManager;

#endif // AURA_SETTINGS_MANAGER_H