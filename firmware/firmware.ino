/**
 * @file AURA_Programs.ino
 * @brief Firmware entry point for AURA AI Desktop Assistant.
 *
 * This file is ONLY the firmware entry point.
 * All business logic remains inside the managers.
 * SystemManager coordinates initialization and runtime.
 */

#include "config.h"
#include "secrets.h"
#include "logger.h"
#include "system_manager.h"

#include <esp_system.h>
#include <esp_task_wdt.h>
#include <WiFi.h>

// Forward declarations for boot-time helpers defined below
static void logBootDiagnostics();
static void initializeWatchdog();
static const char* getResetReasonString(esp_reset_reason_t reason);

// Use extern global from system_manager.cpp (avoid duplicate instances)
extern SystemManager systemManager;

// ====================================================
// CONSTANTS
// ====================================================
static constexpr uint32_t kSerialBaudRate = 115200;
static constexpr uint32_t kSerialTimeoutMs = 500;
static constexpr uint32_t kWatchdogTimeoutMs = 30000;

// Stack high-water-mark diagnostics (Phase 6): log ONLY when the loop task's
// free stack drops below the warning threshold, and never spam (edge-triggered).
static constexpr uint32_t kStackHwmCheckMs = 60000UL;
static constexpr uint32_t kStackHwmWarnBytes = 2048UL;

// Tracks whether the loop task is actually registered with the task WDT.
// esp_task_wdt_reset() is only safe to call when this is true.
static bool g_watchdogArmed = false;

// ====================================================
// ARDUINO ENTRY POINTS
// ====================================================

void setup() {
    // 1. Initialize Serial FIRST (for early diagnostics)
    Serial.begin(kSerialBaudRate);
    while (!Serial && millis() < kSerialTimeoutMs) {
        delay(1);
    }

    // 2. Initialize Logger
    if (!Logger::initialize()) {
        // Logger failed - continue anyway, Serial still works
    }

    // 3. Boot Banner & Diagnostics
    logBootDiagnostics();

    // 4. Initialize Watchdog BEFORE module init to prevent boot hangs
    initializeWatchdog();

    // 5. Initialize SystemManager (initializes all modules in dependency order)
    if (!systemManager.initialize()) {
        Logger::error("SYSTEM", "System initialization failed - entering ERROR state");
        // SystemManager handles rollback and enters ERROR state internally
    } else {
        Logger::info("SYSTEM", "System initialization complete");
        Logger::info("SYSTEM", "Free heap: %u bytes", ESP.getFreeHeap());
    }
}

void loop() {
    // Feed watchdog (only if the current task is actually registered)
    if (g_watchdogArmed) {
        esp_task_wdt_reset();
    }

    // Run system state machine (updates all modules)
    systemManager.run();

    // Stack high-water-mark diagnostics: warn only when the loop task is close
    // to exhausting its stack. FreeRTOS measures the minimum free stack ever
    // seen, so this is a one-shot edge trigger per low-water event.
    static unsigned long lastHwmCheck = 0;
    static bool hwmWarned = false;
    const unsigned long now = millis();
    if (now - lastHwmCheck >= kStackHwmCheckMs) {
        lastHwmCheck = now;
        const uint32_t freeStack = uxTaskGetStackHighWaterMark(nullptr);
        if (freeStack < kStackHwmWarnBytes) {
            if (!hwmWarned) {
                hwmWarned = true;
                Logger::warning("SYSTEM", "Loop task stack low: %u bytes free", (unsigned)freeStack);
            }
        } else {
            hwmWarned = false;
        }
    }

    // Yield to other tasks
    yield();
}

// ====================================================
// PRIVATE HELPERS
// ====================================================

