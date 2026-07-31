#include "display_manager.h"
#include <Wire.h>
#include <math.h>

DisplayManager displayManager;

DisplayManager::DisplayManager() noexcept
    : m_display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET)
    , m_currentState(DisplayState::BOOT)
    , m_previousState(DisplayState::BOOT)
    , m_lastRenderedState(DisplayState::BOOT)
    , m_initialized(false)
    , m_sleeping(false)
    , m_screenDirty(true)
    , m_lastUpdateTime(0)
    , m_lastRefreshTime(0)
    , m_stateStartTime(0)
    , m_notificationTimeout(0)
    , m_lastActivityTime(0)
    , m_brightness(m_defaultBrightness)
    , m_contrast(m_defaultContrast)
    , m_inverted(false)
    , m_nightMode(false)
    , m_rotation(0)
    , m_animationFrame(0)
    , m_cachedTitle("")
    , m_cachedMessage("")
    , m_cachedFooter("")
    , m_cachedSSID("")
    , m_cachedStorageType("")
    , m_cachedWifiConnected(false)
    , m_cachedSignal(0)
    , m_cachedUsedMB(0)
    , m_cachedTotalMB(0)
    , m_cachedOTAProgress(0)
    , m_cachedBootProgress(0)
    , m_cachedGreetingLineCount(0)
    , m_transitionActive(false)
    , m_transitionStartTime(0)
    , m_transitionFrom(DisplayState::HOME)
    , m_transitionTo(DisplayState::HOME)
    , m_homePersonality("")
    , m_homeActivity("")
    , m_homeConversations(0)
    , m_homeMemories(0)
    , m_homeReminders(0)
    , m_micMuted(true)
    , m_dashboardDirty(true)
    , m_lastDashboardRefresh(0) {
    for (auto& line : m_cachedGreetingLines) {
        line = String();
    }
    // Enable all widgets by default
    for (auto& w : m_widgetEnabled) w = true;
}

DisplayManager::~DisplayManager() noexcept = default;

bool DisplayManager::initialize() noexcept {
    if (m_initialized) {
        return true;
    }

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!m_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        LOG_ERROR("DisplayManager", "SSD1306 initialization failed");
        return false;
    }

    m_display.clearDisplay();
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setTextWrap(false);
    m_display.cp437(true);

    // Mark initialized before calling setter functions
m_initialized = true;

    setBrightness(m_brightness);
    setContrast(m_contrast);
    setRotation(m_rotation);
    setInverted(m_inverted);

    
    m_lastUpdateTime = millis();
    m_lastRefreshTime = millis();
    m_stateStartTime = millis();
    m_lastActivityTime = millis();

    LOG_INFO("DisplayManager", "DisplayManager initialized");
    return true;
}

void DisplayManager::run() noexcept {
    update();
}

void DisplayManager::update() noexcept {
    if (!m_initialized || m_sleeping) {
        return;
    }

    const unsigned long now = millis();

    updateAnimation();
    updateScreenTimeout();

    if (m_transitionActive) {
        renderTransition();
        m_lastRefreshTime = now;
        return;
    }

    bool forceHomeRedraw = (m_currentState == DisplayState::HOME && m_dashboardDirty);

    if (m_screenDirty || forceHomeRedraw || (now - m_lastRefreshTime >= m_refreshIntervalMs)) {
        if (m_currentState != m_lastRenderedState || m_screenDirty || forceHomeRedraw) {
            if (forceHomeRedraw) {
                m_dashboardDirty = false;
            }
            switch (m_currentState) {
                case DisplayState::BOOT:
                    renderBoot(m_cachedBootProgress);
                    break;
                case DisplayState::HOME:
                    renderHome();
                    break;
                case DisplayState::LISTENING:
                    renderListening();
                    break;
                case DisplayState::THINKING:
                    renderThinking();
                    break;
                case DisplayState::SPEAKING:
                    renderSpeaking();
                    break;
                case DisplayState::REMINDER:
                    renderReminder();
                    break;
                case DisplayState::NOTIFICATION:
                    renderNotification();
                    break;
                case DisplayState::ERROR:
                    renderError();
                    break;
                case DisplayState::OTA:
                    renderOTAProgress();
                    break;
                case DisplayState::SLEEP:
                    renderSleep();
                    break;
                case DisplayState::STARTUP_GREETING:
                    renderStartupGreeting();
                    break;
            }
            m_lastRenderedState = m_currentState;
            m_screenDirty = false;
        }
        m_display.display();
        m_lastRefreshTime = now;
    }
}

void DisplayManager::clear() noexcept {
    if (!m_initialized) return;
    m_display.clearDisplay();
    m_screenDirty = true;
}

void DisplayManager::reset() noexcept {
    if (!m_initialized) return;
    changeState(DisplayState::BOOT);
    m_cachedTitle.clear();
    m_cachedMessage.clear();
    m_cachedFooter.clear();
    m_cachedSSID.clear();
    m_cachedStorageType.clear();
    m_cachedWifiConnected = false;
    m_cachedSignal = 0;
    m_cachedUsedMB = 0;
    m_cachedTotalMB = 0;
    m_cachedOTAProgress = 0;
    m_cachedBootProgress = 0;
    for (auto& line : m_cachedGreetingLines) {
        line = String();
    }
    m_cachedGreetingLineCount = 0;
    m_notificationTimeout = 0;
    m_animationFrame = 0;
    m_screenDirty = true;
}

void DisplayManager::refresh() noexcept {
    if (!m_initialized || m_sleeping) return;
    m_display.display();
}

void DisplayManager::forceRefresh() noexcept {
    if (!m_initialized) return;
    m_screenDirty = true;
    m_lastRenderedState = static_cast<DisplayState>(255);
    update();
}

void DisplayManager::setState(DisplayState newState) noexcept {
    if (newState != m_currentState) {
        startTransition(m_currentState, newState);
    }
    changeState(newState);
}

DisplayState DisplayManager::getState() const noexcept {
    return m_currentState;
}

bool DisplayManager::isInitialized() const noexcept {
    return m_initialized;
}

