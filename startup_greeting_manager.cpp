#include "startup_greeting_manager.h"
#include "sarvam_tts.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "aura_system.h"

StartupGreetingManager startupGreetingManager;

StartupGreetingManager::StartupGreetingManager() noexcept
    : m_initialized(false)
    , m_active(false)
    , m_phase(GreetingPhase::IDLE)
    , m_startTime(0)
    , m_displayDuration(STARTUP_GREETING_DISPLAY_MS) {}

StartupGreetingManager::~StartupGreetingManager() noexcept {
    if (m_initialized) {
        m_prefs.end();
    }
}

bool StartupGreetingManager::initialize() noexcept {
    if (m_initialized) return true;

    m_prefs.begin(kNvsNamespace, false);
    loadSettings();

    m_initialized = true;
    LOG_INFO(kLogCategory, "Startup greeting manager initialized");
    return true;
}

void StartupGreetingManager::update() noexcept {
    if (!m_initialized || !m_active) return;
    if (m_phase == GreetingPhase::IDLE || m_phase == GreetingPhase::COMPLETED) return;

    const unsigned long now = millis();

    if (m_phase == GreetingPhase::DISPLAYING) {
        if (now - m_startTime >= m_displayDuration) {
            if (m_settings.speakGreeting) {
                m_phase = GreetingPhase::SPEAKING;
                const String speechText = buildSpeechText();
                if (!speechText.isEmpty()) {
                    textToSpeech.speak(speechText, true);
                }
            } else {
                m_phase = GreetingPhase::COMPLETED;
                m_active = false;
                displayManager.showHome();
                auraSystem.enterIdle();
                LOG_INFO(kLogCategory, "Startup greeting complete (no speech)");
            }
            m_startTime = now;
        }
    }

    if (m_phase == GreetingPhase::SPEAKING) {
        if (!textToSpeech.isBusy()) {
m_phase = GreetingPhase::COMPLETED;
            m_active = false;
            displayManager.showHome();
            auraSystem.enterIdle();
            LOG_INFO(kLogCategory, "Startup greeting complete");
        }
    }
}

void StartupGreetingManager::start() noexcept {
    if (!m_initialized || !m_settings.enabled || m_active) return;

    m_active = true;
    m_phase = GreetingPhase::DISPLAYING;
    m_startTime = millis();
    m_displayDuration = static_cast<unsigned long>(m_settings.displayDurationSec) * 1000UL;
    if (m_displayDuration < kMinDisplayMs) m_displayDuration = kMinDisplayMs;
    if (m_displayDuration > kMaxDisplayMs) m_displayDuration = kMaxDisplayMs;

    if (!hasInstallDate()) {
        setInstallDate();
    }

    String lines[STARTUP_GREETING_LINES_MAX];
    uint8_t count = 0;
    buildDisplayLines(lines, count);

    displayManager.showStartupGreeting(lines, count);

    LOG_INFO(kLogCategory, "Startup greeting started (%lu ms display)", m_displayDuration);
}

bool StartupGreetingManager::isActive() const noexcept {
    return m_active;
}

StartupGreetingSettings StartupGreetingManager::getSettings() const noexcept {
    return m_settings;
}

bool StartupGreetingManager::updateSettings(const StartupGreetingSettings& settings) noexcept {
    m_settings = settings;
    saveSettings();
    LOG_INFO(kLogCategory, "Settings updated");
    return true;
}

void StartupGreetingManager::loadSettings() noexcept {
    m_settings.enabled = m_prefs.getBool("enabled", true);
    m_settings.speakGreeting = m_prefs.getBool("speak", true);
    m_settings.showMark = m_prefs.getBool("showMark", true);
    m_settings.showCodename = m_prefs.getBool("showCode", true);
    m_settings.showSemVer = m_prefs.getBool("showSemVer", true);
    m_settings.showBuildDate = m_prefs.getBool("showBDate", true);
    m_settings.showAge = m_prefs.getBool("showAge", true);
    m_settings.showPersonality = m_prefs.getBool("showPers", true);
    m_settings.showWifi = m_prefs.getBool("showWifi", true);
    m_settings.showStorage = m_prefs.getBool("showStor", true);
    m_settings.displayDurationSec = m_prefs.getUChar("duration", 4);
    if (m_settings.displayDurationSec < 2) m_settings.displayDurationSec = 2;
    if (m_settings.displayDurationSec > 15) m_settings.displayDurationSec = 15;
}

void StartupGreetingManager::saveSettings() noexcept {
    m_prefs.putBool("enabled", m_settings.enabled);
    m_prefs.putBool("speak", m_settings.speakGreeting);
    m_prefs.putBool("showMark", m_settings.showMark);
    m_prefs.putBool("showCode", m_settings.showCodename);
    m_prefs.putBool("showSemVer", m_settings.showSemVer);
    m_prefs.putBool("showBDate", m_settings.showBuildDate);
    m_prefs.putBool("showAge", m_settings.showAge);
    m_prefs.putBool("showPers", m_settings.showPersonality);
    m_prefs.putBool("showWifi", m_settings.showWifi);
    m_prefs.putBool("showStor", m_settings.showStorage);
    m_prefs.putUChar("duration", m_settings.displayDurationSec);
}

