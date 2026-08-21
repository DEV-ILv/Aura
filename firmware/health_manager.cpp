#include "health_manager.h"
#include "logger.h"
#include "wifi_manager.h"
#include "web_portal.h"
#include "audio_manager.h"
#include "sarvam_stt.h"
#include "gemini_client.h"
#include "sarvam_tts.h"
#include "storage_manager.h"
#include "settings_manager.h"
#include "ota_manager.h"
#include "display_manager.h"
#include "system_manager.h"

HealthManager healthManager;

namespace {
constexpr const char* kTag = "Health";

// Only a narrow set of failures justify a device reboot as a last resort.
// Optional subsystems (speaker, OLED, SD, OTA), WiFi (self-recovering via its
// own state machine) and network-dependent services (STT/AI/TTS) must never
// trigger a reboot: their absence or an external failure cannot be fixed by
// restarting and would otherwise produce an indefinite reboot loop.
bool isRebootCritical(SubsystemId id) noexcept {
    switch (id) {
        case SubsystemId::MICROPHONE:
        case SubsystemId::NVS:
        case SubsystemId::TASKS:
            return true;
        default:
            return false;
    }
}

// Per-subsystem recovery actions. Each action must be safe to call from the
// main loop and must NOT block for long periods. Actions reuse the bounded,
// existing recovery functions of the owning subsystem.
void recoverWifi() noexcept {
    if (!wifiManager.isConnected()) wifiManager.reconnect();
}
void recoverWebSocket() noexcept {
    if (!webPortal.isRunning()) webPortal.start();
}
void recoverMicrophone() noexcept {
    // Bounded, idempotent recovery: recreate the I2S mic path. The previous
    // startRecording()-only action was a no-op while already RECORDING and
    // could never revive a failed I2S driver. Returns false (no reboot) if the
    // driver still cannot be created, letting HealthManager escalate to FAILED.
    audioManager.recoverMicrophone();
}
void recoverStt() noexcept {
    speechToText.cancelRecognition();
}
void recoverAi() noexcept {
    geminiClient.cancelRequest();
}
void recoverTts() noexcept {
    textToSpeech.stop();
}
void recoverSpeaker() noexcept {
    audioManager.stopPlayback();
}
void recoverOled() noexcept {
    displayManager.reinitialize();
}
void recoverSd() noexcept {
    if (storageManager.isSDMounted()) {
        storageManager.unmountSD();
    }
    storageManager.mountSD();
}
void recoverNvs() noexcept {
    if (!settingsManager.isInitialized()) settingsManager.load();
}
void recoverOta() noexcept {
    otaManager.cancelUpdate();
}
void recoverTasks() noexcept {
    // No direct action: stuck-task recovery is bounded via reboot last resort.
}
} // namespace

void HealthManager::initialize() noexcept {
    if (m_initialized) return;

    m_bootTime = millis();
    m_lastRebootMs = m_bootTime;

    registerRecovery(SubsystemId::WIFI, recoverWifi);
    registerRecovery(SubsystemId::WEBSOCKET, recoverWebSocket);
    registerRecovery(SubsystemId::MICROPHONE, recoverMicrophone);
    registerRecovery(SubsystemId::STT, recoverStt);
    registerRecovery(SubsystemId::AI, recoverAi);
    registerRecovery(SubsystemId::TTS, recoverTts);
    registerRecovery(SubsystemId::SPEAKER, recoverSpeaker);
    registerRecovery(SubsystemId::OLED, recoverOled);
    registerRecovery(SubsystemId::SD, recoverSd);
    registerRecovery(SubsystemId::NVS, recoverNvs);
    registerRecovery(SubsystemId::OTA, recoverOta);
    registerRecovery(SubsystemId::TASKS, recoverTasks);

    m_initialized = true;
    Logger::info(kTag, "HealthManager initialized (%u subsystems)",
                 static_cast<unsigned>(SubsystemId::COUNT));
}