void DisplayManager::showSplash() noexcept {
    if (!m_initialized) return;
    changeState(DisplayState::BOOT);
    m_cachedBootProgress = 0;
    m_screenDirty = true;
}

void DisplayManager::showBoot(uint8_t progress) noexcept {
    if (!m_initialized) return;
    if (m_currentState != DisplayState::BOOT) {
        changeState(DisplayState::BOOT);
    }
    m_cachedBootProgress = progress;
    m_screenDirty = true;
}

void DisplayManager::showHome() noexcept {
    if (!m_initialized) return;
    changeState(DisplayState::HOME);
    m_screenDirty = true;
}

void DisplayManager::showListening() noexcept {
    if (!m_initialized) return;
    changeState(DisplayState::LISTENING);
    m_screenDirty = true;
}

void DisplayManager::showThinking() noexcept {
    if (!m_initialized) return;
    changeState(DisplayState::THINKING);
    m_screenDirty = true;
}

void DisplayManager::showSpeaking() noexcept {
    if (!m_initialized) return;
    changeState(DisplayState::SPEAKING);
    m_screenDirty = true;
}

void DisplayManager::showReminder(const String& title, const String& message) noexcept {
    if (!m_initialized) return;
    m_cachedTitle = title;
    m_cachedMessage = message;
    changeState(DisplayState::REMINDER);
    m_screenDirty = true;
}

void DisplayManager::showNotification(const String& title, const String& message, unsigned long durationMs) noexcept {
    if (!m_initialized) return;
    m_cachedTitle = title;
    m_cachedMessage = message;
    m_notificationTimeout = durationMs ? (millis() + durationMs) : 0;
    changeState(DisplayState::NOTIFICATION);
    m_screenDirty = true;
}

void DisplayManager::showError(const String& title, const String& message) noexcept {
    if (!m_initialized) return;
    m_cachedTitle = title;
    m_cachedMessage = message;
    changeState(DisplayState::ERROR);
    m_screenDirty = true;
}

void DisplayManager::showOTAProgress(uint8_t progress) noexcept {
    if (!m_initialized) return;
    if (m_currentState != DisplayState::OTA) {
        changeState(DisplayState::OTA);
    }
    m_cachedOTAProgress = progress;
    m_screenDirty = true;
}

void DisplayManager::showWifiStatus(bool connected, const String& ssid, int32_t signal) noexcept {
    if (!m_initialized) return;
    m_cachedWifiConnected = connected;
    m_cachedSSID = ssid;
    m_cachedSignal = signal;
    changeState(DisplayState::HOME);
    m_screenDirty = true;
}

void DisplayManager::showStorageStatus(const String& storageType, uint32_t usedMB, uint32_t totalMB) noexcept {
    if (!m_initialized) return;
    m_cachedStorageType = storageType;
    m_cachedUsedMB = usedMB;
    m_cachedTotalMB = totalMB;
    changeState(DisplayState::HOME);
    m_screenDirty = true;
}

void DisplayManager::setStateLabel(const DisplayState state, const String& label) noexcept {
    const uint8_t idx = static_cast<uint8_t>(state);
    if (idx < 8) {
        m_stateLabels[idx] = label;
    }
}

void DisplayManager::showMessage(const String& title, const String& body, const String& footer) noexcept {
    if (!m_initialized) return;
    m_cachedTitle = title;
    m_cachedMessage = body;
    m_cachedFooter = footer;
    changeState(DisplayState::HOME);
    m_screenDirty = true;
}

void DisplayManager::showStartupGreeting(const String lines[], uint8_t count) noexcept {
    if (!m_initialized) return;
    const uint8_t maxLines = STARTUP_GREETING_LINES_MAX;
    m_cachedGreetingLineCount = (count > maxLines) ? maxLines : count;
    for (uint8_t i = 0; i < m_cachedGreetingLineCount; ++i) {
        m_cachedGreetingLines[i] = lines[i];
    }
    changeState(DisplayState::STARTUP_GREETING);
    m_screenDirty = true;
}

void DisplayManager::setBrightness(uint8_t brightness) noexcept {
    if (!m_initialized) return;
    m_brightness = brightness;
    m_display.ssd1306_command(SSD1306_SETCONTRAST);
    m_display.ssd1306_command(brightness);
    LOG_DEBUG("DisplayManager", "Brightness set to %d", brightness);
}

void DisplayManager::sleep() noexcept {
    if (!m_initialized || m_sleeping) return;
    m_sleeping = true;
    m_display.clearDisplay();
    m_display.display();
    m_display.ssd1306_command(SSD1306_DISPLAYOFF);
    LOG_INFO("DisplayManager", "Display sleep");
}

void DisplayManager::wake() noexcept {
    if (!m_initialized || !m_sleeping) return;
    m_sleeping = false;
    m_display.ssd1306_command(SSD1306_DISPLAYON);
    m_screenDirty = true;
    m_lastActivityTime = millis();
    LOG_INFO("DisplayManager", "Display wake");
}

void DisplayManager::displayOn() noexcept {
    if (!m_initialized) return;
    m_display.ssd1306_command(SSD1306_DISPLAYON);
}

