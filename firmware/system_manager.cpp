#include "system_manager.h"
#include "config.h"
#include "logger.h"
#include "storage_manager.h"
#include "wifi_manager.h"
#include "display_manager.h"
#include "audio_manager.h"
#include "sarvam_stt.h"
#include "gemini_client.h"
#include "sarvam_tts.h"
#include "speech_provider.h"
#include "tts_provider.h"
#include "conversation_manager.h"
#include "reminder_manager.h"
#include "ota_manager.h"
#include "web_portal.h"
#include "led_ring.h"
#include "aura_system.h"
#include "uptime_monitor.h"
#include "settings_manager.h"
#include "sound_manager.h"
#include "memory_manager.h"
#include "plugin_manager.h"
#include "skill_manager.h"
#include "personality_manager.h"
#include "context_manager.h"
#include "performance_manager.h"
#include "crash_manager.h"
#include "diagnostics_manager.h"
#include "error_manager.h"
#include "knowledge_graph_manager.h"
#include "goal_manager.h"
#include "habit_manager.h"
#include "planner_manager.h"
#include "function_router.h"
#include "reflection_manager.h"
#include "automation_manager.h"
#include "startup_greeting_manager.h"
#include "tiny_ai_manager.h"
#include "timeline_manager.h"
#include "briefing_manager.h"
#include "semantic_search_manager.h"
#include "decision_manager.h"
#include "learning_manager.h"

#include "prediction_manager.h"
#include "document_manager.h"
#include "workspace_manager.h"
#include "vault_manager.h"
#include "event_bus.h"
#include "study_manager.h"
#include "companion_manager.h"
#include "esp_now_manager.h"
#include "health_monitor.h"
#include "smart_search.h"
#include "analytics_manager.h"
#include "device_mesh.h"
#include "executive_assistant.h"
#include "ui_framework.h"
#include "service_manager.h"
#include "health_manager.h"
#include "capability_manager.h"
#include "platform_abstraction.h"
#include "task_scheduler.h"
#include "ai_pipeline.h"
#include "command_palette.h"
#include "scene_engine.h"
#include "workflow_engine.h"
#include "diagnostic_system.h"
#include "log_manager.h"
#include "security_manager.h"
#include "resilience_manager.h"
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

/// Global SystemManager instance
SystemManager systemManager;

// ============================================================================
// Anonymous Namespace - Internal Helpers
// ============================================================================

namespace {

constexpr const char* kModuleNames[] = {
    "StorageManager",
    "WiFiManager",
    "DisplayManager",
    "AudioManager",
    "SpeechToText",
    "GeminiClient",
    "TextToSpeech",
    "ConversationManager",
    "ReminderManager",
    "OtaManager",
    "WebPortal",
    "SoundManager",
    "MemoryManager",
    "LedRing",
    "SettingsManager",
    "PluginManager",
    "SkillManager",
    "PersonalityManager",
    "ContextManager",
    "(merged into BriefingManager)",
    "PerformanceManager",
    "CrashManager",
    "DiagnosticsManager",
    "KnowledgeGraphManager",
    "GoalManager",
    "HabitManager",
    "PlannerManager",
    "FunctionRouter",
    "ReflectionManager",
    "AutomationManager",
    "TinyAIManager",
    "TimelineManager",
    "BriefingManager",
    "SemanticSearchManager",
    "DecisionManager",
    "LearningManager",
    "(merged into ExecutiveAssistant)",
    "PredictionManager",
    "DocumentManager",
    "WorkspaceManager",
    "VaultManager",
    "EventBus",           // 41
    "StudyManager",       // 42
    "CompanionManager",   // 43
    "EspNowManager",      // 44
    "HealthMonitor",      // 45
    "SmartSearch",        // 46
    "AnalyticsManager",   // 47
    "DeviceMesh",         // 48
    "ExecutiveAssistant"  // 49
};

constexpr size_t kModuleCount = sizeof(kModuleNames) / sizeof(kModuleNames[0]);

/**
 * @brief Safe string assignment with reserve
 */
void safeAssign(String& dest, const String& src) noexcept {
    dest = src;
}

/**
 * @brief Human-readable label for a persisted Wi-Fi state.
 */
const char* wifiStateToString(WifiState state) noexcept {
    switch (state) {
        case WifiState::CONNECTING:   return "CONNECTING";
        case WifiState::CONNECTED:    return "CONNECTED";
        case WifiState::ACCESS_POINT: return "ACCESS_POINT";
        case WifiState::ERROR:        return "ERROR";
        case WifiState::DISCONNECTED:
        default:                      return "DISCONNECTED";
    }
}

/**
 * @brief Log and clear the restart reason persisted by requestRestart().
 *
 * Called early in boot so the operator can see why the device restarted.
 * The key is removed after reading so it is reported only once.
 */
void logAndClearLastRestartReason() noexcept {
    Preferences prefs;
    if (!prefs.begin(CRASH_COUNTER_NVS_NAMESPACE, true)) return;
    String reason = prefs.getString(CRASH_RESTART_REASON_KEY, "");
    prefs.end();
    if (reason.isEmpty()) return;
    Logger::warning("SystemManager", "[AURA][LAST RESTART] reason=%s", reason.c_str());
    Preferences writer;
    if (writer.begin(CRASH_COUNTER_NVS_NAMESPACE, false)) {
        writer.remove(CRASH_RESTART_REASON_KEY);
        writer.end();
    }
}

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

SystemManager::SystemManager() noexcept
    : m_initialized(false),
      m_currentState(SystemState::BOOTING),
      m_lastError(SystemError::NONE),
      m_info(),
      m_bootTime(0),
      m_lastHealthCheck(0),
      m_moduleInitStartTime(0),
      m_initModuleIndex(0),
      m_safeMode(false),
      m_headless(false),
      m_headlessMode(HeadlessMode::HM_NORMAL) {
}

SystemManager::~SystemManager() noexcept {
    if (m_initialized) {
        shutdown();
    }
}

// ============================================================================
// Public API - Lifecycle
// ============================================================================

bool SystemManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    Logger::info(kLogCategory, "=== %s ===", aura::identity::kProjectName);
    Logger::info(kLogCategory, "Firmware: AURA OS MARK %s v%s (%s)",
        AURA_MARK_ROMAN, AURA_SEMVER, aura::version::kCodename);
    Logger::info(kLogCategory, "Channel: %s | Built: %s %s",
        aura::version::kChannel, aura::version::kBuildDate, aura::version::kBuildTime);
    Logger::info(kLogCategory, "Booting...");

    // Check boot loop counter before any init
    // We must read NVS directly before crashManager is initialized
    int bootCount = 0;
    {
        Preferences prefs;
        prefs.begin(CRASH_COUNTER_NVS_NAMESPACE, true);
        bootCount = prefs.getInt(CRASH_COUNTER_KEY, 0);
        prefs.end();
    }
    if (bootCount >= BOOT_LOOP_THRESHOLD) {
        m_safeMode = true;
        Logger::warning(kLogCategory, "=== BOOT LOOP DETECTED (%d crashes) ===", bootCount);
        Logger::warning(kLogCategory,
            "[AURA][SAFE MODE] Boot-loop threshold reached - startup limited to "
            "essential subsystems");
        Logger::warning(kLogCategory, "=== SAFE MODE ACTIVATED ===");
    }

    m_bootTime = millis();
    changeState(SystemState::INITIALIZING);