void HealthManager::registerRecovery(SubsystemId id, void (*action)(void)) noexcept {
    if (static_cast<size_t>(id) >= static_cast<size_t>(SubsystemId::COUNT)) return;
    m_states[static_cast<size_t>(id)].recover = action;
}

void HealthManager::update() noexcept {
    if (!m_initialized) return;

    unsigned long now = millis();

    // Bounded reboot as a last resort: only when a critical subsystem has been
    // FAILED for a sustained period, the device has been up long enough, and a
    // reboot cooldown has elapsed. One subsystem failure must never crash the
    // device immediately.
    if (AURA_HEALTH_REBOOT_ENABLED && (now - m_lastRebootMs >= kRebootCooldownMs) &&
        (now - m_bootTime >= kMinUptimeMs)) {
        for (size_t i = 0; i < static_cast<size_t>(SubsystemId::COUNT); ++i) {
            if (!isRebootCritical(static_cast<SubsystemId>(i))) continue;
            const SubsystemState& st = m_states[i];
            if (st.health == SubsystemHealth::FAILED && st.lastFailedMs != 0 &&
                (now - st.lastFailedMs >= kFailedToRebootMs)) {
                char reason[48];
                snprintf(reason, sizeof(reason), "health_reboot_%s",
                         subsystemName(static_cast<SubsystemId>(i)));
                Logger::warning(kTag,
                    "[AURA][RECOVERY] %s FAILED for %lu ms - reboot as last resort",
                    subsystemName(static_cast<SubsystemId>(i)),
                    static_cast<unsigned long>(now - st.lastFailedMs));
                m_lastRebootMs = now;
                systemManager.requestRestart(reason, false);
                return;
            }
        }
    }
}

void HealthManager::reportFailure(SubsystemId id, const char* reason) noexcept {
    if (!m_initialized) return;
    const size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(SubsystemId::COUNT)) return;

    SubsystemState& st = m_states[idx];
    if (st.health == SubsystemHealth::OFFLINE) return;

    unsigned long now = millis();
    st.lastFailureMs = now;

    // Already recovering / failed: only re-log if cooldown allows a new attempt.
    if (st.health != SubsystemHealth::HEALTHY &&
        st.health != SubsystemHealth::DEGRADED) {
        if (now < st.cooldownUntilMs) return;
    }

    if (st.attempts >= kMaxAttempts) {
        if (st.health != SubsystemHealth::FAILED) {
            st.health = SubsystemHealth::FAILED;
            st.lastFailedMs = now;
            RecoveryEvent ev;
            ev.timestampMs = now;
            ev.subsystem = id;
            ev.reason = reason;
            ev.attempt = st.attempts;
            ev.success = false;
            ev.freeHeap = ESP.getFreeHeap();
            ev.maxBlock = ESP.getMaxAllocHeap();
            st.last = ev;
            logRecovery(ev, false);
        }
        return;
    }

    st.health = SubsystemHealth::RECOVERING;
    st.attempts++;
    st.cooldownUntilMs = now + kCooldownMs;

    RecoveryEvent ev;
    ev.timestampMs = now;
    ev.subsystem = id;
    ev.reason = reason;
    ev.attempt = st.attempts;
    ev.success = false;
    ev.freeHeap = ESP.getFreeHeap();
    ev.maxBlock = ESP.getMaxAllocHeap();
    st.last = ev;
    logRecovery(ev, false);

    runRecovery(st, id);
}

void HealthManager::reportHealthy(SubsystemId id) noexcept {
    if (!m_initialized) return;
    const size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(SubsystemId::COUNT)) return;

    SubsystemState& st = m_states[idx];
    if (st.health == SubsystemHealth::OFFLINE) return;

    if (st.health != SubsystemHealth::HEALTHY) {
        RecoveryEvent ev = st.last;
        ev.timestampMs = millis();
        ev.success = true;
        ev.freeHeap = ESP.getFreeHeap();
        ev.maxBlock = ESP.getMaxAllocHeap();
        st.last = ev;
        m_recoveryCount++;
        logRecovery(ev, true);
    }

    st.health = SubsystemHealth::HEALTHY;
    st.attempts = 0;
    st.lastFailedMs = 0;
    st.cooldownUntilMs = 0;
}