void DisplayManager::displayOff() noexcept {
    if (!m_initialized) return;
    m_display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void DisplayManager::setNightMode(bool enabled) noexcept {
    if (m_nightMode == enabled) return;
    m_nightMode = enabled;
    if (enabled) {
        setBrightness(30);
        setContrast(100);
    } else {
        setBrightness(m_defaultBrightness);
        setContrast(m_defaultContrast);
    }
    LOG_INFO("DisplayManager", "Night mode %s", enabled ? "enabled" : "disabled");
}

bool DisplayManager::isNightMode() const noexcept {
    return m_nightMode;
}

void DisplayManager::setAutoBrightness(uint16_t ambientLight) noexcept {
    // Map 0-1023 ambient light to 20-255 brightness
    uint8_t targetBrightness = map(constrain(ambientLight, 0, 1023), 0, 1023, 20, 255);
    if (m_nightMode && targetBrightness > 30) targetBrightness = 30;
    setBrightness(targetBrightness);
}

bool DisplayManager::isSleeping() const noexcept {
    return m_sleeping;
}

bool DisplayManager::isAwake() const noexcept {
    return !m_sleeping;
}

void DisplayManager::setRotation(uint8_t rotation) noexcept {
    if (!m_initialized) return;
    m_rotation = rotation & 0x03;
    m_display.setRotation(m_rotation);
    m_screenDirty = true;
    LOG_DEBUG("DisplayManager", "Rotation set to %d", m_rotation);
}

void DisplayManager::setContrast(uint8_t contrast) noexcept {
    if (!m_initialized) return;
    m_contrast = contrast;
    m_display.ssd1306_command(SSD1306_SETCONTRAST);
    m_display.ssd1306_command(contrast);
    LOG_DEBUG("DisplayManager", "Contrast set to %d", contrast);
}

void DisplayManager::setInverted(bool inverted) noexcept {
    if (!m_initialized) return;
    m_inverted = inverted;
    m_display.ssd1306_command(inverted ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
    LOG_DEBUG("DisplayManager", "Inverted set to %s", inverted ? "true" : "false");
}

uint16_t DisplayManager::getWidth() const noexcept {
    return m_displayWidth;
}

uint16_t DisplayManager::getHeight() const noexcept {
    return m_displayHeight;
}

void DisplayManager::changeState(DisplayState newState) noexcept {
    if (m_currentState == newState) return;

    m_previousState = m_currentState;
    m_currentState = newState;
    m_stateStartTime = millis();
    m_animationFrame = 0;
    m_screenDirty = true;
    m_lastActivityTime = millis();
}

bool DisplayManager::startTransition(DisplayState from, DisplayState to) noexcept {
    // Only animate between conversation states
    if (from != DisplayState::LISTENING && from != DisplayState::THINKING && from != DisplayState::SPEAKING) {
        return false;
    }
    if (to != DisplayState::LISTENING && to != DisplayState::THINKING && to != DisplayState::SPEAKING) {
        return false;
    }
    m_transitionActive = true;
    m_transitionStartTime = millis();
    m_transitionFrom = from;
    m_transitionTo = to;
    return true;
}

void DisplayManager::renderTransition() noexcept {
    const unsigned long elapsed = millis() - m_transitionStartTime;
    constexpr unsigned long kDurationMs = 180;
    uint8_t progress = (elapsed >= kDurationMs) ? 255 : static_cast<uint8_t>((elapsed * 255U) / kDurationMs);

    m_display.clearDisplay();

    const uint8_t cx = m_displayWidth / 2;
    const uint8_t cy = m_displayHeight / 2;
    const uint8_t maxRadius = (m_displayWidth < m_displayHeight ? m_displayWidth : m_displayHeight) / 2;
    const uint8_t ringRadius = static_cast<uint8_t>((static_cast<uint16_t>(progress) * maxRadius) / 255);
    // Expanding double-ring wipe
    if (ringRadius > 2) {
        m_display.drawCircle(cx, cy, ringRadius, SSD1306_WHITE);
        if (ringRadius > 6) {
            m_display.drawCircle(cx, cy, ringRadius - 3, SSD1306_WHITE);
        }
    }
    m_display.display();

    if (elapsed >= kDurationMs) {
        m_transitionActive = false;
        m_lastRenderedState = static_cast<DisplayState>(255); // force full re-render
        m_screenDirty = true;
    }
}

void DisplayManager::renderBoot(uint8_t progress) noexcept {
    m_display.clearDisplay();

    const uint8_t frame = m_animationFrame;
    const float pulse = 1.0f + 0.05f * sinf((float)(frame % 64) * 3.14159f / 32.0f);
    const uint8_t titleSize = (uint8_t)(2.5f * pulse);

    drawCenteredText("AURA", (uint8_t)(4 * pulse), 2);

    char markLine[16];
    snprintf(markLine, sizeof(markLine), "MARK %s", AURA_MARK_ROMAN);
    drawCenteredText(markLine, 20, 1);

    drawCenteredText(aura::version::kCodename, 30, 1);

    char verLine[20];
    snprintf(verLine, sizeof(verLine), "v%s", aura::version::kSemVer);
    drawCenteredText(verLine, 40, 1);

    drawProgressBar(14, 48, 100, 8, progress);
    if (progress > 0 && progress < 100) {
        const uint8_t scanX = 14 + (frame % 100) * 100 / 100;
        m_display.drawLine(scanX, 48, scanX, 56, SSD1306_WHITE);
    }

    char pctStr[6];
    snprintf(pctStr, sizeof(pctStr), "%d%%", progress);
    drawCenteredText(pctStr, 58, 1);

    uint8_t dots = (frame / 4) % 4;
    char loadingStr[16];
    snprintf(loadingStr, sizeof(loadingStr), "INITIALIZING");
    for (uint8_t i = 0; i < dots; ++i) {
        uint8_t dotX = 64 + 24 + i * 6;
        m_display.fillCircle(dotX, 58, 1, SSD1306_WHITE);
    }
}

void DisplayManager::updateHomeData(const String& personalityName, const String& lastActivity,
                                     size_t conversationsToday, size_t memoryCount, size_t reminderCount) noexcept {
    m_homePersonality = personalityName;
    m_homeActivity = lastActivity;
    m_homeConversations = conversationsToday;
    m_homeMemories = memoryCount;
    m_homeReminders = reminderCount;
}

void DisplayManager::setMicMuted(bool muted) noexcept {
    m_micMuted = muted;
}

void DisplayManager::renderHome() noexcept {
    m_display.clearDisplay();
    drawStatusBar();

    uint8_t y = 11;
    for (int i = 0; i < static_cast<int>(WidgetType::COUNT); ++i) {
        if (m_widgetEnabled[i]) {
            renderWidget(static_cast<WidgetType>(i), y);
        }
        if (y >= 60) break;
    }
}

void DisplayManager::renderDashboard() noexcept {
    renderHome();
}

void DisplayManager::renderWidget(WidgetType widget, uint8_t& y) noexcept {
    switch (widget) {
        case WidgetType::CLOCK:          renderClockWidget(y); break;
        case WidgetType::GREETING:       renderGreetingWidget(y); break;
        case WidgetType::NEXT_REMINDER:  renderNextReminderWidget(y); break;
        case WidgetType::PROJECT_STATUS: renderProjectStatusWidget(y); break;
        case WidgetType::TODAYS_STUDY:   renderStudyWidget(y); break;
        case WidgetType::DAILY_PROGRESS: renderProgressWidget(y); break;
        case WidgetType::NOTIFICATIONS:  renderNotificationsWidget(y); break;
        case WidgetType::WIFI_STATUS:    renderWifiWidget(y); break;
        case WidgetType::HEAP_USAGE:     renderHeapWidget(y); break;
        case WidgetType::STORAGE_USAGE:  renderStorageWidget(y); break;
        case WidgetType::ACTIVE_CONTEXT: renderContextWidget(y); break;
        default: break;
    }
}

void DisplayManager::renderClockWidget(uint8_t& y) noexcept {
    if (m_widgetData.clockStr.isEmpty()) return;
    m_display.setTextSize(2);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print(m_widgetData.clockStr);
    y += 18;
}

void DisplayManager::renderGreetingWidget(uint8_t& y) noexcept {
    if (m_widgetData.greetingStr.isEmpty()) return;
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    String g = m_widgetData.greetingStr;
    if (g.length() > 20) g = g.substring(0, 18) + "...";
    m_display.print(g);
    y += 10;
}

void DisplayManager::renderNextReminderWidget(uint8_t& y) noexcept {
    if (m_widgetData.nextReminder.isEmpty()) return;
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print("R:");
    String r = m_widgetData.nextReminder;
    if (r.length() > 18) r = r.substring(0, 17) + "...";
    m_display.print(r);
    y += 10;
}

void DisplayManager::renderProjectStatusWidget(uint8_t& y) noexcept {
    if (m_widgetData.projectStatus.isEmpty()) return;
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print("P:");
    String p = m_widgetData.projectStatus;
    if (p.length() > 18) p = p.substring(0, 17) + "...";
    m_display.print(p);
    y += 10;
}

void DisplayManager::renderStudyWidget(uint8_t& y) noexcept {
    if (m_widgetData.studySummary.isEmpty()) return;
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print("S:");
    String s = m_widgetData.studySummary;
    if (s.length() > 18) s = s.substring(0, 17) + "...";
    m_display.print(s);
    y += 10;
}

void DisplayManager::renderProgressWidget(uint8_t& y) noexcept {
    if (m_widgetData.progressSummary.isEmpty()) return;
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print(m_widgetData.progressSummary);
    y += 10;
}

void DisplayManager::renderNotificationsWidget(uint8_t& y) noexcept {
    if (m_widgetData.notificationCount == 0) return;
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print("N:" + String(m_widgetData.notificationCount));
    y += 10;
}

void DisplayManager::renderWifiWidget(uint8_t& y) noexcept {
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    if (m_widgetData.wifiConnected) {
        m_display.print("WiFi:" + String(m_widgetData.wifiRSSI) + "dBm");
    } else {
        m_display.print("WiFi:OFF");
    }
    y += 10;
}

void DisplayManager::renderHeapWidget(uint8_t& y) noexcept {
    uint32_t freeKB = m_widgetData.freeHeap / 1024;
    uint32_t totalKB = m_widgetData.totalHeap / 1024;
    if (totalKB == 0) totalKB = 1;
    uint8_t pct = static_cast<uint8_t>((freeKB * 100UL) / totalKB);
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print("RAM:" + String(freeKB) + "K/" + String(totalKB) + "K " + String(pct) + "%");
    y += 10;
}

void DisplayManager::renderStorageWidget(uint8_t& y) noexcept {
    uint32_t usedMB = m_widgetData.usedStorageKB / 1024;
    uint32_t totalMB = m_widgetData.totalStorageKB / 1024;
    if (totalMB == 0) return;
    uint8_t pct = static_cast<uint8_t>((usedMB * 100UL) / totalMB);
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print("SD:" + String(usedMB) + "M/" + String(totalMB) + "M " + String(pct) + "%");
    y += 10;
}

void DisplayManager::renderContextWidget(uint8_t& y) noexcept {
    if (m_widgetData.contextName.isEmpty()) return;
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, y);
    m_display.print("Mode:" + m_widgetData.contextName);
    y += 10;
}

void DisplayManager::setWidgetEnabled(WidgetType widget, bool enabled) noexcept {
    m_widgetEnabled[static_cast<size_t>(widget)] = enabled;
    m_dashboardDirty = true;
}

bool DisplayManager::isWidgetEnabled(WidgetType widget) const noexcept {
    return m_widgetEnabled[static_cast<size_t>(widget)];
}

void DisplayManager::updateWidgetData(WidgetType widget, const String& data) noexcept {
    unsigned long now = millis();
    size_t idx = static_cast<size_t>(widget);
    switch (widget) {
        case WidgetType::CLOCK:          m_widgetData.clockStr = data; break;
        case WidgetType::GREETING:       m_widgetData.greetingStr = data; break;
        case WidgetType::NEXT_REMINDER:  m_widgetData.nextReminder = data; break;
        case WidgetType::PROJECT_STATUS: m_widgetData.projectStatus = data; break;
        case WidgetType::TODAYS_STUDY:   m_widgetData.studySummary = data; break;
        case WidgetType::DAILY_PROGRESS: m_widgetData.progressSummary = data; break;
        case WidgetType::ACTIVE_CONTEXT: m_widgetData.contextName = data; break;
        default: break;
    }
    m_widgetData.lastUpdated[idx] = now;
    m_dashboardDirty = true;
}

void DisplayManager::setDashboardNumericData(uint32_t freeHeap, uint32_t totalHeap,
                                              uint32_t usedKB, uint32_t totalKB,
                                              int32_t rssi, bool wifiConnected) noexcept {
    m_widgetData.freeHeap = freeHeap;
    m_widgetData.totalHeap = totalHeap;
    m_widgetData.usedStorageKB = usedKB;
    m_widgetData.totalStorageKB = totalKB;
    m_widgetData.wifiRSSI = rssi;
    m_widgetData.wifiConnected = wifiConnected;
    m_dashboardDirty = true;
}

void DisplayManager::refreshDashboard() noexcept {
    m_dashboardDirty = true;
}

void DisplayManager::renderListening() noexcept {
    m_display.clearDisplay();

    const uint8_t cx = m_displayWidth / 2;
    const uint8_t cy = m_displayHeight / 2 - 4;
    const uint8_t frame = m_animationFrame;

    drawMicrophone(cx, cy);

    const uint8_t breathFrame = frame % 20;
    const uint8_t breathRadius = 16 + (breathFrame < 10 ? breathFrame : 19 - breathFrame);

    m_display.drawCircle(cx, cy, breathRadius, SSD1306_WHITE);

    drawWave(cx, cy + 14, 4, frame, 5);

    const uint8_t orbitFrame = frame % 16;
    for (uint8_t i = 0; i < 3; ++i) {
        const float angle = (float)(frame * 3 + i * 120) * 3.14159f / 180.0f;
        const uint8_t r = breathRadius + 3;
        const uint8_t dx = (uint8_t)((float)r * cosf(angle));
        const uint8_t dy = (uint8_t)((float)r * sinf(angle));
        m_display.drawPixel(cx + dx, cy + dy, SSD1306_WHITE);
    }

    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    const String& listeningLabel = m_stateLabels[static_cast<uint8_t>(DisplayState::LISTENING)];
    const char* listeningText = listeningLabel.isEmpty() ? "LISTENING" : listeningLabel.c_str();
    m_display.getTextBounds(listeningText, 0, 0, &x1, &y1, &w, &h);
    m_display.setCursor((m_displayWidth - w) / 2, 56);
    m_display.print(listeningText);
}

void DisplayManager::renderThinking() noexcept {
    m_display.clearDisplay();

    const uint8_t cx = m_displayWidth / 2;
    const uint8_t cy = m_displayHeight / 2 - 4;
    const uint8_t frame = m_animationFrame;

    drawThinkingCore(cx, cy, frame);

    drawRing(cx, cy, 14, 2, frame);

    const uint8_t pulseFrame = frame % 20;
    const uint8_t pulseR = 18 + (pulseFrame < 10 ? pulseFrame : 19 - pulseFrame);
    m_display.drawCircle(cx, cy, pulseR, SSD1306_WHITE);

    for (uint8_t i = 0; i < 4; ++i) {
        const float angle = (float)(frame * 5 + i * 90) * 3.14159f / 180.0f;
        const uint8_t r = 20 + ((frame + i * 3) % 8);
        const uint8_t dx = (uint8_t)((float)r * cosf(angle));
        const uint8_t dy = (uint8_t)((float)r * sinf(angle));
        m_display.drawPixel(cx + dx, cy + dy, SSD1306_WHITE);
    }

    int16_t x1, y1;
    uint16_t w, h;
    const String& thinkingLabel = m_stateLabels[static_cast<uint8_t>(DisplayState::THINKING)];
    const char* thinkingText = thinkingLabel.isEmpty() ? "THINKING" : thinkingLabel.c_str();
    m_display.getTextBounds(thinkingText, 0, 0, &x1, &y1, &w, &h);
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor((m_displayWidth - w) / 2, 56);
    m_display.print(thinkingText);
}

void DisplayManager::renderSpeaking() noexcept {
    m_display.clearDisplay();

    const uint8_t cx = m_displayWidth / 2;
    const uint8_t frame = m_animationFrame;

    drawEqualizer(8, 14, 9, 22, frame);

    const uint8_t pulseFrame = frame % 16;
    const uint8_t pulseR = 3 + (pulseFrame < 8 ? pulseFrame : 15 - pulseFrame);
    m_display.drawCircle(cx, 34, pulseR, SSD1306_WHITE);

    int16_t x1, y1;
    uint16_t w, h;
    const String& speakingLabel = m_stateLabels[static_cast<uint8_t>(DisplayState::SPEAKING)];
    const char* speakingText = speakingLabel.isEmpty() ? "SPEAKING" : speakingLabel.c_str();
    m_display.getTextBounds(speakingText, 0, 0, &x1, &y1, &w, &h);
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor((m_displayWidth - w) / 2, 54);
    m_display.print(speakingText);
}

void DisplayManager::renderReminder() noexcept {
    m_display.clearDisplay();

    const uint8_t cx = m_displayWidth / 2;
    const uint8_t frame = m_animationFrame;

    const uint8_t pulseFrame = frame % 12;
    const uint8_t pulseR = 26 + (pulseFrame < 6 ? pulseFrame : 11 - pulseFrame);
    m_display.drawCircle(cx, 22, pulseR, SSD1306_WHITE);

    drawBell(cx, 22, frame);

    if (!m_cachedTitle.isEmpty()) {
        m_display.setTextSize(1);
        m_display.setTextColor(SSD1306_WHITE);
        int16_t x1, y1;
        uint16_t w, h;
        m_display.getTextBounds(m_cachedTitle.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 36);
        m_display.print(m_cachedTitle);
    }

    if (!m_cachedMessage.isEmpty()) {
        m_display.setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        m_display.getTextBounds(m_cachedMessage.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 46);
        m_display.print(m_cachedMessage);
    }

    const uint8_t countdown = (frame / 4) % 8;
    for (uint8_t i = 0; i < 8; ++i) {
        const float angle = (float)i * 6.2832f / 8.0f;
        const uint8_t dx = (uint8_t)(30.0f * cosf(angle));
        const uint8_t dy = (uint8_t)(30.0f * sinf(angle));
        if (i < countdown) {
            m_display.drawPixel(cx + dx, 22 + dy, SSD1306_WHITE);
        }
    }
}

void DisplayManager::renderNotification() noexcept {
    m_display.clearDisplay();

    const uint8_t frame = m_animationFrame;

    const uint8_t slideIn = (frame < 8) ? (8 - frame) * 2 : 0;
    const uint8_t boxX = 4 - slideIn;
    const uint8_t boxY = 4;
    const uint8_t boxW = m_displayWidth - 8;
    const uint8_t boxH = m_displayHeight - 8;

    drawCard(boxX, boxY, boxW, boxH);

    m_display.fillRect(boxX, boxY, boxW, 8, SSD1306_WHITE);

    if (!m_cachedTitle.isEmpty()) {
        m_display.setTextSize(1);
        m_display.setTextColor(SSD1306_BLACK);
        int16_t x1, y1;
        uint16_t w, h;
        m_display.getTextBounds(m_cachedTitle.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor(boxX + 4, boxY + 1);
        m_display.print(m_cachedTitle);
    }

    if (!m_cachedMessage.isEmpty()) {
        m_display.setTextSize(1);
        m_display.setTextColor(SSD1306_WHITE);
        m_display.setCursor(boxX + 4, boxY + 12);
        m_display.print(m_cachedMessage);
    }

    const uint8_t barX = boxX + boxW - 8;
    const uint8_t barY = boxY + boxH - 4;
    const uint8_t barW = 6;
    const uint8_t progress = (frame % 16) * barW / 16;
    m_display.fillRect(barX, barY, progress, 2, SSD1306_WHITE);

    if (m_notificationTimeout && millis() >= m_notificationTimeout) {
        changeState(DisplayState::HOME);
    }
}

void DisplayManager::renderError() noexcept {
    m_display.clearDisplay();

    const uint8_t cx = m_displayWidth / 2;
    const uint8_t frame = m_animationFrame;

    const bool blink = (frame / 4) % 2 == 0;
    if (blink) {
        drawCard(2, 2, m_displayWidth - 4, m_displayHeight - 4);
    }

    const uint8_t xSize = 14 + ((frame % 6) < 3 ? 0 : 2);
    const uint8_t cy = 18;
    m_display.drawLine(cx - xSize, cy - xSize, cx + xSize, cy + xSize, SSD1306_WHITE);
    m_display.drawLine(cx - xSize, cy + xSize, cx + xSize, cy - xSize, SSD1306_WHITE);

    const uint8_t warnFrame = frame % 8;
    const uint8_t warnR = 18 + (warnFrame < 4 ? warnFrame : 7 - warnFrame);
    m_display.drawCircle(cx, cy, warnR, SSD1306_WHITE);

    if (!m_cachedTitle.isEmpty()) {
        m_display.setTextSize(1);
        m_display.setTextColor(SSD1306_WHITE);
        int16_t x1, y1;
        uint16_t w, h;
        m_display.getTextBounds(m_cachedTitle.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 40);
        m_display.print(m_cachedTitle);
    }

    if (!m_cachedMessage.isEmpty()) {
        m_display.setTextSize(1);
        m_display.setCursor(4, 50);
        const uint8_t maxLen = m_displayWidth / 6;
        String msg = m_cachedMessage.substring(0, maxLen);
        m_display.print(msg);
    }

    m_display.setCursor(4, 60);
    m_display.setTextSize(1);
    m_display.print("Restart recommended");
}

void DisplayManager::renderOTAProgress() noexcept {
    m_display.clearDisplay();

    const uint8_t frame = m_animationFrame;

    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    m_display.getTextBounds("FIRMWARE UPDATE", 0, 0, &x1, &y1, &w, &h);
    m_display.setCursor((m_displayWidth - w) / 2, 6);
    m_display.print("FIRMWARE UPDATE");

    drawProgressBar(10, 22, 108, 12, m_cachedOTAProgress);

    const uint8_t scanX = 10 + (frame % 108);
    m_display.drawLine(scanX, 22, scanX, 34, SSD1306_WHITE);

    char pctStr[6];
    snprintf(pctStr, sizeof(pctStr), "%d%%", m_cachedOTAProgress);
    m_display.setTextSize(2);
    m_display.getTextBounds(pctStr, 0, 0, &x1, &y1, &w, &h);
    m_display.setCursor((m_displayWidth - w) / 2, 38);
    m_display.print(pctStr);

    m_display.setTextSize(1);
    m_display.setCursor(4, 56);
    m_display.print("Installing...");

    drawSpinner(m_displayWidth - 10, 58, 4, frame);
}

void DisplayManager::renderWifiStatus() noexcept {
    m_display.clearDisplay();

    const char* title = m_cachedWifiConnected ? "WiFi Connected" : "WiFi Disconnected";
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    m_display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    m_display.setCursor((m_displayWidth - w) / 2, 4);
    m_display.print(title);

    if (m_cachedWifiConnected && !m_cachedSSID.isEmpty()) {
        m_display.setTextSize(1);
        m_display.getTextBounds(m_cachedSSID.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 18);
        m_display.print(m_cachedSSID);
    }

    drawWifiIcon(54, 32, m_cachedSignal);

    if (m_cachedSignal) {
        char signalStr[16];
        snprintf(signalStr, sizeof(signalStr), "%ld dBm", m_cachedSignal);
        m_display.setTextSize(1);
        m_display.getTextBounds(signalStr, 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 56);
        m_display.print(signalStr);
    }
}

void DisplayManager::renderStorageStatus() noexcept {
    m_display.clearDisplay();

    const char* title = "Storage";
    m_display.setTextSize(2);
    m_display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    m_display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    m_display.setCursor((m_displayWidth - w) / 2, 4);
    m_display.print(title);

    if (!m_cachedStorageType.isEmpty()) {
        m_display.setTextSize(1);
        m_display.getTextBounds(m_cachedStorageType.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 24);
        m_display.print(m_cachedStorageType);
    }

    drawStorageIcon(54, 36);

    if (m_cachedTotalMB > 0) {
        const uint8_t usedPercent = (m_cachedUsedMB * 100) / m_cachedTotalMB;
        drawProgressBar(14, 48, 100, 8, usedPercent);

        char usageStr[24];
        snprintf(usageStr, sizeof(usageStr), "%lu / %lu MB", m_cachedUsedMB, m_cachedTotalMB);
        m_display.setTextSize(1);
        m_display.getTextBounds(usageStr, 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 58);
        m_display.print(usageStr);
    }
}

void DisplayManager::renderMessage() noexcept {
    m_display.clearDisplay();

    if (!m_cachedTitle.isEmpty()) {
        m_display.setTextSize(2);
        m_display.setTextColor(SSD1306_WHITE);
        int16_t x1, y1;
        uint16_t w, h;
        m_display.getTextBounds(m_cachedTitle.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 4);
        m_display.print(m_cachedTitle);
    }

    if (!m_cachedMessage.isEmpty()) {
        m_display.setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        m_display.getTextBounds(m_cachedMessage.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 28);
        m_display.print(m_cachedMessage);
    }

    if (!m_cachedFooter.isEmpty()) {
        m_display.setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        m_display.getTextBounds(m_cachedFooter.c_str(), 0, 0, &x1, &y1, &w, &h);
        m_display.setCursor((m_displayWidth - w) / 2, 52);
        m_display.print(m_cachedFooter);
    }
}

void DisplayManager::renderStartupGreeting() noexcept {
    m_display.clearDisplay();
    const uint8_t lineHeight = 8;
    const uint8_t startY = 0;
    for (uint8_t i = 0; i < m_cachedGreetingLineCount; ++i) {
        m_display.setTextSize(1);
        m_display.setTextColor(SSD1306_WHITE);
        m_display.setCursor(0, startY + i * lineHeight);
        m_display.print(m_cachedGreetingLines[i]);
    }
}

void DisplayManager::renderSleep() noexcept {
    m_display.clearDisplay();
}

void DisplayManager::drawCenteredText(const String& text, uint8_t y, uint8_t textSize) noexcept {
    m_display.setTextSize(textSize);
    m_display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    m_display.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
    m_display.setCursor((m_displayWidth - w) / 2, y);
    m_display.print(text);
}

void DisplayManager::drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t percent) noexcept {
    if (percent > 100) percent = 100;

    m_display.drawRect(x, y, width, height, SSD1306_WHITE);

    if (percent > 0) {
        const uint8_t innerWidth = (width - 2) * percent / 100;
        if (innerWidth > 0) {
            m_display.fillRect(x + 1, y + 1, innerWidth, height - 2, SSD1306_WHITE);
        }
    }
}

void DisplayManager::drawWifiIcon(uint8_t x, uint8_t y, int32_t signal) noexcept {
    uint8_t bars = 0;
    if (signal <= -90) bars = 1;
    else if (signal <= -70) bars = 2;
    else if (signal <= -50) bars = 3;
    else if (signal <= 0) bars = 4;
    else bars = 0;

    for (uint8_t i = 0; i < 4; ++i) {
        const uint8_t barHeight = 4 + i * 4;
        const uint8_t barY = y + 16 - barHeight;
        if (i < bars) {
            m_display.fillRect(x + i * 4, barY, 3, barHeight, SSD1306_WHITE);
        } else {
            m_display.drawRect(x + i * 4, barY, 3, barHeight, SSD1306_WHITE);
        }
    }

    m_display.drawLine(x + 1, y + 16, x + 14, y + 16, SSD1306_WHITE);
}

void DisplayManager::drawStorageIcon(uint8_t x, uint8_t y) noexcept {
    m_display.drawRect(x, y, 20, 14, SSD1306_WHITE);
    m_display.drawRect(x + 2, y - 4, 16, 6, SSD1306_WHITE);
    m_display.drawLine(x + 2, y, x + 18, y, SSD1306_WHITE);
    m_display.drawLine(x + 6, y + 4, x + 6, y + 10, SSD1306_WHITE);
    m_display.drawLine(x + 12, y + 4, x + 12, y + 10, SSD1306_WHITE);
}

void DisplayManager::drawMicStatus(uint8_t x, uint8_t y) noexcept {
    const uint8_t cx = x + 5;
    const uint8_t cy = y + 6;
    // Mic body: small rectangle
    m_display.drawRect(cx - 3, cy - 5, 6, 8, SSD1306_WHITE);
    // Mic stem: vertical line
    m_display.drawPixel(cx, cy + 3, SSD1306_WHITE);
    m_display.drawPixel(cx, cy + 4, SSD1306_WHITE);
    // Mic base: small arc
    m_display.drawPixel(cx - 2, cy + 4, SSD1306_WHITE);
    m_display.drawPixel(cx + 2, cy + 4, SSD1306_WHITE);

    if (m_micMuted) {
        // Mute slash: diagonal line across the mic
        m_display.drawLine(cx - 4, cy - 6, cx + 4, cy + 6, SSD1306_WHITE);
    }
}

void DisplayManager::drawStatusBar() noexcept {
    drawWifiIcon(0, 0, m_cachedSignal);
    drawMicStatus(16, 0);

    if (m_cachedTotalMB > 0) {
        drawStorageIcon(104, 0);
    }

    m_display.drawLine(0, 10, m_displayWidth, 10, SSD1306_WHITE);
}

void DisplayManager::drawRoundedBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r) noexcept {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    m_display.drawLine(x + r, y, x + w - r, y, SSD1306_WHITE);
    m_display.drawLine(x + r, y + h, x + w - r, y + h, SSD1306_WHITE);
    m_display.drawLine(x, y + r, x, y + h - r, SSD1306_WHITE);
    m_display.drawLine(x + w, y + r, x + w, y + h - r, SSD1306_WHITE);

    m_display.drawCircleHelper(x + r, y + r, r, 0x08, SSD1306_WHITE);
    m_display.drawCircleHelper(x + w - r, y + r, r, 0x02, SSD1306_WHITE);
    m_display.drawCircleHelper(x + r, y + h - r, r, 0x04, SSD1306_WHITE);
    m_display.drawCircleHelper(x + w - r, y + h - r, r, 0x01, SSD1306_WHITE);
}