    // Initialize static system info
    safeAssign(m_info.firmwareVersion, AURA_VERSION);
    safeAssign(m_info.deviceName, AURA_NAME);
    m_info.uptime = 0;
    m_info.wifiConnected = false;
    m_info.otaRunning = false;
    m_info.conversationRunning = false;
    m_info.reminderRunning = false;

    // Initialize all modules
    if (!initializeModules()) {
        Logger::error(kLogCategory, "Module initialization failed");
        rollbackInitialization();
        changeState(SystemState::ERROR);
        setError(SystemError::INIT_FAILED);
        displayManager.showError("SYSTEM ERROR", "See Serial Monitor");
        auraSystem.critical();
        logAndClearLastRestartReason();
        return false;
    }

    m_initialized = true;

    // Clear boot loop counter only if the last restart was NOT a recovery reboot.
    // Health-driven restarts should accumulate the counter across boots so that
    // a stuck subsystem eventually triggers safe mode.
    if (!crashManager.wasRecoveryRestart()) {
        crashManager.clearBootLoopCounter();
    }

    logAndClearLastRestartReason();

    validateCredentials();

    if (!m_safeMode) {
        startupGreetingManager.start();
    }

    // Bring the web portal up only now that all modules have initialized.
    if (webPortal.initialize()) {
        webPortal.start();
        serviceStatusManager.setStatus(ServiceId::SVC_WEB_PORTAL, ServiceStatus::SS_ONLINE);
        serviceStatusManager.setStatus(ServiceId::SVC_REST, ServiceStatus::SS_ONLINE);
        serviceStatusManager.setStatus(ServiceId::SVC_WEBSOCKET, ServiceStatus::SS_ONLINE);
    }

    changeState(SystemState::READY);

    const unsigned long initTimeMs = millis() - m_bootTime;
    Logger::info(kLogCategory, "System initialization complete (%lu ms)", initTimeMs);
    Logger::info(kLogCategory, "Free heap: %u bytes", ESP.getFreeHeap());

    return true;
}

void SystemManager::run() noexcept {
    update();
}

void SystemManager::update() noexcept {
    if (!m_initialized) return;

    unsigned long now = millis();
    m_info.uptime = (now - m_bootTime) / 1000;

    // Update all modules
    updateModules();

    // Health monitoring
    if (now - m_lastHealthCheck >= kHealthCheckIntervalMs) {
        m_lastHealthCheck = now;
        checkHealth();
        refreshDynamicServiceStatus();
    }

    // State-specific logic
    switch (m_currentState) {
        case SystemState::LOW_POWER:
            // Keep modules in low power
            break;

        case SystemState::UPDATING:
            if (!otaManager.isBusy()) {
                changeState(SystemState::READY);
            }
            break;

        case SystemState::ERROR:
            // Error state - wait for recovery or restart
            break;

        default:
            break;
    }
}

void SystemManager::shutdown() noexcept {
    Logger::info(kLogCategory, "Shutting down...");

    changeState(SystemState::SHUTDOWN);

    // Save all dirty state before stopping modules
    settingsManager.saveAll();
    crashManager.save();
    memoryManager.save();
    reminderManager.save();
    if (briefingManager.isInitialized()) briefingManager.saveSummaries();
    pluginManager.saveState();
    skillManager.save();
    personalityManager.save();
    goalManager.save();
    habitManager.save();
    plannerManager.save();
    reflectionManager.save();
    automationManager.save();
    knowledgeGraphManager.save();
    timelineManager.save();
    briefingManager.saveBriefings();
    decisionManager.save();
    learningManager.save();
    executiveAssistant.saveRecommendations();
    predictionManager.save();
    documentManager.save();
    workspaceManager.save();
    vaultManager.save();
    studyManager.save();
    companionManager.save();
    espNowManager.shutdown();

    // Stop all modules gracefully
    conversationManager.stopConversation();
    textToSpeech.stop();
    audioManager.stopPlayback();
    audioManager.stopRecording();
    ledRing.turnOff();
    serviceManager.Shutdown();
    uiFramework.shutdown();
    displayManager.sleep();
    webPortal.stop();
    wifiManager.disconnect();
    storageManager.unmountSPIFFS();
    storageManager.unmountSD();
    uptimeMonitor.persist();

    Logger::info(kLogCategory, "Shutdown complete");
}

void SystemManager::restart() noexcept {
    Logger::warning(kLogCategory, "Restarting device...");

    // Log crash before restart if system was in an error state
    if (m_currentState == SystemState::ERROR) {
        crashManager.logCrashBeforeRestart("System restart from error state");
    }

    displayManager.showMessage("Restarting...", "");
    requestRestart("user_restart", true);
}

void SystemManager::requestRestart(const char* reason, bool flushState) noexcept {
    const char* r = (reason && reason[0] != '\0') ? reason : "unknown";
    Logger::warning(kLogCategory, "[AURA][RESTART] reason=%s", r);

    // Persist the restart reason so the next boot can report why we restarted.
    {
        Preferences prefs;
        if (prefs.begin(CRASH_COUNTER_NVS_NAMESPACE, false)) {
            prefs.putString(CRASH_RESTART_REASON_KEY, r);
            prefs.end();
        }
    }

    // Flush bounded persistent state before resetting so a hard power cut
    // during/after the restart does not lose recent user data.
    if (flushState) {
        settingsManager.saveAll();
        uptimeMonitor.persist();
    }

    delay(200);
    ESP.restart();
}

void SystemManager::factoryReset() noexcept {
    Logger::warning(kLogCategory, "Factory reset initiated");

    // Clear all settings
    storageManager.formatSPIFFS();
    wifiManager.clearCredentials();
    settingsManager.factoryReset();

    // Reset all modules
    conversationManager.clearHistory();
    reminderManager.clearReminders();
    otaManager.cancelUpdate();
    memoryManager.clear();
    crashManager.clearCrashes();
    personalityManager.activateProfile("jarvis");

    Logger::info(kLogCategory, "Factory reset complete");
    restart();
}

void SystemManager::enterLowPower() noexcept {
    if (m_currentState == SystemState::LOW_POWER) return;

    Logger::info(kLogCategory, "Entering low power mode");

    conversationManager.stopConversation();
    textToSpeech.stop();
    audioManager.stopPlayback();
    audioManager.stopRecording();
    displayManager.sleep();
    ledRing.turnOff();

    // Disable WiFi to save power
    wifiManager.disconnect();

    changeState(SystemState::LOW_POWER);
}

void SystemManager::exitLowPower() noexcept {
    if (m_currentState != SystemState::LOW_POWER) return;

    Logger::info(kLogCategory, "Exiting low power mode");

    // Reconnect WiFi
    wifiManager.reconnect();

    // Wake display
    displayManager.wake();
    displayManager.showHome();
    auraSystem.wake();

    changeState(SystemState::READY);
}

