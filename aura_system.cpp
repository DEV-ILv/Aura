#include "aura_system.h"
#include "display_manager.h"

AuraSystem auraSystem;

namespace {
constexpr const char* kLogCategory = "AuraSystem";
}

void AuraSystem::initialize() noexcept {
    m_initialized = true;
    setMood(AuraMood::BOOT);
    Logger::info(kLogCategory, "Aura system initialized (face + aura synchronised)");
}

void AuraSystem::update() noexcept {
    if (!m_initialized) return;
    ledRing.update();
}

void AuraSystem::setVoiceLevel(const uint8_t level) noexcept {
    auraFace.setAttention(level);
    ledRing.setVoiceLevel(level);
}

void AuraSystem::setOtaProgress(const uint8_t percentage) noexcept {
    ledRing.setOtaProgress(percentage);
}

void AuraSystem::setMood(const AuraMood mood) noexcept {
    if (!m_initialized) return;
    m_mood = mood;
    ledRing.setMood(mood);
    applyFace(mood);
}

void AuraSystem::applyFace(const AuraMood mood) noexcept {
    // Face + aura pairings (see aura_mood.h). Operational moods (REMINDER,
    // OTA, WIFI_*) keep whatever operational screen the caller is showing and
    // only drive the ring; the face updates the next time it is shown.
    switch (mood) {
        case AuraMood::IDLE:
            displayManager.showFace();
            break;
        case AuraMood::LISTENING:
            displayManager.showListening();
            break;
        case AuraMood::RECORDING:
            displayManager.showListening();
            break;
        case AuraMood::THINKING:
        case AuraMood::PROCESSING:
            displayManager.showThinking();
            break;
        case AuraMood::SPEAKING:
            displayManager.showSpeaking();
            break;
        case AuraMood::HAPPY:
        case AuraMood::SUCCESS:
            displayManager.showFaceExpression(FaceExpression::HAPPY);
            break;
        case AuraMood::WARNING:
            displayManager.showFaceExpression(FaceExpression::CONCERNED);
            break;
        case AuraMood::PRIVACY:
        case AuraMood::ERROR:
            displayManager.showFaceExpression(FaceExpression::CONCERNED);
            break;
        case AuraMood::CRITICAL:
            displayManager.showFaceExpression(FaceExpression::CRITICAL);
            break;
        case AuraMood::OFFLINE:
            displayManager.showFaceExpression(FaceExpression::OFFLINE);
            break;
        case AuraMood::SLEEP:
            displayManager.showFaceExpression(FaceExpression::OFFLINE);
            break;
        case AuraMood::WAKE:
            displayManager.showFace();
            displayManager.faceNotify(FaceEvent::WAKE);
            break;
        case AuraMood::WIFI_CONNECTED:
            displayManager.showFace();
            displayManager.faceNotify(FaceEvent::WIFI_CONNECTED);
            break;
        case AuraMood::WIFI_CONNECTING:
            displayManager.showFace();
            break;
        case AuraMood::REMINDER:
        case AuraMood::OTA:
        case AuraMood::BOOT:
        default:
            break;   // ring-only / caller-managed screen
    }
}