void logBootDiagnostics() {
    // Serial boot banner (direct to Serial, not Logger, for visual formatting)
    Serial.println();
    Serial.println("==============================================");
    Serial.printf("        %s\n", aura::identity::kProjectName);
    Serial.println("----------------------------------------------");
    Serial.printf("Firmware : %s\n", aura::identity::kVersion);
    Serial.printf("Hardware : %s\n", aura::identity::kHardwareRev);
    Serial.printf("Codename : %s\n", aura::identity::kCodename);
    Serial.printf("Platform : %s\n", aura::identity::kPlatform);
    Serial.printf("Build    : %s %s\n", aura::identity::kBuildDate, aura::identity::kBuildTime);
    Serial.printf("Compiler : %s\n", aura::identity::kCompiler);
    Serial.printf("Author   : %s\n", aura::identity::kAuthor);
    Serial.println("==============================================");
    Serial.println();

    // Firmware info via Logger
    Logger::info("SYSTEM", "Firmware : %s", aura::identity::kVersion);
    Logger::info("SYSTEM", "Codename : %s", aura::identity::kCodename);
    Logger::info("SYSTEM", "Build    : %s %s", aura::identity::kBuildDate, aura::identity::kBuildTime);

    // ESP32 Chip Info
    Logger::info("SYSTEM", "Chip: %s (%d cores, rev %d)",
        ESP.getChipModel(), ESP.getChipCores(), ESP.getChipRevision());
    Logger::info("SYSTEM", "CPU: %d MHz", ESP.getCpuFreqMHz());
    Logger::info("SYSTEM", "Flash: %u MB (%d MHz)",
        ESP.getFlashChipSize() / (1024 * 1024),
        ESP.getFlashChipSpeed() / 1000000);
    Logger::info("SYSTEM", "SDK: %s", ESP.getSdkVersion());
    Logger::info("SYSTEM", "Arduino: %d", ARDUINO);

    // Reset Reason (single call — value is chip-wide, not per-core)
    const esp_reset_reason_t resetReason = esp_reset_reason();
    Logger::info("SYSTEM", "Reset reason: %s", getResetReasonString(resetReason));

    // Memory
    Logger::info("SYSTEM", "Heap: %u free, %u min, %u max block",
        ESP.getFreeHeap(),
        ESP.getMinFreeHeap(),
        ESP.getMaxAllocHeap());

    // Network
    Logger::info("SYSTEM", "MAC: %s", WiFi.macAddress().c_str());

    Logger::info("SYSTEM", "Booting...");
}

void initializeWatchdog() {
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = kWatchdogTimeoutMs,
        .trigger_panic = true
    };
    esp_err_t err = esp_task_wdt_init(&wdt_config);
    if (err == ESP_ERR_INVALID_STATE) {
        // TWDT is already initialized (e.g. by the Arduino core / WiFi stack)
        // with a tighter timeout (typically 5-15s). Boot takes ~16s, so that
        // shorter backstop aborts even when nothing is actually hung. Tear
        // it down and re-create it with our intended 30s timeout.
        Logger::warning("SYSTEM", "Watchdog pre-initialized with tighter timeout; recreating at %lu ms", kWatchdogTimeoutMs);
        esp_task_wdt_deinit();
        err = esp_task_wdt_init(&wdt_config);
    }
    if (err == ESP_OK) {
        // Register the current (loop) task with the task WDT. Ignore the
        // direct return value; esp_task_wdt_status() below is authoritative.
        esp_task_wdt_add(nullptr);
    }
    if (esp_task_wdt_status(nullptr) == ESP_OK) {
        g_watchdogArmed = true;
        Logger::info("SYSTEM", "Watchdog enabled (%lu ms)", kWatchdogTimeoutMs);
    } else {
        g_watchdogArmed = false;
        Logger::warning("SYSTEM", "Watchdog not armed (loop task unregistered)");
    }
}

const char* getResetReasonString(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:    return "POWER_ON";
        case ESP_RST_SW:         return "SOFTWARE";
        case ESP_RST_PANIC:      return "PANIC";
        case ESP_RST_INT_WDT:    return "INT_WDT";
        case ESP_RST_TASK_WDT:   return "TASK_WDT";
        case ESP_RST_WDT:        return "WDT";
        case ESP_RST_DEEPSLEEP:  return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT:   return "BROWN_OUT";
        case ESP_RST_SDIO:       return "SDIO";
        case ESP_RST_EXT:        return "EXT";
        default:                 return "UNKNOWN";
    }
}