void DisplayManager::drawCard(uint8_t x, uint8_t y, uint8_t w, uint8_t h) noexcept {
    drawRoundedBox(x, y, w, h, 3);
}

void DisplayManager::drawPulse(uint8_t cx, uint8_t cy, uint8_t maxRadius, uint8_t frame) noexcept {
    const uint8_t phase = frame % 16;
    const uint8_t r = 2 + (phase * maxRadius / 16);
    m_display.drawCircle(cx, cy, r, SSD1306_WHITE);
}

void DisplayManager::drawSpinner(uint8_t cx, uint8_t cy, uint8_t radius, uint8_t frame) noexcept {
    const uint8_t numDots = 6;
    for (uint8_t i = 0; i < numDots; ++i) {
        const float angle = (float)(frame * 3 + i * 60) * 3.14159f / 180.0f;
        const uint8_t dx = (uint8_t)((float)radius * cosf(angle));
        const uint8_t dy = (uint8_t)((float)radius * sinf(angle));
        const uint8_t dotSize = (i == (frame % numDots)) ? 3 : 2;
        m_display.fillCircle(cx + dx, cy + dy, dotSize, SSD1306_WHITE);
    }
}

void DisplayManager::drawWave(uint8_t cx, uint8_t cy, uint8_t amp, uint8_t frame, uint8_t count) noexcept {
    for (uint8_t i = 0; i < count; ++i) {
        const uint8_t phase = (frame + i * 4) % 32;
        const uint8_t h = 4 + (phase < 16 ? phase : 31 - phase) * amp / 16;
        const uint8_t x = cx - count * 3 + i * 6;
        m_display.drawLine(x, cy - h / 2, x, cy + h / 2, SSD1306_WHITE);
    }
}