bool SystemManager::checkHealth() noexcept {
    bool healthy = true;

    // Drive the centralized self-recovery state machine (bounded recovery +
    // last-resort reboot gate). Must run before per-subsystem reporting so
    // failures detected this tick are attributed to the current cycle.
    healthManager.update();

    // Monitor memory
    monitorMemory();
    if (m_info.freeHeap < kMinimumFreeHeap) {
        Logger::warning(kLogCategory, "Low memory: %u bytes (min: %u)",
            m_info.freeHeap, kMinimumFreeHeap);
        healthy = false;
    }

    // Monitor WiFi
    monitorWiFi();
    if (!wifiManager.isConnected()) {
        Logger::warning(kLogCategory, "WiFi disconnected");
        healthy = false;
    }

    // Monitor OTA
    monitorOTA();

    // Monitor subsystem health for centralized recovery
    monitorSubsystems();

    // Monitor reminders
    monitorReminders();

    // Monitor conversation
    monitorConversation();

    // Monitor tasks
    monitorTasks();

    m_info.wifiConnected = wifiManager.isConnected();
    m_info.otaRunning = otaManager.isBusy();
    m_info.conversationRunning = conversationManager.isBusy();
    m_info.reminderRunning = reminderManager.isBusy();

    if (!healthy) {
        setError(SystemError::UNKNOWN);
    }

    return healthy;
}

const SystemInfo& SystemManager::getSystemInfo() const noexcept {
    return m_info;
}

SystemState SystemManager::getState() const noexcept {
    return m_currentState;
}

SystemError SystemManager::getError() const noexcept {
    return m_lastError;
}

bool SystemManager::isInitialized() const noexcept {
    return m_initialized;
}

bool SystemManager::isBusy() const noexcept {
    return m_currentState != SystemState::READY &&
           m_currentState != SystemState::LOW_POWER;
}

bool SystemManager::isSafeMode() const noexcept {
    return m_safeMode;
}

bool SystemManager::isHeadless() const noexcept {
    return m_headless;
}

HeadlessMode SystemManager::getHeadlessMode() const noexcept {
    return m_headlessMode;
}

// ============================================================================
// Private Methods
// ============================================================================

void SystemManager::changeState(SystemState newState) noexcept {
    if (m_currentState == newState) return;

    static constexpr bool validTransition[8][8] = {
        // BOOTING, INITIALIZING, READY, BUSY, LOW_POWER, UPDATING, ERROR, SHUTDOWN
        {0, 1, 0, 0, 0, 0, 1, 1},   // BOOTING
        {0, 0, 1, 0, 0, 0, 1, 1},   // INITIALIZING
        {0, 0, 0, 1, 1, 1, 1, 1},   // READY
        {0, 0, 1, 0, 1, 1, 1, 1},   // BUSY
        {0, 0, 1, 0, 0, 0, 1, 1},   // LOW_POWER
        {0, 0, 1, 0, 0, 0, 1, 1},   // UPDATING
        {1, 1, 1, 0, 0, 0, 0, 1},   // ERROR
        {0, 0, 0, 0, 0, 0, 0, 0}    // SHUTDOWN
    };

    if (!validTransition[static_cast<uint8_t>(m_currentState)]
                        [static_cast<uint8_t>(newState)]) {
        Logger::warning(kLogCategory, "Invalid state transition %d -> %d",
            static_cast<int>(m_currentState), static_cast<int>(newState));
        return;
    }

    Logger::debug(kLogCategory, "State: %d -> %d",
        static_cast<int>(m_currentState), static_cast<int>(newState));
    m_currentState = newState;
}

void SystemManager::setError(SystemError error) noexcept {
    if (m_lastError == error) return;
    m_lastError = error;
    Logger::error(kLogCategory, "Error: %d", static_cast<int>(error));
}

static unsigned long s_bootStageStartMs = 0;
static unsigned long s_bootStageLastLogMs = 0;

// Feed the task watchdog on boot progress and log the elapsed boot time so a
// slow-but-healthy init sequence never trips the (tighter) core watchdog.
// A genuinely stuck module never reaches this point, so the 30s app backstop
// still fires and reboots instead of hanging forever.
static void bootProgressWdt() noexcept {
    if (esp_task_wdt_status(nullptr) == ESP_OK) {
        esp_task_wdt_reset();
    }
    unsigned long now = millis();
    if (now - s_bootStageLastLogMs >= 25) {
        s_bootStageLastLogMs = now;
    }
}

