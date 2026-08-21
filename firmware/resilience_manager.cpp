#include "resilience_manager.h"
#include <algorithm>
#include <WiFi.h>
#include "storage_manager.h"
#include "display_manager.h"
#include "memory_manager.h"
#include "ai_pipeline.h"
#include "aura_system.h"
#include "wifi_manager.h"

ResilienceManager resilienceManager;

ResilienceManager::ResilienceManager() noexcept
    : Service(kStaticName, BootPriority::NORMAL)
    , m_totalFailures(0)
    , m_totalRecoveries(0)
    , m_lastCleanup(0)
    , m_initialized(false)
    , m_wifiRecoverPending(false)
    , m_wifiRecoverStarted(0) {
    // Initialize all plans with defaults
    for (int i = 0; i < 12; ++i) {
        m_plans[i] = GetDefaultPlan(static_cast<FailureType>(i));
    }
}

ResilienceManager::~ResilienceManager() noexcept = default;

bool ResilienceManager::Initialize() noexcept {
    if (GetState() != ServiceState::UNINITIALIZED) return true;
    SetState(ServiceState::INITIALIZING);
    SetState(ServiceState::INITIALIZED);
    m_initialized = true;
    LOG_INFO(kLogCategory, "ResilienceManager initialized");
    return true;
}

void ResilienceManager::Update() noexcept {
    unsigned long now = millis();

    // Finalize asynchronous WiFi recovery without blocking the main loop.
    if (m_wifiRecoverPending) {
        if (wifiManager.isConnected() || (now - m_wifiRecoverStarted) >= kRecoveryWindowMs) {
            m_wifiRecoverPending = false;
            FinalizeWifiRecovery(wifiManager.isConnected());
        }
    }

    if (now - m_lastCleanup >= kCleanupIntervalMs) {
        m_lastCleanup = now;
        PruneRecords();
    }
}

void ResilienceManager::RegisterPlan(FailureType type, const FailureRecoveryPlan& plan) noexcept {
    m_plans[static_cast<uint8_t>(type)] = plan;
}

FailureRecoveryPlan ResilienceManager::GetPlan(FailureType type) const noexcept {
    return m_plans[static_cast<uint8_t>(type)];
}

void ResilienceManager::ReportFailure(FailureType type, const String& source,
                                       const String& details) noexcept {
    FailureRecord record;
    record.type = type;
    record.timestamp = millis();
    record.source = source;
    record.details = details;
    record.attemptCount = 0;
    record.recovered = false;

    m_records.push_back(record);
    m_totalFailures++;
    PruneRecords();

    LOG_WARNING(kLogCategory, "Failure reported: %s from %s (%s)",
                GetDefaultPlan(type).description.c_str(),
                source.c_str(), details.c_str());

    // Emit event
    if (eventBus.isInitialized()) {
        String payload = "{\"type\":" + String(static_cast<int>(type)) +
                         ",\"source\":\"" + source + "\"}";
        eventBus.publish(EventType::SYSTEM_ERROR, "ResilienceManager", payload);
    }

    // Attempt automatic recovery
    AttemptRecovery(type);
}

bool ResilienceManager::AttemptRecovery(FailureType type) noexcept {
    auto& plan = m_plans[static_cast<uint8_t>(type)];

    // Check retry limits
    uint32_t consecutiveFails = 0;
    for (const auto& r : m_records) {
        if (r.type == type && !r.recovered) consecutiveFails++;
    }

    if (consecutiveFails > plan.maxRetries) {
        LOG_ERROR(kLogCategory, "Max retries exceeded for failure type %d, entering safe mode",
                  static_cast<int>(type));
        if (plan.safeModeOnFailure) {
            // Enter safe mode (handled by SystemManager)
        }
        return false;
    }

    LOG_INFO(kLogCategory, "Attempting recovery for failure type %d (attempt %lu)",
             static_cast<int>(type), consecutiveFails + 1);

    bool recovered = false;
    switch (type) {
        case FailureType::WIFI_LOSS:
            // Asynchronous recovery: RecoverWiFi() returns immediately and the
            // outcome is finalized by Update(). Treat the attempt as in progress
            // so the retry budget isn't consumed while the window is open.
            RecoverWiFi();
            return true;
        case FailureType::SD_FAILURE:   recovered = RecoverSD(); break;
        case FailureType::RENDERER_CRASH: recovered = RecoverRenderer(); break;
        case FailureType::AI_FAILURE:   recovered = RecoverAI(); break;
        case FailureType::HEAP_EXHAUSTION: recovered = RecoverMemory(); break;
        default: recovered = false; break;
    }

    // Update records
    for (auto& r : m_records) {
        if (r.type == type && !r.recovered) {
            r.recovered = recovered;
            r.attemptCount++;
        }
    }

    if (recovered) {
        m_totalRecoveries++;
        LOG_INFO(kLogCategory, "Recovery successful for failure type %d",
                 static_cast<int>(type));
    }

    return recovered;
}

