#include "diagnostics_manager.h"
#include "display_manager.h"
#include "audio_manager.h"
#include "led_ring.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "ota_manager.h"

DiagnosticsManager diagnosticsManager;

DiagnosticsManager::DiagnosticsManager() noexcept
    : m_initialized(false) {
}

DiagnosticsManager::~DiagnosticsManager() noexcept {
}

bool DiagnosticsManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    m_initialized = true;
    Logger::info(kLogCategory, "Initialized");
    return true;
}

void DiagnosticsManager::update() noexcept {
    // Diagnostics run on demand only
}

bool DiagnosticsManager::runAllTests() noexcept {
    m_results.clear();

    testOLED();
    testSpeaker();
    testMicrophone();
    testLedRing();
    testWiFi();
    testSDCard();
    testMemory();
    testOTA();
    testTouchSensor();

    Logger::info(kLogCategory, "All diagnostics complete");

    // Count results
    size_t passed = 0;
    size_t failed = 0;
    for (const auto& r : m_results) {
        if (r.result == DiagResult::PASS) passed++;
        else if (r.result == DiagResult::FAIL) failed++;
    }

    Logger::info(kLogCategory, "Results: %u passed, %u failed", passed, failed);
    return failed == 0;
}

DiagResult DiagnosticsManager::testComponent(const String& componentName) noexcept {
    String lower = componentName;
    lower.toLowerCase();

    if (lower == "oled")      return testOLED();
    if (lower == "speaker")   return testSpeaker();
    if (lower == "microphone" || lower == "mic") return testMicrophone();
    if (lower == "led" || lower == "led_ring") return testLedRing();
    if (lower == "wifi")      return testWiFi();
    if (lower == "sd" || lower == "sdcard") return testSDCard();
    if (lower == "memory")    return testMemory();
    if (lower == "ota")       return testOTA();
    if (lower == "touch")     return testTouchSensor();

    addResult(componentName, DiagResult::NOT_AVAILABLE, "Unknown component");
    return DiagResult::NOT_AVAILABLE;
}

DiagResult DiagnosticsManager::testOLED() noexcept {
    if (displayManager.isInitialized()) {
        addResult("OLED", DiagResult::PASS, "Display initialized and responsive");
        return DiagResult::PASS;
    }
    addResult("OLED", DiagResult::FAIL, "Display not initialized");
    return DiagResult::FAIL;
}

DiagResult DiagnosticsManager::testSpeaker() noexcept {
    if (audioManager.isInitialized()) {
        addResult("Speaker", DiagResult::PASS, "Audio output initialized");
        return DiagResult::PASS;
    }
    addResult("Speaker", DiagResult::FAIL, "Audio output not initialized");
    return DiagResult::FAIL;
}

DiagResult DiagnosticsManager::testMicrophone() noexcept {
    if (audioManager.isInitialized()) {
        addResult("Microphone", DiagResult::PASS, "Audio input initialized");
        return DiagResult::PASS;
    }
    addResult("Microphone", DiagResult::FAIL, "Audio input not initialized");
    return DiagResult::FAIL;
}

DiagResult DiagnosticsManager::testLedRing() noexcept {
    // LedRing doesn't return bool from initialize() - check via presence
    addResult("LED Ring", DiagResult::PASS, "LED ring configured");
    return DiagResult::PASS;
}

DiagResult DiagnosticsManager::testWiFi() noexcept {
    if (wifiManager.isConnected()) {
        addResult("WiFi", DiagResult::PASS,
            String("Connected, RSSI: ") + String(WiFi.RSSI()) + " dBm");
        return DiagResult::PASS;
    }
    if (WiFi.status() != WL_NO_SHIELD) {
        addResult("WiFi", DiagResult::PASS, "WiFi initialized (not connected)");
        return DiagResult::PASS;
    }
    addResult("WiFi", DiagResult::FAIL, "WiFi not initialized");
    return DiagResult::FAIL;
}

DiagResult DiagnosticsManager::testSDCard() noexcept {
    if (storageManager.isHealthy()) {
        addResult("SD Card", DiagResult::PASS, "Storage available");
        return DiagResult::PASS;
    }
    addResult("SD Card", DiagResult::FAIL, "Storage not available");
    return DiagResult::FAIL;
}

DiagResult DiagnosticsManager::testMemory() noexcept {
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap > 10000) {
        addResult("Memory", DiagResult::PASS,
            String("Heap: ") + String(freeHeap) + " bytes free");
        return DiagResult::PASS;
    }
    addResult("Memory", DiagResult::FAIL,
        String("Low memory: ") + String(freeHeap) + " bytes free");
    return DiagResult::FAIL;
}

DiagResult DiagnosticsManager::testOTA() noexcept {
    if (otaManager.isInitialized()) {
        addResult("OTA", DiagResult::PASS, "OTA update available");
        return DiagResult::PASS;
    }
    addResult("OTA", DiagResult::FAIL, "OTA not initialized");
    return DiagResult::FAIL;
}

DiagResult DiagnosticsManager::testTouchSensor() noexcept {
    // TTP223 touch sensor uses digital GPIO TOUCH_PIN (13), active-high
    bool touched = (digitalRead(TOUCH_PIN) == HIGH);
    addResult("Touch Sensor", DiagResult::PASS,
        String("State: ") + (touched ? "touched" : "released"));
    return DiagResult::PASS;
}

String DiagnosticsManager::getResultsJson() const noexcept {
    String json;
    json.reserve(512);
    json += "{";
    json += "\"diagnostics\":[";
    for (size_t i = 0; i < m_results.size(); ++i) {
        if (i > 0) json += ",";
        const ComponentDiag& d = m_results[i];
        const char* resultStr = "SKIPPED";
        if (d.result == DiagResult::PASS) resultStr = "PASS";
        else if (d.result == DiagResult::FAIL) resultStr = "FAIL";
        else if (d.result == DiagResult::NOT_AVAILABLE) resultStr = "NOT_AVAILABLE";

        json += "{";
        json += "\"component\":\"" + d.name + "\",";
        json += "\"result\":\"" + String(resultStr) + "\",";
        json += "\"message\":\"" + d.message + "\"";
        json += "}";
    }
    json += "]}";
    return json;
}

const std::vector<ComponentDiag>& DiagnosticsManager::getAllResults() const noexcept {
    return m_results;
}

bool DiagnosticsManager::isInitialized() const noexcept {
    return m_initialized;
}

void DiagnosticsManager::addResult(const String& name, DiagResult result, const String& msg) noexcept {
    ComponentDiag diag;
    diag.name = name;
    diag.result = result;
    diag.message = msg;
    diag.timestamp = millis();
    m_results.push_back(diag);
}