bool SystemManager::initializeModules() noexcept {
    m_moduleInitStartTime = millis();
    s_bootStageStartMs = m_moduleInitStartTime;
    s_bootStageLastLogMs = s_bootStageStartMs;

    // Headless development mode: force from config before probing hardware.
    // Auto-detection (HEADLESS_MODE_AUTO) is applied when the display probe
    // below fails. In headless mode optional peripherals are disabled with a
    // warning and boot NEVER aborts for a missing peripheral.
    m_headless = HEADLESS_MODE_FORCE;
    m_headlessMode = m_headless ? HeadlessMode::HM_FORCED : HeadlessMode::HM_NORMAL;
    if (m_headless) {
        Logger::info(kLogCategory, "Headless mode forced by config");
    }

    // TTP223 touch sensor (digital, active-high) on TOUCH_PIN.
    // INPUT_PULLDOWN keeps the pin at a known "released" state when idle.
    pinMode(TOUCH_PIN, INPUT_PULLDOWN);

    // Check for safe mode (touch held during boot OR boot loop detected)
    // Non-blocking: poll with yield() instead of delay()
    if (!m_safeMode && !m_headless) {
        unsigned long touchStart = millis();
        unsigned long touchCount = 0;
        unsigned long touchSamples = 0;
        while (millis() - touchStart < SAFE_MODE_TOUCH_HOLD_MS) {
            if (digitalRead(TOUCH_PIN) == HIGH) {
                touchCount++;
            }
            touchSamples++;
            // Feed watchdog (only if the current task is registered) and yield to other tasks
            if (esp_task_wdt_status(nullptr) == ESP_OK) esp_task_wdt_reset();
            yield();
            delay(1);
        }
        // Safe mode if touch was held for >80% of samples
        m_safeMode = (touchSamples > 0) && (touchCount * 10 > touchSamples * 8);
        if (m_safeMode) {
            Logger::warning(kLogCategory, "Touch-based safe mode activated (%u/%u samples)", touchCount, touchSamples);
        }
    }

    if (m_safeMode) {
        Logger::warning(kLogCategory, "=== SAFE MODE ACTIVATED ===");
        // Only initialize essential modules for safe mode
    }

    // Initialize service status registry first so all modules can be tracked
    serviceStatusManager.initialize();
    serviceStatusManager.setStatus(ServiceId::SVC_SYSTEM, ServiceStatus::SS_ONLINE);

    // 1. StorageManager (must be first - other modules depend on it)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[0]);
    if (!storageManager.initialize()) {
        Logger::error(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[0]);
        serviceStatusManager.setStatus(ServiceId::SVC_STORAGE, ServiceStatus::SS_ERROR);
        errorManager.report(AuraErrorSeverity::ERROR, "Storage", "STORAGE_INIT_FAIL",
                            "Storage backend failed to initialize",
                            "SPIFFS/SD storage is unavailable; error history will be RAM-only");
    } else {
        serviceStatusManager.setStatus(ServiceId::SVC_STORAGE, ServiceStatus::SS_ONLINE);
    }
    serviceStatusManager.setStatus(ServiceId::SVC_SD_CARD,
        storageManager.isSDMounted() ? ServiceStatus::SS_ONLINE : ServiceStatus::SS_OFFLINE);
    bootProgressWdt();
    m_initModuleIndex = 1;

    // ErrorManager: needs StorageManager for persistence but can operate
    // RAM-only if storage is down, so it is brought up before anything else
    // that could want to report an init failure.
    Logger::info(kLogCategory, "Initializing: ErrorManager");
    errorManager.initialize();

    // 2. MemoryManager (needs StorageManager)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[12]);
        if (!memoryManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[12]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 2;

    // 2b. Service Framework (foundational)
    Logger::info(kLogCategory, "Initializing: ServiceManager");
    serviceManager.Initialize();


    Logger::info(kLogCategory, "Initializing: CapabilityManager");
    capabilityManager.Initialize();
    serviceManager.Register(&capabilityManager);


    Logger::info(kLogCategory, "Initializing: PlatformAbstraction");
    platform.Initialize();
    serviceManager.Register(&platform);


    Logger::info(kLogCategory, "Initializing: TaskScheduler");
    taskScheduler.Initialize();
    serviceManager.Register(&taskScheduler);

    // 3. DisplayManager (early for status feedback)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[2]);
    {
        bool displayOk = false;
        if (m_headless) {
            // Forced headless: display is intentionally disabled
            serviceStatusManager.setStatus(ServiceId::SVC_DISPLAY, ServiceStatus::SS_DISABLED);
        } else {
            displayOk = displayManager.initialize();
            if (!displayOk) {
                if (HEADLESS_MODE_AUTO) {
                    m_headless = true;
                    m_headlessMode = HeadlessMode::HM_AUTO;
                    serviceStatusManager.setStatus(ServiceId::SVC_DISPLAY, ServiceStatus::SS_DISABLED);
                    Logger::warning(kLogCategory, "Display not detected - AURA Headless Mode Enabled");
                } else {
                    Logger::error(kLogCategory, "Failed to initialize %s", kModuleNames[2]);
                    errorManager.report(AuraErrorSeverity::CRITICAL, "Display", "OLED_INIT_FAIL",
                                        "OLED display failed to initialize",
                                        "Boot aborted because the display is required and headless mode is off");
                    return false;
                }
            } else {
                serviceStatusManager.setStatus(ServiceId::SVC_DISPLAY, ServiceStatus::SS_ONLINE);
            }
        }
        serviceStatusManager.setHeadless(m_headless, m_headlessMode);

        if (displayOk) {
            if (m_safeMode) {
                displayManager.showMessage("SAFE MODE", "Recovery mode active");
            } else {
                displayManager.showBoot(10);
            }
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 3;

    // 4. WiFiManager (needed by network modules)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[1]);
    if (!wifiManager.initialize()) {
        Logger::error(kLogCategory, "Failed to initialize %s", kModuleNames[1]);
        errorManager.report(AuraErrorSeverity::CRITICAL, "WiFi", "WIFI_MANAGER_INIT_FAIL",
                            "WiFi manager failed to initialize", "");
        return false;
    }
    serviceStatusManager.setStatus(ServiceId::SVC_WIFI, ServiceStatus::SS_ONLINE);

    // Try to connect with stored credentials
    if (wifiManager.hasCredentials()) {
        Logger::info(kLogCategory, "Connecting to saved WiFi...");
        wifiManager.reconnect();
        displayManager.showBoot(30);
        // Do NOT start AP mode here - WifiManager will automatically fall back to AP
        // mode if reconnection fails (after max attempts or error state)
    } else {
        Logger::info(kLogCategory, "No WiFi credentials, starting AP mode");
        // Password selection is centralized in WifiManager: an empty
        // Secrets::AP_PASSWORD resolves to the dev default or a MAC-derived
        // password (production) via getAccessPointPassword().
        wifiManager.startAccessPoint(Secrets::AP_SSID, nullptr);
        displayManager.showBoot(50);
    }

    // 4b. UI Framework (wraps DisplayManager, needs it initialized; no rollback index shift)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: UIFramework");
        if (!uiFramework.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize UIFramework (continuing)");
        }
    }

    bootProgressWdt();
    m_initModuleIndex = 4;

    // 5. AudioManager
    if (!m_safeMode && !m_headless) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[3]);
        if (!audioManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[3]);
        }
    }
    if (m_headless) {
        serviceStatusManager.setStatus(ServiceId::SVC_MICROPHONE, ServiceStatus::SS_DISABLED);
        serviceStatusManager.setStatus(ServiceId::SVC_SPEAKER, ServiceStatus::SS_DISABLED);
    } else {
        serviceStatusManager.setStatus(ServiceId::SVC_MICROPHONE,
            audioManager.isInitialized() ? ServiceStatus::SS_ONLINE : ServiceStatus::SS_OFFLINE);
        serviceStatusManager.setStatus(ServiceId::SVC_SPEAKER,
            (AURA_HW_SPEAKER_PRESENT && audioManager.isInitialized())
                ? ServiceStatus::SS_ONLINE : ServiceStatus::SS_OFFLINE);
    }
    if (!AURA_HW_SPEAKER_PRESENT) {
        Logger::info(kLogCategory, "Speaker OFFLINE (hardware not connected)");
    }
    bootProgressWdt();
    m_initModuleIndex = 5;

    // 6. LedRing (visual feedback, independent)
    if (!m_safeMode && !m_headless && AURA_HW_LED_RING_PRESENT) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[13]);
        ledRing.initialize();
        Logger::info(kLogCategory, "Initialized: %s", kModuleNames[13]);
    } else if (!AURA_HW_LED_RING_PRESENT && !m_headless) {
        Logger::info(kLogCategory, "LedRing OFFLINE (hardware not connected)");
    }
    auraSystem.initialize();
    serviceStatusManager.setStatus(ServiceId::SVC_LED_RING,
        m_headless ? ServiceStatus::SS_DISABLED
                   : (AURA_HW_LED_RING_PRESENT ? ServiceStatus::SS_ONLINE : ServiceStatus::SS_OFFLINE));
    serviceStatusManager.setStatus(ServiceId::SVC_TOUCH,
        m_headless ? ServiceStatus::SS_DISABLED : ServiceStatus::SS_ONLINE);
    bootProgressWdt();
    m_initModuleIndex = 6;

    // 7. SoundManager (needs AudioManager)
    if (!m_safeMode && !m_headless && AURA_HW_SPEAKER_PRESENT) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[11]);
        if (!soundManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[11]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 7;

    // 7.5 AudioAssetManager (needs StorageManager + AudioManager + speaker)
    // Gated on speaker presence: its SD-backed cache directories stall boot on
    // boards whose SD card SPI stops responding (task watchdog abort), so it is
    // only enabled when there is actually an audio output to drive.
    if (!m_safeMode && AURA_HW_SPEAKER_PRESENT) {
        Logger::info(kLogCategory, "Initializing: AudioAssetManager");
        if (!AudioAssetManager::instance().begin()) {
            Logger::warning(kLogCategory, "Failed to initialize AudioAssetManager (continuing)");
        }
    } else if (!AURA_HW_SPEAKER_PRESENT) {
        Logger::info(kLogCategory, "AudioAssetManager OFFLINE (no speaker connected)");
    }

// 8. WebPortal (needs WiFi) - deferred to the end of init so the ~34 KB
    // it reserves (routes, server, WebSocket stack) stays free while the
    // memory-heavy late modules (CommandPalette, SceneEngine, Executive)
    // initialize. The portal is brought up right before READY below.
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[10]);
    displayManager.showBoot(70);
    bootProgressWdt();
    m_initModuleIndex = 8;

    // 9. SpeechToText (needs WiFi)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[4]);
        if (!speechToText.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[4]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 9;

    // 10. GeminiClient (needs WiFi)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[5]);
        if (!geminiClient.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[5]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 10;

    // 11. TextToSpeech (needs WiFi + Audio)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[6]);
        if (!textToSpeech.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[6]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 11;

    // Voice provider selection (Sarvam-ready). Sarvam AI is the active
    // placeholder provider until the integration lands; the factory never
    // returns a hardcoded provider into the rest of the codebase.
    if (!m_safeMode) {
        SpeechToTextProvider* sttProvider = createSpeechToTextProvider(DEFAULT_SPEECH_PROVIDER);
        TextToSpeechProvider* ttsProvider = createTextToSpeechProvider(DEFAULT_TTS_PROVIDER);
        Logger::info(kLogCategory, "Voice providers: STT=%s TTS=%s",
                     sttProvider ? sttProvider->providerName() : "none",
                     ttsProvider ? ttsProvider->providerName() : "none");
    }

    // 12. ConversationManager (needs STT, Gemini, TTS)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[7]);
        if (!conversationManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[7]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 12;

    // 13. ReminderManager
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[8]);
        if (!reminderManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[8]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 13;

    // 14. OtaManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[9]);
    if (!otaManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[9]);
    }
    bootProgressWdt();
    m_initModuleIndex = 14;

    if (m_safeMode) {
        Logger::info(kLogCategory, "Safe mode initialized - limited modules active");
        displayManager.showMessage("SAFE MODE", "Use Web Portal for OTA recovery");
        syncServiceStatuses();
        printBootBanner();
        return true;
    }

    // 15. SettingsManager (loads user prefs, no module deps)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[14]);
        if (!settingsManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[14]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 15;

    // 16. PluginManager (needs SD card via StorageManager)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[15]);
    if (!pluginManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[15]);
    }
    bootProgressWdt();
    m_initModuleIndex = 16;

    // 17. SkillManager (needs StorageManager)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[16]);
    if (!skillManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[16]);
    }
    bootProgressWdt();
    m_initModuleIndex = 17;

    // 18. PersonalityManager (standalone, no hard deps)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[17]);
    if (!personalityManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[17]);
    }
    bootProgressWdt();
    m_initModuleIndex = 18;

    // 19. ContextManager (depends on many managers being up)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[18]);
    if (!contextManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[18]);
    }
    bootProgressWdt();
    m_initModuleIndex = 19;

    // 20. (merged into BriefingManager)
    Logger::info(kLogCategory, "Skipping: %s", kModuleNames[19]);
    bootProgressWdt();
    m_initModuleIndex = 20;

    // 21. PerformanceManager (standalone monitor)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[20]);
    if (!performanceManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[20]);
    }
    bootProgressWdt();
    m_initModuleIndex = 21;

    // 22. CrashManager (needs StorageManager)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[21]);
    if (!crashManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[21]);
    }
    bootProgressWdt();
    m_initModuleIndex = 22;

    // 22.5 UptimeMonitor (standalone, NVS-backed)
    uptimeMonitor.begin();

    // 23. DiagnosticsManager (standalone)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[22]);
    if (!diagnosticsManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[22]);
    }
    bootProgressWdt();
    m_initModuleIndex = 23;

    // 24. KnowledgeGraphManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[23]);
    if (!knowledgeGraphManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[23]);
    }
    bootProgressWdt();
    m_initModuleIndex = 24;

    // 25. GoalManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[24]);
    if (!goalManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[24]);
    }
    bootProgressWdt();
    m_initModuleIndex = 25;

    // 26. HabitManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[25]);
    if (!habitManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[25]);
    }
    bootProgressWdt();
    m_initModuleIndex = 26;

    // 27. PlannerManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[26]);
    if (!plannerManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[26]);
    }
    bootProgressWdt();
    m_initModuleIndex = 27;

    // 28. FunctionRouter
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[27]);
    if (!functionRouter.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[27]);
    }
    bootProgressWdt();
    m_initModuleIndex = 28;

    // 29. ReflectionManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[28]);
    if (!reflectionManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[28]);
    }
    bootProgressWdt();
    m_initModuleIndex = 29;

    // 30. AutomationManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[29]);
    if (!automationManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[29]);
    }
    bootProgressWdt();
    m_initModuleIndex = 30;

    // 31. (merged into BriefingManager)
    Logger::info(kLogCategory, "Skipping: %s", kModuleNames[19]);
    bootProgressWdt();
    m_initModuleIndex = 31;

    // 32. StartupGreetingManager
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: StartupGreetingManager");
        if (!startupGreetingManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize StartupGreetingManager");
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 32;

    // 33. TinyAIManager (offline fallback - always available)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[30]);
    if (!tinyAIManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[30]);
    }
    bootProgressWdt();
    m_initModuleIndex = 33;

    // 34. TimelineManager (needs StorageManager)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[31]);
    if (!timelineManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[31]);
    }
    bootProgressWdt();
    m_initModuleIndex = 34;

    // 35. BriefingManager (needs TimelineManager, StorageManager)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[32]);
    if (!briefingManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[32]);
    }
    bootProgressWdt();
    m_initModuleIndex = 35;

    // 36. SemanticSearchManager (no storage deps, queries other managers)
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[33]);
    if (!semanticSearchManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[33]);
    }
    bootProgressWdt();
    m_initModuleIndex = 36;

    // 37. DecisionManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[34]);
    if (!decisionManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[34]);
    }
    bootProgressWdt();
    m_initModuleIndex = 37;

    // 38. LearningManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[35]);
    if (!learningManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[35]);
    }
    bootProgressWdt();
    m_initModuleIndex = 38;

    // 40. PredictionManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[37]);
    if (!predictionManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[37]);
    }
    bootProgressWdt();
    m_initModuleIndex = 40;

    // 41. DocumentManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[38]);
    if (!documentManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[38]);
    }
    bootProgressWdt();
    m_initModuleIndex = 41;

    // 42. WorkspaceManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[39]);
    if (!workspaceManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[39]);
    }
    bootProgressWdt();
    m_initModuleIndex = 42;

    // 43. VaultManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[40]);
    if (!vaultManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[40]);
    }
    bootProgressWdt();
    m_initModuleIndex = 43;

    // 44. EventBus
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[41]);
        if (!eventBus.initialize()) {
            Logger::error(kLogCategory, "Failed to initialize %s", kModuleNames[41]);
            errorManager.report(AuraErrorSeverity::ERROR, "EventBus", "EVENT_BUS_INIT_FAIL",
                                "Event bus failed to initialize",
                                "System services depending on the event bus are degraded");
            return false;
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 44;

    // 45. StudyManager
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[42]);
        if (!studyManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[42]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 45;

    // 46. CompanionManager
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[43]);
        if (!companionManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[43]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 46;

    // 47. EspNowManager (needs WiFi)
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: %s", kModuleNames[44]);
        if (!espNowManager.initialize()) {
            Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[44]);
        }
    }
    bootProgressWdt();
    m_initModuleIndex = 47;

    // 48. HealthMonitor
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[45]);
    if (!healthMonitor.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[45]);
    }
    bootProgressWdt();
    m_initModuleIndex = 48;

    // 49. SmartSearch
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[46]);
    if (!smartSearch.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[46]);
    }
    bootProgressWdt();
    m_initModuleIndex = 49;

    // 50. AnalyticsManager
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[47]);
    if (!analyticsManager.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[47]);
    }
    bootProgressWdt();
    m_initModuleIndex = 50;

    // 51. DeviceMesh
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[48]);
    if (!deviceMesh.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[48]);
    }
    bootProgressWdt();
    m_initModuleIndex = 51;

    // 52. ExecutiveAssistant
    Logger::info(kLogCategory, "Initializing: %s", kModuleNames[49]);
    if (!executiveAssistant.initialize()) {
        Logger::warning(kLogCategory, "Failed to initialize %s (continuing)", kModuleNames[49]);
    }
    bootProgressWdt();
    m_initModuleIndex = 52;

    // 53. New OS services
    if (!m_safeMode) {
        Logger::info(kLogCategory, "Initializing: AIPipeline");
        aiPipeline.Initialize();
        serviceManager.Register(&aiPipeline);


        Logger::info(kLogCategory, "Initializing: CommandPalette");
        commandPalette.Initialize();
        serviceManager.Register(&commandPalette);


        Logger::info(kLogCategory, "Initializing: SceneEngine");
        sceneEngine.Initialize();
        serviceManager.Register(&sceneEngine);


        Logger::info(kLogCategory, "Initializing: WorkflowEngine");
        workflowEngine.Initialize();
        serviceManager.Register(&workflowEngine);


        Logger::info(kLogCategory, "Initializing: LogManager");
        logManager.Initialize();
        serviceManager.Register(&logManager);


        Logger::info(kLogCategory, "Initializing: SecurityManager");
        securityManager.Initialize();
        serviceManager.Register(&securityManager);


        Logger::info(kLogCategory, "Initializing: ResilienceManager");
        resilienceManager.Initialize();
        serviceManager.Register(&resilienceManager);


        Logger::info(kLogCategory, "Initializing: DiagnosticSystem");
        diagnosticSystem.Initialize();
        serviceManager.Register(&diagnosticSystem);
    }

    // 54. HealthManager (self-recovery) - depends on all subsystems it monitors
    Logger::info(kLogCategory, "Initializing: HealthManager");
    healthManager.initialize();

    displayManager.showBoot(100);
    delay(100);

    syncServiceStatuses();
    printBootBanner();

    return true;
}