void DisplayManager::drawRing(uint8_t cx, uint8_t cy, uint8_t radius, uint8_t thickness, uint8_t frame) noexcept {
    const float angle = (float)(frame * 6) * 3.14159f / 180.0f;
    const uint8_t segments = 12;
    for (uint8_t i = 0; i < segments; i += 2) {
        const float a = angle + (float)i * 6.2832f / (float)segments;
        const uint8_t dx = (uint8_t)((float)radius * cosf(a));
        const uint8_t dy = (uint8_t)((float)radius * sinf(a));
        m_display.fillCircle(cx + dx, cy + dy, thickness, SSD1306_WHITE);
    }
}

void DisplayManager::drawEqualizer(uint8_t x, uint8_t y, uint8_t barCount, uint8_t maxHeight, uint8_t frame) noexcept {
    const uint8_t barWidth = (m_displayWidth - 2 * x) / barCount - 2;
    for (uint8_t i = 0; i < barCount; ++i) {
        const uint8_t phase = (frame + i * 3) % 20;
        const uint8_t h = 3 + (phase < 10 ? phase : 19 - phase) * (maxHeight - 3) / 10;
        const uint8_t bx = x + i * (barWidth + 2);
        const uint8_t by = y + maxHeight - h;
        m_display.fillRoundRect(bx, by, barWidth, h, 1, SSD1306_WHITE);
    }
}

