#ifndef AURA_DISPLAY_MANAGER_H
#define AURA_DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <cstdint>
#include "config.h"
#include "logger.h"
#include "aura_face.h"

/**
 * @enum DisplayState
 * @brief Enumeration of display states
 */
enum class DisplayState : uint8_t {
    BOOT,          ///< Boot/initialization screen
    HOME,          ///< Home/idle screen
    LISTENING,     ///< Microphone listening state
    THINKING,      ///< Processing/thinking state
    SPEAKING,      ///< Speaker output state
    REMINDER,      ///< Reminder notification
    NOTIFICATION,  ///< Generic notification
    ERROR,         ///< Error display
    OTA,           ///< Over-the-air update progress
    SLEEP,         ///< Sleep/low-power state
    STARTUP_GREETING,  ///< Startup greeting display
    FACE           ///< JARVIS-style animated presence (idle / active expressions)
};

/**
 * @enum MicStatus
 * @brief Live microphone/voice state shown by the top-right status icon.
 *
 * The OLED is 1-bit monochrome, so a "colour" cannot be rendered literally.
 * Each state therefore maps to a distinct, crisp glyph (dash/slash, solid,
 * animated pulse, orbiting dot, sound waves, X) that reads independently as
 * the voice pipeline advances.
 */
enum class MicStatus : uint8_t {
    MUTED,       ///< Mic muted / privacy (grey mic + slash)
    IDLE,        ///< Ready, listening for input (solid mic)
    LISTENING,   ///< Voice detected / capturing intent (solid + in-arrow)
    RECORDING,   ///< Recording capture (breathing pulse, ~200ms)
    PROCESSING,  ///< AI/STT processing (orbiting dot)
    SPEAKING,    ///< Speaking (spinning sound waves)
    ERROR        ///< Mic/voice error (X)
};

/**
 * @class DisplayManager
 * @brief Single authority for all OLED display operations
 *
 * Manages:
 * - OLED initialization and control
 * - State-based screen rendering
 * - Animations and transitions
 * - Status displays (Wi-Fi, storage, system)
 * - Notifications and error messages
 * - Brightness and contrast control
 * - Sleep mode management
 * - Display refresh and buffering
 *
 * Non-blocking, production-quality display management for ESP32.
 */
class DisplayManager {
public:
    /**
     * @brief Constructor
     */
    DisplayManager() noexcept;

    /**
     * @brief Destructor
     */
    ~DisplayManager() noexcept;

    // Delete copy semantics
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    // Delete move semantics
    DisplayManager(DisplayManager&&) = delete;
    DisplayManager& operator=(DisplayManager&&) = delete;

    /**
     * @brief Initialize the display manager
     * @return true if initialization successful, false otherwise
     * @note Should be called once during setup()
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update display state and rendering
     * @note Should be called regularly from loop()
     * @note Non-blocking; respects refresh interval
     */
    void update() noexcept;

    /**
     * @brief Scheduler-compatible alias for update()
     */
    void run() noexcept;

    /**
     * @brief Clear the display buffer
     */
    void clear() noexcept;
    /**
 * @brief Reset the display manager to its default state.
 */
void reset() noexcept;

    /**
     * @brief Refresh/update the display with buffer contents
     */
    void refresh() noexcept;

    /**
     * @brief Force immediate redraw of current state
     * @note Use only when display must update immediately
     */
    void forceRefresh() noexcept;

    // ========================================================================
    // State Management
    // ========================================================================

    /**
     * @brief Change display state
     * @param newState Target display state
     * @note Preferred method for screen transitions
     */
    void setState(DisplayState newState) noexcept;

    /**
     * @brief Get current display state
     * @return Current DisplayState value
     */
    [[nodiscard]] DisplayState getState() const noexcept;

    /**
     * @brief Get display initialization status
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    // ========================================================================
    // Screen Display Methods
    // ========================================================================

    /**
     * @brief Show AURA splash screen
     */
    void showSplash() noexcept;

    /**
     * @brief Show boot/initialization screen with progress
     * @param progress Progress percentage (0-100)
     */
    void showBoot(uint8_t progress) noexcept;

    /**
     * @brief Show home/idle screen
     */
    void showHome() noexcept;

    /**
     * @brief Show the animated AURA presence face (default idle).
     */
    void showFace() noexcept;

    /**
     * @brief Show the AURA presence face with an explicit expression.
     * @note Used by AuraSystem so the OLED face and RGB aura always stay in sync.
     */
    void showFaceExpression(FaceExpression expr) noexcept;