void SystemManager::updateModules() noexcept {
    storageManager.update();
    wifiManager.update();
    displayManager.update();
    audioManager.update();
    auraSystem.update();
    soundManager.update();
    memoryManager.update();
    settingsManager.update();
    speechToText.update();
    geminiClient.update();
    textToSpeech.update();
    conversationManager.update();
    reminderManager.update();
    otaManager.update();
    uptimeMonitor.update();
    webPortal.update();

    uiFramework.update();
    serviceManager.Update();

    if (!m_safeMode) {
        pluginManager.update();
        skillManager.update();
        personalityManager.update();
        contextManager.update();
        performanceManager.update();
        crashManager.update();
        diagnosticsManager.update();
        knowledgeGraphManager.update();
        goalManager.update();
        habitManager.update();
        plannerManager.update();
        reflectionManager.update();
        automationManager.update();
        startupGreetingManager.update();
        tinyAIManager.update();
        errorManager.update();
        timelineManager.update();
        briefingManager.update();
        semanticSearchManager.update();
        decisionManager.update();
        learningManager.update();
        predictionManager.update();
        documentManager.update();
        workspaceManager.update();
        vaultManager.update();
        eventBus.update();
        studyManager.update();
        companionManager.update();
        espNowManager.update();
        healthMonitor.update();
        smartSearch.update();
        analyticsManager.update();
        deviceMesh.update();
        executiveAssistant.update();
    }
}

