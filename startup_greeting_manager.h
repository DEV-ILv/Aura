#ifndef AURA_STARTUP_GREETING_MANAGER_H
#define AURA_STARTUP_GREETING_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "logger.h"
#include "display_manager.h"
#include "version.h"
#include "wifi_manager.h"
#include "personality_manager.h"

struct StartupGreetingSettings {
    bool enabled;
    bool speakGreeting;
    bool showMark;
    bool showCodename;
    bool showSemVer;
    bool showBuildDate;
    bool showAge;
    bool showPersonality;
    bool showWifi;
    bool showStorage;
    uint8_t displayDurationSec;

    StartupGreetingSettings() noexcept
        : enabled(true), speakGreeting(true), showMark(true), showCodename(true),
          showSemVer(true), showBuildDate(true), showAge(true),
          showPersonality(true), showWifi(true), showStorage(true),
          displayDurationSec(4) {}
};

class StartupGreetingManager {
public:
    StartupGreetingManager() noexcept;
    ~StartupGreetingManager() noexcept;

    StartupGreetingManager(const StartupGreetingManager&) = delete;
    StartupGreetingManager& operator=(const StartupGreetingManager&) = delete;
    StartupGreetingManager(StartupGreetingManager&&) = delete;
    StartupGreetingManager& operator=(StartupGreetingManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    void start() noexcept;
    bool isActive() const noexcept;

    StartupGreetingSettings getSettings() const noexcept;
    bool updateSettings(const StartupGreetingSettings& settings) noexcept;

private:
    enum class GreetingPhase : uint8_t {
        IDLE,
        DISPLAYING,
        SPEAKING,
        COMPLETED
    };

    static constexpr const char* kLogCategory = "StartupGreeting";
    static constexpr const char* kNvsNamespace = STARTUP_GREETING_NVS_NAMESPACE;
    static constexpr unsigned long kMinDisplayMs = 2000UL;
    static constexpr unsigned long kMaxDisplayMs = 15000UL;

    void loadSettings() noexcept;
    void saveSettings() noexcept;
    String getTimeBasedGreeting() noexcept;
    String getTimeOfDay() noexcept;
    String getDayOfWeek() noexcept;
    String formatDate() noexcept;
    String formatTime() noexcept;
    String getSystemAgeText() noexcept;
    String getPersonalityName() noexcept;
    void buildDisplayLines(String lines[], uint8_t& count) noexcept;
    String buildSpeechText() noexcept;
    bool hasInstallDate() noexcept;
    unsigned long getInstallDate() noexcept;
    void setInstallDate() noexcept;
    String getWifiStatus() noexcept;
    String getStorageStatus() noexcept;
    String getBuildDateText() noexcept;

    bool m_initialized;
    bool m_active;
    GreetingPhase m_phase;
    StartupGreetingSettings m_settings;
    Preferences m_prefs;
    unsigned long m_startTime;
    unsigned long m_displayDuration;
};

extern StartupGreetingManager startupGreetingManager;

#endif