    /**
     * @brief Show the smart dashboard on demand.
     * @note Returns to the face automatically after kDashboardReturnMs.
     */
    void showDashboard() noexcept;

    /**
     * @brief True while the on-demand dashboard is being shown.
     */
    [[nodiscard]] bool isDashboardActive() const noexcept;

    /**
     * @brief Trigger a face micro-animation from a system event.
     */
    void faceNotify(FaceEvent event) noexcept;

    /**
     * @brief Update home screen dynamic data for ambient intelligence
     * @param personalityName Active personality display name
     * @param lastActivity Current activity description
     * @param conversationsToday Number of conversations today
     * @param memoryCount Number of stored memories
     * @param reminderCount Number of active reminders
     */
    void updateHomeData(const String& personalityName, const String& lastActivity,
                        size_t conversationsToday, size_t memoryCount, size_t reminderCount) noexcept;

    /**
     * @brief Set microphone muted state for status bar icon
     * @param muted true if mic is muted (push-to-talk idle), false if active (continuous listening)
     */
    void setMicMuted(bool muted) noexcept;

    /**
     * @brief Set the live vocie state shown by the top-right mic icon.
     * @note Updates the icon region immediately (no full-screen redraw).
     */
    void setMicStatus(MicStatus status) noexcept;

    /**
     * @brief Get the mic/voice state currently shown by the top-right icon.
     */
    [[nodiscard]] MicStatus getMicStatus() const noexcept;

    /**
     * @brief Show microphone listening state
     */
    void showListening() noexcept;

    /**
     * @brief Show processing/thinking state
     */
    void showThinking() noexcept;

    /**
     * @brief Show speaker output/speaking state
     */
    void showSpeaking() noexcept;

    /**
     * @brief Show reminder notification
     * @param title Reminder title text
     * @param message Reminder message text
     */
    void showReminder(const String& title, const String& message) noexcept;

    /**
     * @brief Show generic notification popup
     * @param title Notification title
     * @param message Notification message
     * @param durationMs Display duration in milliseconds (0 = indefinite)
     */
    void showNotification(const String& title, const String& message, unsigned long durationMs = 0) noexcept;

    /**
     * @brief Show error message
     * @param title Error title
     * @param message Error description
     */
    void showError(const String& title, const String& message) noexcept;

    /**
     * @brief Show OTA (Over-The-Air) update progress
     * @param progress Update progress percentage (0-100)
     */
    void showOTAProgress(uint8_t progress) noexcept;

    /**
     * @brief Show Wi-Fi status screen
     * @param connected true if connected, false if disconnected
     * @param ssid Network SSID name
     * @param signal Signal strength (RSSI in dBm, or 0)
     */
    void showWifiStatus(bool connected, const String& ssid, int32_t signal) noexcept;

    /**
     * @brief Show storage status screen
     * @param storageType Storage media name (e.g., "SPIFFS", "SD Card")
     * @param usedMB Used storage in MB
     * @param totalMB Total storage in MB
     */
    void showStorageStatus(const String& storageType, uint32_t usedMB, uint32_t totalMB) noexcept;

    /**
     * @brief Set a custom state label for personality-aware display
     * @param state The display state (LISTENING, THINKING, SPEAKING)
     * @param label Custom label text (empty = use default)
     */
    void setStateLabel(DisplayState state, const String& label) noexcept;

    /**
     * @brief Show custom message screen
     * @param title Message title
     * @param body Message body text
     * @param footer Optional footer text
     */
    void showMessage(const String& title, const String& body, const String& footer = "") noexcept;

    /**
     * @brief Pin the current status screen so the idle auto-return cannot
     *        overwrite it until unpinned (e.g. "Microphone Muted", "Setup Mode").
     */
    void pinStatus() noexcept;

    /**
     * @brief Allow the idle auto-return again (call when the pinned status ends).
     */
    void unpinStatus() noexcept;

    /**
     * @brief Show startup greeting with custom lines
     * @param lines Array of greeting text lines
     * @param count Number of lines (max STARTUP_GREETING_LINES_MAX)
     * @note Displays multi-line system information on boot
     */
    void showStartupGreeting(const String lines[], uint8_t count) noexcept;

    // ========================================================================
    // Display Control Methods
    // ========================================================================

    /**
     * @brief Set display brightness
     * @param brightness Brightness level (0-255)
     */
    void setBrightness(uint8_t brightness) noexcept;