void SystemManager::syncServiceStatuses() noexcept {
    auto sync = [](ServiceId id, bool ok) {
        serviceStatusManager.setStatus(id, ok ? ServiceStatus::SS_ONLINE : ServiceStatus::SS_OFFLINE);
    };

    sync(ServiceId::SVC_GEMINI, geminiClient.isInitialized());
    sync(ServiceId::SVC_LOCAL_AI, tinyAIManager.isInitialized());
    sync(ServiceId::SVC_MEMORY, memoryManager.isInitialized());
    sync(ServiceId::SVC_PLANNER, plannerManager.isInitialized());
    sync(ServiceId::SVC_GOALS, goalManager.isInitialized());
    sync(ServiceId::SVC_HABITS, habitManager.isInitialized());
    sync(ServiceId::SVC_KNOWLEDGE_GRAPH, knowledgeGraphManager.isInitialized());
    sync(ServiceId::SVC_REMINDERS, reminderManager.isInitialized());
    sync(ServiceId::SVC_WORKSPACES, workspaceManager.isInitialized());
    sync(ServiceId::SVC_OTA, otaManager.isInitialized());
    sync(ServiceId::SVC_SETTINGS, settingsManager.isInitialized());
    sync(ServiceId::SVC_COMPANION, companionManager.isInitialized());

    // No BME280/BH1750 sensor modules exist in this firmware build.
    serviceStatusManager.setStatus(ServiceId::SVC_SENSORS,
        m_headless ? ServiceStatus::SS_DISABLED : ServiceStatus::SS_OFFLINE);

    // Wi-Fi connectivity is dynamic; report manager availability as ONLINE
    // and let the WebSocket/status payload reflect live connection state.
    if (serviceStatusManager.getStatus(ServiceId::SVC_WIFI) == ServiceStatus::SS_UNKNOWN) {
        serviceStatusManager.setStatus(ServiceId::SVC_WIFI, ServiceStatus::SS_ONLINE);
    }
}

void SystemManager::refreshDynamicServiceStatus() noexcept {
    // Live peripheral re-detection: SD card hot-plug, Wi-Fi state.
    ServiceStatus sdStatus = ServiceStatus::SS_OFFLINE;
    if (m_headless) {
        sdStatus = ServiceStatus::SS_DISABLED;
    } else if (storageManager.isSDMounted()) {
        sdStatus = ServiceStatus::SS_ONLINE;
    }
    serviceStatusManager.setStatus(ServiceId::SVC_SD_CARD, sdStatus);

    ServiceStatus wifiStatus = (wifiManager.getState() != WifiState::ERROR)
                                   ? ServiceStatus::SS_ONLINE
                                   : ServiceStatus::SS_OFFLINE;
    serviceStatusManager.setStatus(ServiceId::SVC_WIFI, wifiStatus);
}

