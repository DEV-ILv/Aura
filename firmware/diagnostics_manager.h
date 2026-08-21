#ifndef AURA_DIAGNOSTICS_MANAGER_H
#define AURA_DIAGNOSTICS_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"

/**
 * @enum DiagResult
 * @brief Diagnostic test result
 */
enum class DiagResult : uint8_t {
    PASS,       ///< Component passed
    FAIL,       ///< Component failed
    SKIPPED,    ///< Component not tested
    NOT_AVAILABLE ///< Component not available
};

/**
 * @struct ComponentDiag
 * @brief Diagnostic result for a single component
 */
struct ComponentDiag {
    String name;            ///< Component name
    DiagResult result;      ///< Test result
    String message;         ///< Result details
    unsigned long timestamp; ///< Test timestamp

    ComponentDiag() noexcept
        : result(DiagResult::SKIPPED), timestamp(0) {}
};

/**
 * @class DiagnosticsManager
 * @brief Hardware and software component diagnostics
 *
 * Tests each system component and returns PASS/FAIL:
 * OLED display, Speaker, Microphone, LED Ring, WiFi,
 * SD Card, Memory (RAM/flash), OTA, Touch Sensor.
 */
class DiagnosticsManager {
public:
    DiagnosticsManager() noexcept;
    ~DiagnosticsManager() noexcept;

    DiagnosticsManager(const DiagnosticsManager&) = delete;
    DiagnosticsManager& operator=(const DiagnosticsManager&) = delete;
    DiagnosticsManager(DiagnosticsManager&&) = delete;
    DiagnosticsManager& operator=(DiagnosticsManager&&) = delete;

    /**
     * @brief Initialize diagnostics manager
     * @return true if initialized
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update diagnostics manager
     */
    void update() noexcept;

    /**
     * @brief Run all diagnostics tests
     * @return true if all components passed
     */
    [[nodiscard]] bool runAllTests() noexcept;

    /**
     * @brief Test a specific component
     * @param componentName Component to test
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testComponent(const String& componentName) noexcept;

    /**
     * @brief Test OLED display
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testOLED() noexcept;

    /**
     * @brief Test speaker
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testSpeaker() noexcept;

    /**
     * @brief Test microphone
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testMicrophone() noexcept;

    /**
     * @brief Test LED ring
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testLedRing() noexcept;

    /**
     * @brief Test WiFi
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testWiFi() noexcept;

    /**
     * @brief Test SD card
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testSDCard() noexcept;

    /**
     * @brief Test memory (RAM + flash)
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testMemory() noexcept;

    /**
     * @brief Test OTA
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testOTA() noexcept;

    /**
     * @brief Test touch sensor
     * @return DiagResult
     */
    [[nodiscard]] DiagResult testTouchSensor() noexcept;

    /**
     * @brief Get all diagnostic results as JSON
     * @return JSON string
     */
    [[nodiscard]] String getResultsJson() const noexcept;

    /**
     * @brief Get all component diagnostics
     * @return Vector of diagnostics
     */
    [[nodiscard]] const std::vector<ComponentDiag>& getAllResults() const noexcept;

    /**
     * @brief Check if initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "Diagnostics";

    void addResult(const String& name, DiagResult result, const String& msg) noexcept;

    bool m_initialized;
    std::vector<ComponentDiag> m_results;
};

extern DiagnosticsManager diagnosticsManager;

#endif // AURA_DIAGNOSTICS_MANAGER_H