    /**
     * @brief Enter sleep mode (blank display, reduced power)
     */
    void sleep() noexcept;

    /**
     * @brief Wake from sleep mode (restore previous display)
     */
    void wake() noexcept;

    /**
     * @brief Enable/disable night mode (reduced brightness, warm tone)
     * @param enabled Night mode state
     */
    void setNightMode(bool enabled) noexcept;

    /**
     * @brief Check if night mode is active
     * @return true if night mode enabled
     */
    [[nodiscard]] bool isNightMode() const noexcept;

    /**
     * @brief Set auto brightness based on ambient conditions
     * @param ambientLight Ambient light level (0-1023, from sensor)
     */
    void setAutoBrightness(uint16_t ambientLight) noexcept;

/**
     * @brief Turn OLED panel on.
     */
    void displayOn() noexcept;

    /**
     * @brief Turn OLED panel off.
     */
    void displayOff() noexcept;

    /**
     * @brief Check if display is in sleep mode
     * @return true if sleeping, false otherwise
     */
    [[nodiscard]] bool isSleeping() const noexcept;

    /**
     * @brief Check if display is awake
     * @return true if awake, false if sleeping
     */
    [[nodiscard]] bool isAwake() const noexcept;

    /**
     * @brief Set display rotation
     * @param rotation Rotation angle (0, 1, 2, 3)
     */
    void setRotation(uint8_t rotation) noexcept;

    /**
     * @brief Set display contrast
     * @param contrast Contrast level (0-255)
     */
    void setContrast(uint8_t contrast) noexcept;

    /**
     * @brief Set display color inversion
     * @param inverted true to invert colors, false for normal
     */
    void setInverted(bool inverted) noexcept;

    // ========================================================================
    // Display Information Methods
    // ========================================================================

    /**
     * @brief Get display width in pixels
     * @return Width in pixels (typically 128)
     */
    [[nodiscard]] uint16_t getWidth() const noexcept;

    /**
     * @brief Get display height in pixels
     * @return Height in pixels (typically 64)
     */
    [[nodiscard]] uint16_t getHeight() const noexcept;

private:
    // Private state management
    void changeState(DisplayState newState) noexcept;
    bool startTransition(DisplayState from, DisplayState to) noexcept;
    void renderTransition() noexcept;

    // Private display rendering methods
    void renderBoot(uint8_t progress) noexcept;
    void renderHome() noexcept;
    void renderListening() noexcept;
    void renderThinking() noexcept;
    void renderSpeaking() noexcept;
    void renderReminder() noexcept;
    void renderNotification() noexcept;
    void renderError() noexcept;
    void renderOTAProgress() noexcept;
    void renderWifiStatus() noexcept;
    void renderStorageStatus() noexcept;
    void renderMessage() noexcept;
    void renderSleep() noexcept;
    void renderStartupGreeting() noexcept;
    void renderFace() noexcept;
    void renderIdleClock() noexcept;