void SystemManager::printBootBanner() noexcept {
    // Serial boot banner (direct to Serial for visual formatting)
    Serial.println();
    Serial.println("==============================================");
    Serial.printf("        %s\n", aura::identity::kProjectName);
    Serial.println("----------------------------------------------");
    Serial.printf("Firmware : %s\n", aura::identity::kVersion);
    Serial.printf("Board    : %s\n", aura::identity::kPlatform);
    Serial.printf("Headless : %s (%s)\n",
        m_headless ? "ENABLED" : "DISABLED",
        m_headless ? serviceStatusManager.getHeadlessModeString() : "normal");

    Serial.printf("Enabled  : %s\n", serviceStatusManager.getConnectedJson().c_str());
    Serial.printf("Disabled : %s\n", serviceStatusManager.getDisabledJson().c_str());

    // Boot diagnostics: reset reason + memory + Wi-Fi state. Kept as a single
    // banner block so a repeat reset's cause remains diagnosable on the wire.
    {
        static const char* kResetNames[] = {
            "UNKNOWN",       // ESP_RST_UNKNOWN
            "POWER_ON",      // ESP_RST_POWERON
            "EXT",           // ESP_RST_EXT
            "SOFTWARE",      // ESP_RST_SW
            "PANIC",         // ESP_RST_PANIC
            "INT_WDT",       // ESP_RST_INT_WDT
            "TASK_WDT",      // ESP_RST_TASK_WDT
            "WDT",           // ESP_RST_WDT
            "DEEP_SLEEP",    // ESP_RST_DEEPSLEEP
            "BROWN_OUT",     // ESP_RST_BROWNOUT
            "SDIO",          // ESP_RST_SDIO
        };
        esp_reset_reason_t resetReason = esp_reset_reason();
        const char* resetName = (static_cast<int>(resetReason) >= 0 &&
                                 static_cast<int>(resetReason) <
                                     static_cast<int>(sizeof(kResetNames) / sizeof(kResetNames[0])))
                                    ? kResetNames[static_cast<int>(resetReason)]
                                    : "UNKNOWN";
        Serial.printf("Reset    : %s\n", resetName);
        Serial.printf("Boots    : %u\n", static_cast<unsigned>(uptimeMonitor.bootCount()));
        Serial.printf("Prev     : wifiState=%s reconnectCount=%u\n",
            wifiStateToString(wifiManager.getLastPersistedState()),
            static_cast<unsigned>(wifiManager.getLastPersistedReconnectCount()));
        Serial.printf("Heap     : free %u B, min %u B, largest block %u B\n",
            static_cast<unsigned>(ESP.getFreeHeap()),
            static_cast<unsigned>(ESP.getMinFreeHeap()),
            static_cast<unsigned>(ESP.getMaxAllocHeap()));
        Serial.printf("Wi-Fi    : %s (state=%d mode=%d channel=%u reconnectCount=%u lastEvent=%d)\n",
            wifiManager.getStateString(),
            static_cast<int>(wifiManager.getState()),
            static_cast<int>(WiFi.getMode()),
            static_cast<unsigned>(wifiManager.getChannel()),
            static_cast<unsigned>(wifiManager.getReconnectCount()),
            static_cast<int>(wifiManager.getLastEvent()));
    }

    Serial.printf("REST     : http://%s/api\n", WiFi.localIP().toString().c_str());
    Serial.printf("WebSocket: ws://%s:81\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gemini   : %s\n",
        geminiClient.isInitialized() ? "Online" : "Offline");
    Serial.println("----------------------------------------------");
    Serial.println("READY");
    Serial.println("==============================================");
    Serial.println();
}

void SystemManager::monitorMemory() noexcept {
    m_info.freeHeap = ESP.getFreeHeap();
    m_info.minimumHeap = ESP.getMinFreeHeap();
    m_info.maxBlockHeap = ESP.getMaxAllocHeap();
    if (m_info.freeHeap < 20000) {
        errorManager.report(AuraErrorSeverity::WARNING, "Memory", "LOW_HEAP",
                            "Low free heap",
                            String("Free heap dropped to ") + String(m_info.freeHeap) + " bytes");
    }
    // Fragmentation signal: a small largest contiguous block with otherwise
    // adequate free heap indicates heap fragmentation (worth surfacing once,
    // deduplicated by ErrorManager by component+code).
    if (m_info.freeHeap >= 30000 && m_info.maxBlockHeap < 8000) {
        errorManager.report(AuraErrorSeverity::WARNING, "Memory", "LOW_MAX_BLOCK",
                            "Heap fragmentation detected",
                            String("Largest contiguous block is ") + String(m_info.maxBlockHeap) + " bytes");
    } else {
        errorManager.resolve("Memory", "LOW_MAX_BLOCK");
    }
}

void SystemManager::monitorTasks() noexcept {
    // Real stack high-water-mark tracking. uxTaskGetStackHighWaterMark(nullptr)
    // returns the loop task's HWM (bytes of stack never used) - a reliable
    // indicator of a runaway/unbalanced call chain.
    static uint32_t minHwm = UINT32_MAX;
    const uint32_t hwm = uxTaskGetStackHighWaterMark(nullptr);
    if (hwm < minHwm) minHwm = hwm;
    if (hwm < 256) {
        Logger::error(kLogCategory, "Loop task stack low: HWM=%u bytes", (unsigned)hwm);
        healthManager.reportFailure(SubsystemId::TASKS, "loop task stack low");
        return;
    }
    healthManager.reportHealthy(SubsystemId::TASKS);
}

void SystemManager::monitorWiFi() noexcept {
    // WifiManager owns reconnection through its bounded state machine
    // (CONNECTING -> DISCONNECTED backoff -> ERROR bounded-rate retry).
    // Calling wifiManager.reconnect() here every health tick (5s) previously
    // bypassed that budget and forced repeated WiFi.mode()/begin() calls, a
    // documented ESP32 crash vector. Nothing is forced here; checkHealth()
    // surfaces link state (and sets m_info.wifiConnected) via errorManager.
    (void)wifiManager.getState();  // keep state sampled for future diagnostics

    // Report link state to the centralized recovery manager.
    if (wifiManager.isConnected() || wifiManager.getState() == WifiState::ACCESS_POINT) {
        healthManager.reportHealthy(SubsystemId::WIFI);
    } else {
        healthManager.reportFailure(SubsystemId::WIFI, "WiFi link down");
    }
}

void SystemManager::monitorSubsystems() noexcept {
    // WebSocket server
    if (webPortal.isRunning()) {
        healthManager.reportHealthy(SubsystemId::WEBSOCKET);
    } else {
        healthManager.reportFailure(SubsystemId::WEBSOCKET, "web server not running");
    }

    // Microphone (I2S read errors)
    static uint32_t lastMicErrors = 0;
    const uint32_t micErrors = audioManager.getMicReadErrorCount();
    if (micErrors > lastMicErrors || audioManager.hasMicStall()) {
        healthManager.reportFailure(SubsystemId::MICROPHONE, "I2S read errors or mic stall");
    } else {
        healthManager.reportHealthy(SubsystemId::MICROPHONE);
    }
    lastMicErrors = micErrors;

    // STT
    if (m_headless) {
        healthManager.markDisabled(SubsystemId::STT);
    } else if (speechToText.isInitialized() && speechToText.getError() == SpeechError::NONE) {
        healthManager.reportHealthy(SubsystemId::STT);
    } else {
        healthManager.reportFailure(SubsystemId::STT, "STT unavailable or error");
    }

    // AI
    if (geminiClient.isInitialized() && geminiClient.getError() == GeminiError::NONE) {
        healthManager.reportHealthy(SubsystemId::AI);
    } else {
        healthManager.reportFailure(SubsystemId::AI, "AI unavailable or error");
    }

    // TTS
    if (m_headless) {
        healthManager.markDisabled(SubsystemId::TTS);
    } else if (textToSpeech.isInitialized() && textToSpeech.getError() == TTSError::NONE) {
        healthManager.reportHealthy(SubsystemId::TTS);
    } else {
        healthManager.reportFailure(SubsystemId::TTS, "TTS unavailable or error");
    }

    // Speaker
    if (m_headless || !AURA_HW_SPEAKER_PRESENT) {
        healthManager.markDisabled(SubsystemId::SPEAKER);
    } else if (audioManager.isInitialized()) {
        healthManager.reportHealthy(SubsystemId::SPEAKER);
    } else {
        healthManager.reportFailure(SubsystemId::SPEAKER, "audio not initialized");
    }

    // OLED
    if (m_headless) {
        healthManager.markDisabled(SubsystemId::OLED);
    } else if (displayManager.isInitialized() && displayManager.isResponsive()) {
        healthManager.reportHealthy(SubsystemId::OLED);
    } else {
        healthManager.reportFailure(SubsystemId::OLED, "display not initialized or unresponsive");
    }

    // SD
    if (storageManager.isSDMounted()) {
        healthManager.reportHealthy(SubsystemId::SD);
    } else {
        healthManager.reportFailure(SubsystemId::SD, "SD not mounted");
    }

    // NVS
    if (settingsManager.isInitialized()) {
        healthManager.reportHealthy(SubsystemId::NVS);
    } else {
        healthManager.reportFailure(SubsystemId::NVS, "settings not initialized");
    }

    // OTA
    if (otaManager.getState() == OTAState::ERROR) {
        healthManager.reportFailure(SubsystemId::OTA, "OTA error state");
    } else {
        healthManager.reportHealthy(SubsystemId::OTA);
    }
}