void HealthManager::markDisabled(SubsystemId id) noexcept {
    const size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(SubsystemId::COUNT)) return;
    m_states[idx].health = SubsystemHealth::OFFLINE;
}

SubsystemHealth HealthManager::getHealth(SubsystemId id) const noexcept {
    const size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(SubsystemId::COUNT)) return SubsystemHealth::OFFLINE;
    return m_states[idx].health;
}

const RecoveryEvent* HealthManager::lastEvent(SubsystemId id) const noexcept {
    const size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(SubsystemId::COUNT)) return nullptr;
    return &m_states[idx].last;
}

uint32_t HealthManager::getRecoveryCount() const noexcept {
    return m_recoveryCount;
}

uint8_t HealthManager::getAttempts(SubsystemId id) const noexcept {
    const size_t idx = static_cast<size_t>(id);
    if (idx >= static_cast<size_t>(SubsystemId::COUNT)) return 0;
    return m_states[idx].attempts;
}

const char* HealthManager::subsystemName(SubsystemId id) noexcept {
    switch (id) {
        case SubsystemId::WIFI:       return "WIFI";
        case SubsystemId::WEBSOCKET:  return "WEBSOCKET";
        case SubsystemId::MICROPHONE: return "MICROPHONE";
        case SubsystemId::STT:        return "STT";
        case SubsystemId::AI:         return "AI";
        case SubsystemId::TTS:        return "TTS";
        case SubsystemId::SPEAKER:    return "SPEAKER";
        case SubsystemId::OLED:       return "OLED";
        case SubsystemId::SD:         return "SD";
        case SubsystemId::NVS:        return "NVS";
        case SubsystemId::OTA:        return "OTA";
        case SubsystemId::TASKS:      return "TASKS";
        default:                      return "?";
    }
}

const char* HealthManager::healthName(SubsystemHealth h) noexcept {
    switch (h) {
        case SubsystemHealth::HEALTHY:    return "HEALTHY";
        case SubsystemHealth::DEGRADED:   return "DEGRADED";
        case SubsystemHealth::FAILED:     return "FAILED";
        case SubsystemHealth::RECOVERING: return "RECOVERING";
        case SubsystemHealth::OFFLINE:   return "OFFLINE";
        default:                          return "?";
    }
}

void HealthManager::runRecovery(SubsystemState& st, SubsystemId id) noexcept {
    if (!st.recover) return;
    Logger::warning(kTag,
        "[AURA][RECOVERY] %s failure, reason=%s, attempt=%u, free=%u, largest=%u",
        subsystemName(id), st.last.reason ? st.last.reason : "unknown",
        static_cast<unsigned>(st.attempts),
        static_cast<unsigned>(st.last.freeHeap),
        static_cast<unsigned>(st.last.maxBlock));
    st.recover();
    st.lastRecoveryMs = millis();
}

void HealthManager::logRecovery(const RecoveryEvent& ev, bool ok) noexcept {
    if (ok) {
        Logger::info(kTag,
            "[AURA][RECOVERY] %s recovered, result=SUCCESS, free=%u, largest=%u",
            subsystemName(ev.subsystem),
            static_cast<unsigned>(ev.freeHeap),
            static_cast<unsigned>(ev.maxBlock));
    } else {
        Logger::warning(kTag,
            "[AURA][RECOVERY] %s failure, reason=%s, attempt=%u, result=FAIL, free=%u, largest=%u",
            subsystemName(ev.subsystem),
            ev.reason ? ev.reason : "unknown",
            static_cast<unsigned>(ev.attempt),
            static_cast<unsigned>(ev.freeHeap),
            static_cast<unsigned>(ev.maxBlock));
    }
}