void DisplayManager::drawMicrophone(uint8_t cx, uint8_t cy) noexcept {
    m_display.fillRoundRect(cx - 4, cy - 10, 8, 12, 2, SSD1306_WHITE);
    m_display.drawLine(cx, cy + 2, cx, cy + 8, SSD1306_WHITE);
    m_display.drawLine(cx - 6, cy + 8, cx + 6, cy + 8, SSD1306_WHITE);
    m_display.drawCircle(cx, cy - 6, 2, SSD1306_WHITE);
}

void DisplayManager::drawBell(uint8_t cx, uint8_t cy, uint8_t frame) noexcept {
    const int8_t shake = (frame % 8 < 4) ? 0 : ((frame % 8 < 6) ? -1 : 1);
    const uint8_t bx = cx + shake;

    m_display.fillCircle(bx, cy - 8, 2, SSD1306_WHITE);
    m_display.drawLine(bx - 6, cy - 2, bx + 6, cy - 2, SSD1306_WHITE);
    m_display.drawLine(bx - 4, cy - 6, bx - 6, cy - 2, SSD1306_WHITE);
    m_display.drawLine(bx + 4, cy - 6, bx + 6, cy - 2, SSD1306_WHITE);
    m_display.drawLine(bx - 2, cy - 2, bx - 2, cy + 2, SSD1306_WHITE);
    m_display.drawLine(bx + 2, cy - 2, bx + 2, cy + 2, SSD1306_WHITE);
}