void SystemManager::monitorOTA() noexcept {
    if (otaManager.isBusy()) {
        m_info.otaRunning = true;
        if (otaManager.getState() == OTAState::DOWNLOADING) {
            auraSystem.setMood(AuraMood::OTA);
            auraSystem.setOtaProgress(otaManager.getProgress());
            displayManager.showOTAProgress(otaManager.getProgress());
        }
    } else {
        m_info.otaRunning = false;
    }
}

void SystemManager::monitorReminders() noexcept {
    m_info.reminderRunning = reminderManager.isBusy();
}

void SystemManager::monitorConversation() noexcept {
    m_info.conversationRunning = conversationManager.isBusy();
}

void SystemManager::rollbackInitialization() noexcept {
    Logger::warning(kLogCategory, "Rolling back initialization...");

    // Reverse order cleanup - call proper shutdown methods, not destructors
    if (m_initModuleIndex >= 52) { /* ExecutiveAssistant has no shutdown */ }
    if (m_initModuleIndex >= 51) { /* DeviceMesh has no shutdown */ }
    if (m_initModuleIndex >= 50) analyticsManager.save();
    if (m_initModuleIndex >= 49) { /* SmartSearch has no state */ }
    if (m_initModuleIndex >= 48) { /* HealthMonitor has no shutdown */ }
    if (m_initModuleIndex >= 47) espNowManager.shutdown();
    if (m_initModuleIndex >= 46) { /* CompanionManager auto-saves */ }
    if (m_initModuleIndex >= 45) { /* StudyManager auto-saves */ }
    if (m_initModuleIndex >= 44) { /* EventBus has no state */ }
    if (m_initModuleIndex >= 43) { /* VaultManager auto-saves */ }
    if (m_initModuleIndex >= 42) { /* WorkspaceManager auto-saves */ }
    if (m_initModuleIndex >= 41) { /* DocumentManager auto-saves */ }
    if (m_initModuleIndex >= 40) { /* PredictionManager auto-saves */ }
    if (m_initModuleIndex >= 39) { executiveAssistant.saveRecommendations(); }
    if (m_initModuleIndex >= 38) { /* LearningManager auto-saves */ }
    if (m_initModuleIndex >= 37) { /* DecisionManager auto-saves */ }
    if (m_initModuleIndex >= 36) { /* SemanticSearchManager has no state */ }
    if (m_initModuleIndex >= 35) { /* BriefingManager auto-saves */ }
    if (m_initModuleIndex >= 34) { /* TimelineManager auto-saves */ }
    if (m_initModuleIndex >= 33) { /* TinyAIManager has no state */ }
    if (m_initModuleIndex >= 32) { /* StartupGreetingManager has no shutdown */ }
    if (m_initModuleIndex >= 31) briefingManager.saveSummaries();
    if (m_initModuleIndex >= 30) { /* AutomationManager auto-saves */ }
    if (m_initModuleIndex >= 29) { /* ReflectionManager auto-saves */ }
    if (m_initModuleIndex >= 28) { /* FunctionRouter has no shutdown */ }
    if (m_initModuleIndex >= 27) { /* PlannerManager auto-saves */ }
    if (m_initModuleIndex >= 26) { /* HabitManager auto-saves */ }
    if (m_initModuleIndex >= 25) { /* GoalManager auto-saves */ }
    if (m_initModuleIndex >= 24) { /* KnowledgeGraphManager auto-saves */ }
    if (m_initModuleIndex >= 23) crashManager.clearCrashes();
    if (m_initModuleIndex >= 22) { /* DiagnosticsManager has no shutdown */ }
    if (m_initModuleIndex >= 21) { /* PerformanceManager has no shutdown */ }
    if (m_initModuleIndex >= 20) { /* DailySummaryManager merged into BriefingManager */ }
    if (m_initModuleIndex >= 19) { /* ContextManager has no shutdown */ }
    if (m_initModuleIndex >= 18) { /* PersonalityManager auto-saves */ }
    if (m_initModuleIndex >= 17) { /* SkillManager auto-saves */ }
    if (m_initModuleIndex >= 16) { /* PluginManager auto-saves */ }
    if (m_initModuleIndex >= 14) otaManager.cancelUpdate();
    if (m_initModuleIndex >= 13) reminderManager.clearReminders();
    if (m_initModuleIndex >= 12) conversationManager.stopConversation();
    if (m_initModuleIndex >= 11) textToSpeech.stop();
    if (m_initModuleIndex >= 10) geminiClient.cancelRequest();
    if (m_initModuleIndex >= 9) speechToText.cancelRecognition();
    if (m_initModuleIndex >= 8) webPortal.stop();
    if (m_initModuleIndex >= 7) soundManager.stop();
    if (m_initModuleIndex >= 6) ledRing.turnOff();
    if (m_initModuleIndex >= 5) {
        audioManager.stopPlayback();
        audioManager.stopRecording();
    }
    if (m_initModuleIndex >= 4) wifiManager.disconnect();
    if (m_initModuleIndex >= 3) {
        displayManager.clear();
        displayManager.displayOff();
    }
    if (m_initModuleIndex >= 2) memoryManager.save();
    if (m_initModuleIndex >= 1) storageManager.unmountSPIFFS();
}

void SystemManager::validateCredentials() noexcept {
    auto warnIfMissing = [](const char* name, const char* value) {
        if (value == nullptr || value[0] == '\0') {
            Logger::warning(kLogCategory, "MISSING: %s is not configured", name);
        }
    };

    warnIfMissing("Secrets::GEMINI_API_KEY", Secrets::GEMINI_API_KEY);
    warnIfMissing("Secrets::SARVAM_API_KEY", Secrets::SARVAM_API_KEY);
    warnIfMissing("Secrets::AP_SSID", Secrets::AP_SSID);
    warnIfMissing("Secrets::AP_PASSWORD", Secrets::AP_PASSWORD);
    warnIfMissing("Secrets::WEB_USERNAME", Secrets::WEB_USERNAME);
    warnIfMissing("Secrets::WEB_PASSWORD", Secrets::WEB_PASSWORD);
}