bool ResilienceManager::RecoverWiFi() noexcept {
    if (m_wifiRecoverPending) {
        // A recovery window is already in progress; report current state.
        return wifiManager.isConnected();
    }
    m_wifiRecoverPending = true;
    m_wifiRecoverStarted = millis();
    // Route through WifiManager (single radio authority) instead of calling
    // WiFi.reconnect() directly - direct calls bypassed WifiManager's state
    // machine and could stack begin() calls with other reconnect triggers.
    wifiManager.reconnect();
    LOG_INFO(kLogCategory, "WiFi recovery initiated (async, %lums window)",
             kRecoveryWindowMs);
    // Non-blocking: the outcome is finalized by Update().
    return wifiManager.isConnected();
}

void ResilienceManager::FinalizeWifiRecovery(bool recovered) noexcept {
    for (auto& r : m_records) {
        if (r.type == FailureType::WIFI_LOSS && !r.recovered) {
            r.recovered = recovered;
            r.attemptCount++;
        }
    }
    if (recovered) {
        m_totalRecoveries++;
        LOG_INFO(kLogCategory, "WiFi recovery successful");
    } else {
        LOG_ERROR(kLogCategory, "WiFi recovery failed after %lums window",
                  kRecoveryWindowMs);
    }
}

bool ResilienceManager::RecoverSD() noexcept {
    if (storageManager.isInitialized()) {
        storageManager.unmountSD();
        delay(100);
        return storageManager.mountSD() == StorageStatus::SUCCESS;
    }
    return false;
}

bool ResilienceManager::RecoverRenderer() noexcept {
    if (displayManager.isInitialized()) {
        displayManager.clear();
        displayManager.showHome();
        auraSystem.enterIdle();
        return true;
    }
    return false;
}

bool ResilienceManager::RecoverAI() noexcept {
    // AI recovery is handled by AIPipeline's provider fallback
    return aiPipeline.GetActiveProvider() != nullptr;
}

bool ResilienceManager::RecoverMemory() noexcept {
    // Attempt to free heap
    uint32_t freeHeap = ESP.getFreeHeap();
    return freeHeap > 30000;
}

bool ResilienceManager::CheckAllSystems() noexcept {
    bool allOk = true;

    // WiFi
    if (!wifiManager.isConnected()) {
        ReportFailure(FailureType::WIFI_LOSS, "system_check");
        allOk = false;
    }

    // Memory
    if (ESP.getFreeHeap() < 20000) {
        ReportFailure(FailureType::HEAP_EXHAUSTION, "system_check");
        allOk = false;
    }

    // Storage
    if (storageManager.isInitialized() && !storageManager.isSDMounted()) {
        ReportFailure(FailureType::SD_FAILURE, "system_check");
        allOk = false;
    }

    return allOk;
}

String ResilienceManager::GetResilienceReport() noexcept {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "Total: %lu failures, %lu recoveries (%.0f%%)",
        m_totalFailures, m_totalRecoveries,
        m_totalFailures > 0 ? (100.0f * m_totalRecoveries / m_totalFailures) : 100.0f);
    return String(buf);
}

uint32_t ResilienceManager::GetTotalFailures() const noexcept { return m_totalFailures; }
uint32_t ResilienceManager::GetTotalRecoveries() const noexcept { return m_totalRecoveries; }

uint32_t ResilienceManager::GetConsecutiveFailures(FailureType type) const noexcept {
    uint32_t count = 0;
    for (const auto& r : m_records) {
        if (r.type == type && !r.recovered) count++;
    }
    return count;
}

float ResilienceManager::GetRecoveryRate() const noexcept {
    if (m_totalFailures == 0) return 100.0f;
    return 100.0f * static_cast<float>(m_totalRecoveries) / static_cast<float>(m_totalFailures);
}

void ResilienceManager::HandleEvent(const String& eventType, const String& eventData) noexcept {
    Service::HandleEvent(eventType, eventData);
}

FailureRecoveryPlan ResilienceManager::GetDefaultPlan(FailureType type) const noexcept {
    FailureRecoveryPlan plan;
    plan.type = type;

    switch (type) {
        case FailureType::WIFI_LOSS:
            plan.description = "WiFi connection lost";
            plan.maxRetries = 5;
            plan.restartModule = true;
            plan.cooldownMs = 10000;
            break;
        case FailureType::CLOUD_TIMEOUT:
            plan.description = "Cloud API timeout";
            plan.maxRetries = 3;
            plan.cooldownMs = 30000;
            break;
        case FailureType::AI_FAILURE:
            plan.description = "AI provider failure";
            plan.maxRetries = 2;
            plan.cooldownMs = 60000;
            break;
        case FailureType::SD_FAILURE:
            plan.description = "SD card failure";
            plan.maxRetries = 2;
            plan.restartModule = true;
            plan.cooldownMs = 60000;
            break;
        case FailureType::RENDERER_CRASH:
            plan.description = "Display renderer crash";
            plan.maxRetries = 3;
            plan.restartModule = true;
            break;
        case FailureType::MEMORY_CORRUPTION:
            plan.description = "Memory corruption detected";
            plan.maxRetries = 1;
            plan.safeModeOnFailure = true;
            break;
        case FailureType::HEAP_EXHAUSTION:
            plan.description = "Heap memory exhausted";
            plan.maxRetries = 3;
            plan.cooldownMs = 5000;
            break;
        default:
            plan.description = "Unknown failure";
            plan.maxRetries = 1;
            break;
    }

    return plan;
}

void ResilienceManager::PruneRecords() noexcept {
    while (m_records.size() > kMaxRecords) {
        m_records.erase(m_records.begin());
    }
}