    // Private helper methods
    void drawCenteredText(const String& text, uint8_t y, uint8_t textSize = 1) noexcept;
    void drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t percent) noexcept;
    void drawWifiIcon(uint8_t x, uint8_t y, int32_t signal) noexcept;
    void drawStorageIcon(uint8_t x, uint8_t y) noexcept;
    void drawMicStatus(uint8_t x, uint8_t y) noexcept;
    void renderMicIcon() noexcept;
    void drawMicGlyph(uint8_t x0, uint8_t y0) noexcept;
    [[nodiscard]] bool isMicIconAnimated() const noexcept;
    void drawStatusBar() noexcept;
    void updateAnimation() noexcept;
    void updateScreenTimeout() noexcept;

    // New drawing helpers
    void drawRoundedBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r) noexcept;
    void drawCard(uint8_t x, uint8_t y, uint8_t w, uint8_t h) noexcept;
    void drawPulse(uint8_t cx, uint8_t cy, uint8_t maxRadius, uint8_t frame) noexcept;
    void drawSpinner(uint8_t cx, uint8_t cy, uint8_t radius, uint8_t frame) noexcept;
    void drawWave(uint8_t cx, uint8_t cy, uint8_t amp, uint8_t frame, uint8_t count) noexcept;
    void drawRing(uint8_t cx, uint8_t cy, uint8_t radius, uint8_t thickness, uint8_t frame) noexcept;
    void drawEqualizer(uint8_t x, uint8_t y, uint8_t barCount, uint8_t maxHeight, uint8_t frame) noexcept;
    void drawMicrophone(uint8_t cx, uint8_t cy) noexcept;
    void drawBell(uint8_t cx, uint8_t cy, uint8_t frame) noexcept;
    void drawThinkingCore(uint8_t cx, uint8_t cy, uint8_t frame) noexcept;

    // Private member variables
    Adafruit_SH1106G m_display{OLED_WIDTH, OLED_HEIGHT};   ///< SH1106 OLED display object
    DisplayState m_currentState;             ///< Current display state
    DisplayState m_previousState;            ///< Previous display state
    DisplayState m_lastRenderedState;
    bool m_transitionActive;                 ///< Cross-fade transition in progress
    unsigned long m_transitionStartTime;     ///< Transition start timestamp
    DisplayState m_transitionFrom;           ///< State transitioning from
    DisplayState m_transitionTo;             ///< State transitioning to
    bool m_initialized;                      ///< Initialization state
    bool m_sleeping;                         ///< Sleep mode state
    bool m_screenDirty;                      ///< Screen needs redraw flag
    unsigned long m_lastUpdateTime;          ///< Last update timestamp
    unsigned long m_lastRefreshTime;         ///< Last refresh timestamp
    unsigned long m_stateStartTime;          ///< Time when state changed
    unsigned long m_notificationTimeout;     ///< Notification display timeout
    unsigned long m_lastActivityTime;        ///< Last user/system activity time
    uint8_t m_brightness;                    ///< Current brightness level
    uint8_t m_contrast;                      ///< Current contrast level
    bool m_inverted;                         ///< Display inversion state
    bool m_nightMode;                        ///< Night mode state
    uint8_t m_rotation;                      ///< Display rotation (0-3)
    uint32_t m_animationFrame;               ///< Animation frame counter

    // Personality-aware state labels (empty = use default hardcoded text)
    String m_stateLabels[12];

    // Home screen enrichment data
    String m_homePersonality;
    String m_homeActivity;
    size_t m_homeConversations;
    size_t m_homeMemories;
    size_t m_homeReminders;

    // Mic state for status bar
    bool m_micMuted;

    // Top-right microphone status icon
    MicStatus m_micStatus;          ///< Current mic/voice state
    MicStatus m_lastMicStatus;      ///< Last rendered state (region-only redraws)
    unsigned long m_lastMicIconTime; ///< Last animated icon tick
    uint8_t m_micIconPhase;         ///< Icon animation phase

    // ========================================================================
    // Smart Dashboard Widget System
    // ========================================================================

public:
    enum class WidgetType : uint8_t {
        CLOCK = 0,
        GREETING,
        NEXT_REMINDER,
        PROJECT_STATUS,
        TODAYS_STUDY,
        DAILY_PROGRESS,
        NOTIFICATIONS,
        WIFI_STATUS,
        HEAP_USAGE,
        STORAGE_USAGE,
        ACTIVE_CONTEXT,
        COUNT
    };

    struct WidgetData {
        String clockStr;
        String greetingStr;
        String nextReminder;
        String projectStatus;
        String studySummary;
        String progressSummary;
        uint8_t notificationCount;
        bool wifiConnected;
        int32_t wifiRSSI;
        uint32_t freeHeap;
        uint32_t totalHeap;
        uint32_t usedStorageKB;
        uint32_t totalStorageKB;
        String contextName;
        unsigned long lastUpdated[static_cast<size_t>(WidgetType::COUNT)];

        WidgetData() noexcept
            : notificationCount(0), wifiConnected(false), wifiRSSI(0),
              freeHeap(0), totalHeap(0), usedStorageKB(0), totalStorageKB(0) {
            for (auto& t : lastUpdated) t = 0;
        }
    };

    /**
     * @brief Enable or disable a dashboard widget
     * @param widget Widget type
     * @param enabled New state
     */
    void setWidgetEnabled(WidgetType widget, bool enabled) noexcept;

    /**
     * @brief Check if a widget is enabled
     * @param widget Widget type
     * @return true if enabled
     */
    [[nodiscard]] bool isWidgetEnabled(WidgetType widget) const noexcept;

    /**
     * @brief Update dashboard data for a widget type
     * @param widget Widget type to update
     * @param data New data string
     */
    void updateWidgetData(WidgetType widget, const String& data) noexcept;

    /**
     * @brief Set dashboard numeric data
     */
    void setDashboardNumericData(uint32_t freeHeap, uint32_t totalHeap,
                                  uint32_t usedKB, uint32_t totalKB,
                                  int32_t rssi, bool wifiConnected) noexcept;

    /**
     * @brief Force full dashboard redraw
     */
    void refreshDashboard() noexcept;