String StartupGreetingManager::getTimeBasedGreeting() noexcept {
    const String tod = getTimeOfDay();
    if (tod == "morning") return "Good morning.";
    if (tod == "afternoon") return "Good afternoon.";
    if (tod == "evening") return "Good evening.";
    if (tod == "night") return "Good evening.";
    return "Hello.";
}

String StartupGreetingManager::getTimeOfDay() noexcept {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) return "";
    const int h = timeinfo.tm_hour;
    if (h < 12) return "morning";
    if (h < 17) return "afternoon";
    if (h < 21) return "evening";
    return "night";
}

String StartupGreetingManager::getDayOfWeek() noexcept {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) return "";
    static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (timeinfo.tm_wday >= 0 && timeinfo.tm_wday <= 6) {
        return days[timeinfo.tm_wday];
    }
    return "";
}

String StartupGreetingManager::formatDate() noexcept {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) return "";
    static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char buf[24];
    const char* mon = (timeinfo.tm_mon >= 0 && timeinfo.tm_mon <= 11) ? months[timeinfo.tm_mon] : "???";
    snprintf(buf, sizeof(buf), "%s %d %s %d", getDayOfWeek().c_str(), timeinfo.tm_mday, mon, timeinfo.tm_year + 1900);
    return String(buf);
}

String StartupGreetingManager::formatTime() noexcept {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) return "";
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buf);
}

String StartupGreetingManager::getSystemAgeText() noexcept {
    if (!hasInstallDate()) return "";
    const unsigned long installEpoch = getInstallDate();
    if (installEpoch == 0) return "";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) return "";

    const time_t nowEpoch = mktime(&timeinfo);
    if (nowEpoch < static_cast<time_t>(installEpoch)) return "New";

    const long days = (nowEpoch - static_cast<time_t>(installEpoch)) / 86400L;
    if (days == 0) return "First day";
    if (days == 1) return "1 day";

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld days", days);
    return String(buf);
}

String StartupGreetingManager::getPersonalityName() noexcept {
    if (!personalityManager.isInitialized()) return "";
    const PersonalityProfile& profile = personalityManager.getActiveProfile();
    if (profile.name.isEmpty()) return "Jarvis";
    return profile.name;
}

void StartupGreetingManager::buildDisplayLines(String lines[], uint8_t& count) noexcept {
    count = 0;

    lines[count++] = AURA_OS_NAME;

    if (m_settings.showMark && m_settings.showCodename) {
        char line[22];
        snprintf(line, sizeof(line), "MARK %s - %s", AURA_MARK_ROMAN, aura::version::kCodename);
        lines[count++] = String(line);
    } else {
        if (m_settings.showMark) {
            char line[18];
            snprintf(line, sizeof(line), "MARK %s", AURA_MARK_ROMAN);
            lines[count++] = String(line);
        }
        if (m_settings.showCodename) {
            lines[count++] = aura::version::kCodename;
        }
    }

    if (m_settings.showSemVer) {
        char verLine[14];
        snprintf(verLine, sizeof(verLine), "v%s", AURA_SEMVER);
        lines[count++] = String(verLine);
    }

    if (m_settings.showBuildDate) {
        lines[count++] = getBuildDateText();
    }

    if (m_settings.showAge) {
        const String age = getSystemAgeText();
        if (!age.isEmpty()) {
            lines[count++] = "Age: " + age;
        }
    }

    if (m_settings.showPersonality) {
        lines[count++] = "Profile: " + getPersonalityName();
    }

    if (m_settings.showWifi) {
        lines[count++] = getWifiStatus();
    }

    if (m_settings.showStorage) {
        lines[count++] = getStorageStatus();
    }

    if (count > STARTUP_GREETING_LINES_MAX) {
        count = STARTUP_GREETING_LINES_MAX;
    }
}

String StartupGreetingManager::buildSpeechText() noexcept {
    String text;
    text += getTimeBasedGreeting();
    text += " Running AURA Operating System, Mark ";
    text += AURA_MARK_WORDS;
    text += ". ";
    const String age = getSystemAgeText();
    if (!age.isEmpty()) {
        text += "System age " + age + ". ";
    }
    text += "Active profile: " + getPersonalityName() + ".";
    return text;
}

bool StartupGreetingManager::hasInstallDate() noexcept {
    return m_prefs.isKey("installDate");
}

unsigned long StartupGreetingManager::getInstallDate() noexcept {
    return m_prefs.getULong("installDate", 0);
}

void StartupGreetingManager::setInstallDate() noexcept {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) {
        m_prefs.putULong("installDate", 0);
        return;
    }
    const time_t epoch = mktime(&timeinfo);
    if (epoch > 0) {
        m_prefs.putULong("installDate", static_cast<unsigned long>(epoch));
        LOG_INFO(kLogCategory, "Install date set to %lu", static_cast<unsigned long>(epoch));
    }
}

String StartupGreetingManager::getBuildDateText() noexcept {
    char buf[20];
    snprintf(buf, sizeof(buf), "Built: %s", aura::version::kBuildDate);
    return String(buf);
}

String StartupGreetingManager::getWifiStatus() noexcept {
    if (wifiManager.isConnected()) {
        return "Wi-Fi: Connected";
    }
    return "Wi-Fi: Offline";
}

String StartupGreetingManager::getStorageStatus() noexcept {
    if (storageManager.isSDMounted()) {
        return "SD: Ready";
    }
    if (storageManager.isSPIFFSMounted()) {
        return "Storage: Internal";
    }
    return "Storage: N/A";
}