void DisplayManager::drawThinkingCore(uint8_t cx, uint8_t cy, uint8_t frame) noexcept {
    const uint8_t coreRadius = 5 + (frame % 4);
    m_display.fillCircle(cx, cy, coreRadius, SSD1306_WHITE);
    m_display.fillCircle(cx, cy, coreRadius - 2, SSD1306_BLACK);

    const float angle = (float)(frame * 8) * 3.14159f / 180.0f;
    for (uint8_t i = 0; i < 3; ++i) {
        const float a = angle + (float)i * 2.0944f;
        const uint8_t px = cx + (uint8_t)((float)coreRadius * cosf(a));
        const uint8_t py = cy + (uint8_t)((float)coreRadius * sinf(a));
        m_display.fillCircle(px, py, 1, SSD1306_WHITE);
    }
}

void DisplayManager::updateAnimation() noexcept {
    const unsigned long now = millis();
    if (now - m_lastUpdateTime >= m_animationIntervalMs) {
        m_animationFrame++;
        m_lastUpdateTime = now;

        switch (m_currentState) {
            case DisplayState::BOOT:
            case DisplayState::LISTENING:
            case DisplayState::THINKING:
            case DisplayState::SPEAKING:
            case DisplayState::REMINDER:
            case DisplayState::NOTIFICATION:
            case DisplayState::ERROR:
            case DisplayState::OTA:
                m_screenDirty = true;
                break;
            default:
                break;
        }
    }
}

void DisplayManager::updateScreenTimeout() noexcept {
    if (m_sleeping) return;

    const unsigned long now = millis();
    if (now - m_lastActivityTime >= m_screenTimeoutMs) {
        sleep();
    }
}