private:
    void renderDashboard() noexcept;
    void renderWidget(WidgetType widget, uint8_t& y) noexcept;
    void renderClockWidget(uint8_t& y) noexcept;
    void renderGreetingWidget(uint8_t& y) noexcept;
    void renderNextReminderWidget(uint8_t& y) noexcept;
    void renderProjectStatusWidget(uint8_t& y) noexcept;
    void renderStudyWidget(uint8_t& y) noexcept;
    void renderProgressWidget(uint8_t& y) noexcept;
    void renderNotificationsWidget(uint8_t& y) noexcept;
    void renderWifiWidget(uint8_t& y) noexcept;
    void renderHeapWidget(uint8_t& y) noexcept;
    void renderStorageWidget(uint8_t& y) noexcept;
    void renderContextWidget(uint8_t& y) noexcept;

    // Widget state
    WidgetData m_widgetData;
    bool m_widgetEnabled[static_cast<size_t>(WidgetType::COUNT)];
    bool m_dashboardDirty;
    unsigned long m_lastDashboardRefresh;

    // Cached display data
    String m_cachedTitle;                    ///< Cached title text
    String m_cachedMessage;                  ///< Cached message text
    String m_cachedFooter;                   ///< Cached footer text
    String m_cachedGreetingLines[STARTUP_GREETING_LINES_MAX];  ///< Cached greeting lines
    uint8_t m_cachedGreetingLineCount;       ///< Number of cached greeting lines
    String m_cachedSSID;                     ///< Cached Wi-Fi SSID
    String m_cachedStorageType;              ///< Cached storage type name
    bool m_cachedWifiConnected;              ///< Cached Wi-Fi connection state
    int32_t m_cachedSignal;                  ///< Cached signal strength
    uint32_t m_cachedUsedMB;                 ///< Cached used storage in MB
    uint32_t m_cachedTotalMB;                ///< Cached total storage in MB
    uint8_t m_cachedOTAProgress;             ///< Cached OTA progress percentage
    uint8_t m_cachedBootProgress;            ///< Cached boot progress percentage

    // AURA presence face + on-demand dashboard timer
    AuraFace m_face;                         ///< JARVIS-style face renderer
    unsigned long m_dashboardUntil;        ///< Timestamp when dashboard returns to face
    bool m_messageActive = false;        ///< HOME shows a message screen instead of dashboard
    bool m_statusPinned = false;         ///< Hold status screen against auto-return

    // Idle clock (displayed only while truly idle on the presence face)
    uint8_t m_clockMinute;   ///< Last rendered IST minute (0..59) for change detection
    char m_clockText[6];     ///< Cached "HH:MM" (NUL-terminated) clock text

    // Display configuration constants
    static constexpr uint16_t m_displayWidth{OLED_WIDTH};           ///< Display width in pixels
    static constexpr uint16_t m_displayHeight{OLED_HEIGHT};         ///< Display height in pixels
    static constexpr unsigned long m_screenTimeoutMs{30000UL};      ///< Screen sleep timeout (30 seconds)
    static constexpr unsigned long m_refreshIntervalMs{33UL};       ///< Refresh interval (≈30 FPS)
    static constexpr unsigned long m_animationIntervalMs{100UL};
    static constexpr unsigned long m_dashboardReturnMs{8000UL};     ///< Dashboard auto-return to face
    static constexpr unsigned long m_infoReturnMs{6000UL};          ///< Notification/reminder auto-return
    static constexpr uint8_t m_defaultBrightness{200};              ///< Default brightness level
    static constexpr uint8_t m_defaultContrast{255};

    // Top-right microphone icon geometry
    static constexpr uint8_t mMicIconX{116};   ///< Left edge of the icon region
    static constexpr uint8_t mMicIconY{0};     ///< Top edge of the icon region
    static constexpr uint8_t mMicIconW{12};    ///< Icon region width
    static constexpr uint8_t mMicIconH{12};    ///< Icon region height
    static constexpr unsigned long mMicIconIntervalMs{100UL}; ///< Animation cadence (~200ms pulse)
};

/**
 * @brief Global display manager instance
 */
extern DisplayManager displayManager;

#endif // AURA_DISPLAY_MANAGER_H