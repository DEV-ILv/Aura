#include "conversation_manager.h"
#include "personality_manager.h"
#include "led_ring.h"
#include "aura_system.h"
#include "sound_manager.h"
#include "context_manager.h"
#include "memory_manager.h"
#include "decision_manager.h"
#include "reminder_manager.h"
#include "learning_manager.h"
#include "executive_assistant.h"
#include "prediction_manager.h"
#include "workspace_manager.h"
#include "event_bus.h"
#include "study_manager.h"
#include "assistant_output.h"
#include "wifi_manager.h"
#include "error_manager.h"

ConversationManager conversationManager;

namespace {
// Forward hardware voice-activity alerts so the conversation manager can start
// a conversation hands-free, the moment the user begins speaking.
void ForwardVoiceDetectedEvent(const Event& e) {
    if (e.type == EventType::VOICE_DETECTED) {
        conversationManager.onVoiceDetected();
    }
}
} // namespace

ConversationManager::ConversationManager() noexcept
    : m_initialized(false)
    , m_currentState(ConversationState::IDLE)
    , m_previousState(ConversationState::IDLE)
    , m_lastError(ConversationError::NONE)
    , m_result()
    , m_wakeWordEnabled(true)
    , m_wakeWordSensitivity(WAKE_WORD_SENSITIVITY_DEFAULT)
    , m_wakeWordCooldownMs(WAKE_WORD_COOLDOWN_DEFAULT)
    , m_lastWakeWordTime(0)
    , m_lastNoiseFloorUpdate(0)
    , m_wakeWordTotalDetections(0)
    , m_wakeWordFalsePositives(0)
    , m_wakeWordIgnoredCooldown(0)
    , m_wakeWordAvgConfidence(0.0f)
    , m_autoSpeak(true)
    , m_autoListen(true)
    , m_continuousConversation(false)
    , m_bargeInEnabled(true)
    , m_conversationTimeoutMs(kDefaultConversationTimeoutMs)
    , m_pushToTalkEnabled(true)
    , m_continuousListeningEnabled(false)
    , m_micActive(false)
    , m_followUpActive(false)
    , m_followUpStart(0)
    , m_endingByTap(false)
    , m_settingsRestored(false)
    , m_silentMode(false)
    , m_privacyMode(false)
    , m_quickCommandMode(false)
    , m_setupMode(false)
    , m_eventSubscribed(false)
    , m_lastVoiceStartTime(0)
    , m_lastWakeSource("boot")
    , m_lastTouchEvent("none")
    , m_touchActive(false)
    , m_touchLastRaw(false)
    , m_touchDebounceStart(0)
    , m_touchPressStart(0)
    , m_lastTouchPollTime(0)
    , m_lastTapTime(0)
    , m_doubleTapPending(false)
    , m_doubleTapStart(0)
    , m_touchRaw(false)
    , m_touchDebounced(false)
    , m_touchTransitionCount(0)
    , m_touchDiagLastLogMs(0)
    , m_touchLastGestureEndMs(0)
    , m_setupHoldTriggered(false)
    , m_lastActivityTime(0)
    , m_lastPeriodicCheckTime(0)
    , m_autoSleepActive(false)
    , m_lastConversationEndTime(0)
    , m_contextReminderShown(false)
    , m_stateStartTime(0)
    , m_conversationStartTime(0)
    , m_sttTriggered(false)
    , m_geminiTriggered(false)
    , m_ttsTriggered(false) {
}

ConversationManager::~ConversationManager() noexcept = default;

bool ConversationManager::initialize() noexcept {
    if (m_initialized) return true;

    if (!speechToText.isInitialized()) {
        // Degraded mode: without STT the conversation engine still provides the
        // touch-controlled mic (single/double tap, 5s setup hold) and status
        // feedback. Voice-to-text only requires a re-initialized STT provider.
        LOG_WARN("ConversationManager", "SpeechToText unavailable - continuing in touch/mic-only mode");
    }

    if (!geminiClient.isInitialized()) {
        LOG_WARN("ConversationManager", "GeminiClient not initialized - offline mode only");
    }

    if (!textToSpeech.isInitialized()) {
        // Degraded mode: without TTS the conversation engine keeps its touch
        // state machine, mic, OLED status and LED ring fully functional; only
        // the spoken reply is skipped. Speech output is re-enabled once a TTS
        // provider becomes available again.
        LOG_WARN("ConversationManager", "TextToSpeech unavailable - text/reply mode only");
    }

    if (!displayManager.isInitialized()) {
        LOG_ERROR("ConversationManager", "DisplayManager not initialized");
        return false;
    }

    // Push-to-talk default: wake word off, mic stopped
    m_wakeWordEnabled = false;
    m_micActive = false;
    if (audioManager.isInitialized()) {
        audioManager.stopRecording();
    }
    if (speechToText.isInitialized()) {
        speechToText.stopRecognition();
    }

    // Voice-triggered wake: enable hardware VAD only when the wake-word engine
    // is actually on. In push-to-talk mode (wake word off) the mic stays off
    // until a touch gesture opens it, so no VOICE_DETECTED chatter can fire on
    // ambient noise / a floating mic.
    if (m_wakeWordEnabled && audioManager.isInitialized()) {
        audioManager.setVoiceActivityMonitoring(true);
    }

    // Default OLED state labels (personality may override these at runtime)
    displayManager.setStateLabel(DisplayState::LISTENING, "Listening...");
    displayManager.setStateLabel(DisplayState::THINKING, "Thinking...");
    displayManager.setStateLabel(DisplayState::SPEAKING, "Speaking...");
    displayManager.setStateLabel(DisplayState::HOME, "Ready");

    m_initialized = true;
    m_lastError = ConversationError::NONE;
    LOG_INFO("ConversationManager", "Initialized (push-to-talk mode, continuous=%s)",
        m_continuousListeningEnabled ? "yes" : "no");
    return true;
}

void ConversationManager::run() noexcept {
    if (!m_initialized) return;

    ensureEventSubscriptions();

    // Deferred settings restore (SettingsManager may init after us)
    if (!m_settingsRestored && settingsManager.isInitialized()) {
        m_settingsRestored = true;
        m_continuousListeningEnabled = settingsManager.getContinuousListeningEnabled();
        if (m_continuousListeningEnabled) {
            m_wakeWordEnabled = true;
            startMic();
            displayManager.setMicMuted(false);
        } else {
            displayManager.setMicMuted(true);
        }
    }

    handleTimeout();

    unsigned long now = millis();

    // Track activity time on any state change from idle
    if (m_currentState != ConversationState::IDLE) {
        m_lastActivityTime = now;
    }

    // Double-tap pending timeout (first tap received, waiting for second).
    // Skipped while a finger is still down so a 5-second setup hold can take
    // priority over the pending single tap.
    if (m_doubleTapPending && !m_touchActive && (now - m_doubleTapStart > DOUBLE_TAP_WINDOW_MS)) {
        m_doubleTapPending = false;
        handleTap();
    }

    // Touch sensor polling (non-blocking, 20ms interval)
    if (now - m_lastTouchPollTime >= TOUCH_POLL_INTERVAL_MS) {
        m_lastTouchPollTime = now;
        processTouch();
    }

    // Quick command: after completion, auto-exit
    if (m_quickCommandMode && m_currentState == ConversationState::IDLE) {
        exitQuickCommandMode();
    }

    // Periodic checks (every ~10s)
    if (now - m_lastPeriodicCheckTime >= kAutoSleepCheckIntervalMs) {
        m_lastPeriodicCheckTime = now;
        checkAutoSleep();
        checkContextReminder();
    }

    // Publish notification queue count on home display
    if (m_currentState == ConversationState::IDLE && !m_notificationQueue.empty()) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned int>(m_notificationQueue.size()));
        displayManager.setStateLabel(DisplayState::HOME, buf);
    }

    switch (m_currentState) {
        case ConversationState::LISTENING:
            handleListening();
            break;
        case ConversationState::TRANSCRIBING:
            handleTranscribing();
            break;
        case ConversationState::THINKING:
            handleThinking();
            break;
        case ConversationState::SPEAKING:
            handleSpeaking();
            break;
        case ConversationState::COMPLETED:
            handleCompletion();
            break;
        case ConversationState::ERROR:
            if (!checkSubmoduleErrors()) {
                changeState(ConversationState::IDLE);
            }
            break;
        case ConversationState::IDLE: {
            // Adaptive noise floor: update every 5 seconds during idle
            unsigned long now = millis();
            if (now - m_lastNoiseFloorUpdate > 5000) {
                float rms = audioManager.getAudioEnergy();
                if (rms > 0.0f) {
                    audioManager.updateAdaptiveNoiseFloor(static_cast<uint16_t>(rms));
                }
                m_lastNoiseFloorUpdate = now;
            }
            break;
        }
        case ConversationState::PAUSED:
        default:
            break;
    }

    updateDisplay();
}

void ConversationManager::update() noexcept {
    run();
}

bool ConversationManager::startConversation() noexcept {
    if (!m_initialized) {
        setError(ConversationError::UNKNOWN);
        return false;
    }
    if (m_privacyMode) {
        LOG_WARN("ConversationManager", "Cannot start conversation in privacy mode");
        displayManager.showMessage("PRIVACY MODE", "Mic disabled");
        return false;
    }
    if (m_setupMode) {
        LOG_WARN("ConversationManager", "Cannot start conversation in setup mode");
        return false;
    }
    if (m_currentState != ConversationState::IDLE && m_currentState != ConversationState::COMPLETED && m_currentState != ConversationState::ERROR) {
        LOG_WARN("ConversationManager", "Cannot start, busy in state %d", static_cast<int>(m_currentState));
        return false;
    }

    resetConversation();
    m_conversationStartTime = millis();
    // Apply personality LED theme and display labels
    if (personalityManager.isInitialized()) {
        const auto& profile = personalityManager.getActiveProfile();
        uint32_t theme = profile.ledTheme;
        ledRing.setThemeColor(CRGB(
            static_cast<uint8_t>((theme >> 16) & 0xFF),
            static_cast<uint8_t>((theme >> 8) & 0xFF),
            static_cast<uint8_t>(theme & 0xFF)));
        // Set personality-aware display labels based on response style
        const String& style = profile.responseStyle;
        if (style == "formal-friendly") {
            displayManager.setStateLabel(DisplayState::LISTENING, "AT YOUR SERVICE");
            displayManager.setStateLabel(DisplayState::THINKING, "PROCESSING");
            displayManager.setStateLabel(DisplayState::SPEAKING, "AT WORK");
        } else if (style == "professional") {
            displayManager.setStateLabel(DisplayState::LISTENING, "AWAITING INPUT");
            displayManager.setStateLabel(DisplayState::THINKING, "ANALYZING");
            displayManager.setStateLabel(DisplayState::SPEAKING, "RESPONDING");
        } else if (style == "educational") {
            displayManager.setStateLabel(DisplayState::LISTENING, "READY TO HELP");
            displayManager.setStateLabel(DisplayState::THINKING, "THINKING");
            displayManager.setStateLabel(DisplayState::SPEAKING, "EXPLAINING");
        } else if (style == "technical") {
            displayManager.setStateLabel(DisplayState::LISTENING, "LISTENING");
            displayManager.setStateLabel(DisplayState::THINKING, "COMPILING");
            displayManager.setStateLabel(DisplayState::SPEAKING, "OUTPUTTING");
        } else if (style == "casual-warm") {
            displayManager.setStateLabel(DisplayState::LISTENING, "LISTENING UP!");
            displayManager.setStateLabel(DisplayState::THINKING, "HMM...");
            displayManager.setStateLabel(DisplayState::SPEAKING, "HERE YOU GO!");
        } else if (style == "minimal") {
            displayManager.setStateLabel(DisplayState::LISTENING, "LISTENING");
            displayManager.setStateLabel(DisplayState::THINKING, "...");
            displayManager.setStateLabel(DisplayState::SPEAKING, "");
        }
    }
    changeState(ConversationState::LISTENING);
    LOG_INFO("ConversationManager", "Conversation started");
    return true;
}

void ConversationManager::stopConversation() noexcept {
    if (m_currentState == ConversationState::IDLE) return;

    speechToText.cancelRecognition();
    geminiClient.cancelRequest();
    textToSpeech.stop();

    resetConversation();
    changeState(ConversationState::IDLE);
    LOG_INFO("ConversationManager", "Conversation stopped");
}

void ConversationManager::pauseConversation() noexcept {
    if (m_currentState == ConversationState::SPEAKING) {
        textToSpeech.pause();
        m_previousState = ConversationState::SPEAKING;
        changeState(ConversationState::PAUSED);
    } else if (m_currentState == ConversationState::LISTENING || m_currentState == ConversationState::TRANSCRIBING) {
        speechToText.cancelRecognition();
        m_previousState = m_currentState;
        changeState(ConversationState::PAUSED);
    } else if (m_currentState == ConversationState::THINKING) {
        geminiClient.cancelRequest();
        m_previousState = ConversationState::THINKING;
        changeState(ConversationState::PAUSED);
    }
}

void ConversationManager::resumeConversation() noexcept {
    if (m_currentState != ConversationState::PAUSED) return;

    switch (m_previousState) {
        case ConversationState::LISTENING:
        case ConversationState::TRANSCRIBING:
            changeState(ConversationState::LISTENING);
            break;
        case ConversationState::THINKING:
            changeState(ConversationState::THINKING);
            break;
        case ConversationState::SPEAKING:
            textToSpeech.resume();
            changeState(ConversationState::SPEAKING);
            break;
        default:
            changeState(ConversationState::IDLE);
            break;
    }
}

void ConversationManager::cancelConversation() noexcept {
    stopConversation();
}

void ConversationManager::processWakeWord() noexcept {
    if (!m_wakeWordEnabled) return;

    // Check cooldown
    unsigned long now = millis();
    if (now - m_lastWakeWordTime < m_wakeWordCooldownMs) {
        m_wakeWordIgnoredCooldown++;
        return;
    }

    // Check audio signal threshold (pre-filter)
    if (!checkAudioSignal()) {
        m_wakeWordFalsePositives++;
        return;
    }

    if (m_currentState == ConversationState::IDLE || m_currentState == ConversationState::COMPLETED) {
        m_lastWakeWordTime = now;
        m_wakeWordTotalDetections++;
        float energy = audioManager.getAudioEnergy();
        float peak = audioManager.getAudioPeak();
        m_wakeWordAvgConfidence = (m_wakeWordAvgConfidence * static_cast<float>(m_wakeWordTotalDetections - 1)
            + (energy > 0 ? (peak / 32768.0f) : 0.0f)) / static_cast<float>(m_wakeWordTotalDetections);

        // Publish wake word event
        if (eventBus.isInitialized()) {
            String data = "{\"confidence\":" + String(m_wakeWordAvgConfidence, 3)
                + ",\"energy\":" + String(energy, 1)
                + ",\"peak\":" + String(peak, 1) + "}";
            eventBus.publish(EventType::WAKE_WORD_DETECTED, "ConversationManager", data);
        }

        startConversation();
    }
}

void ConversationManager::setWakeWordEnabled(bool enabled) noexcept {
    m_wakeWordEnabled = enabled;
}

bool ConversationManager::isWakeWordEnabled() const noexcept {
    return m_wakeWordEnabled;
}

bool ConversationManager::addWakeWordPhrase(const String& phrase) noexcept {
    if (phrase.isEmpty() || phrase.length() > WAKE_WORD_PHRASE_MAX_LEN) return false;
    if (m_wakeWordPhrases.size() >= WAKE_WORD_PHRASES_MAX) return false;
    for (const auto& p : m_wakeWordPhrases) {
        if (p.equalsIgnoreCase(phrase)) return false;
    }
    m_wakeWordPhrases.push_back(phrase);
    return true;
}

void ConversationManager::removeWakeWordPhrase(size_t index) noexcept {
    if (index < m_wakeWordPhrases.size()) {
        m_wakeWordPhrases.erase(m_wakeWordPhrases.begin() + static_cast<ptrdiff_t>(index));
    }
}

const std::vector<String>& ConversationManager::getWakeWordPhrases() const noexcept {
    return m_wakeWordPhrases;
}

void ConversationManager::setWakeWordSensitivity(float sensitivity) noexcept {
    m_wakeWordSensitivity = constrain(sensitivity, WAKE_WORD_SENSITIVITY_MIN, WAKE_WORD_SENSITIVITY_MAX);
}

float ConversationManager::getWakeWordSensitivity() const noexcept {
    return m_wakeWordSensitivity;
}

void ConversationManager::setWakeWordCooldown(unsigned long cooldownMs) noexcept {
    m_wakeWordCooldownMs = cooldownMs;
}

unsigned long ConversationManager::getWakeWordCooldown() const noexcept {
    return m_wakeWordCooldownMs;
}

bool ConversationManager::isWakeWordCooldownActive() const noexcept {
    return (millis() - m_lastWakeWordTime) < m_wakeWordCooldownMs;
}

String ConversationManager::getWakeWordStatsJson() const noexcept {
    String json = "{\"enabled\":" + String(m_wakeWordEnabled ? "true" : "false");
    json += ",\"phrases\":[";
    for (size_t i = 0; i < m_wakeWordPhrases.size(); ++i) {
        if (i > 0) json += ",";
        json += "\"" + m_wakeWordPhrases[i] + "\"";
    }
    json += "],\"sensitivity\":" + String(m_wakeWordSensitivity, 2);
    json += ",\"cooldownMs\":" + String(m_wakeWordCooldownMs);
    json += ",\"totalDetections\":" + String(m_wakeWordTotalDetections);
    json += ",\"falsePositives\":" + String(m_wakeWordFalsePositives);
    json += ",\"ignoredCooldown\":" + String(m_wakeWordIgnoredCooldown);
    json += ",\"avgConfidence\":" + String(m_wakeWordAvgConfidence, 3);
    json += ",\"cooldownActive\":" + String(isWakeWordCooldownActive() ? "true" : "false");
    json += ",\"audioEnergy\":" + String(audioManager.getAudioEnergy(), 1);
    json += ",\"audioPeak\":" + String(audioManager.getAudioPeak(), 1);
    json += ",\"noiseFloor\":" + String(audioManager.getNoiseFloor());
    json += ",\"noiseThreshold\":" + String(audioManager.getNoiseThreshold());
    json += "}";
    return json;
}

void ConversationManager::resetWakeWordStats() noexcept {
    m_wakeWordTotalDetections = 0;
    m_wakeWordFalsePositives = 0;
    m_wakeWordIgnoredCooldown = 0;
    m_wakeWordAvgConfidence = 0.0f;
}

bool ConversationManager::checkAudioSignal() const noexcept {
    if (!audioManager.isInitialized()) return true;
    float energy = audioManager.getAudioEnergy();
    uint16_t threshold = audioManager.getNoiseFloor() + audioManager.getNoiseThreshold();
    return energy > static_cast<float>(threshold);
}

// ============================================================================
// Touch Gesture System — exactly THREE gestures, spec-driven:
//   * SINGLE TAP   -> microphone ON, listening (confirmed only after the
//                     double-tap window expires, so it can never become a
//                     single+double, or vice-versa).
//   * DOUBLE TAP   -> microphone OFF, cancel active voice interaction, IDLE.
//   * 5s HOLD      -> AURA SETUP mode (highest priority, never a tap).
//
// Priority: 5s hold > double tap > single tap. Non-blocking: everything is
// millis()-based and this runs from the main loop (never delay()).
// ============================================================================

void ConversationManager::processTouch() noexcept {
    // TTP223 touch sensor (digital, active-high)
    const bool touched = (digitalRead(TOUCH_PIN) == HIGH);
    const unsigned long now = millis();

#if TOUCH_DIAGNOSTICS_ENABLED
    m_touchRaw = touched;
#endif

    // --- Edge-based debounce ------------------------------------------------
    // Every raw transition restarts the debounce timer. The stable, debounced
    // state is only re-evaluated after TOUCH_DEBOUNCE_MS without a change.
    if (touched != m_touchLastRaw) {
        m_touchDebounceStart = now;
        m_touchLastRaw = touched;
#if TOUCH_DIAGNOSTICS_ENABLED
        m_touchTransitionCount++;
#endif
        return;
    }
    if (now - m_touchDebounceStart < TOUCH_DEBOUNCE_MS) return;

#if TOUCH_DIAGNOSTICS_ENABLED
    m_touchDebounced = touched;
#endif

    if (touched && !m_touchActive) {
        // --- Press: begin tracking a new gesture --------------------------
        // False-trigger guard: a press arriving very soon (< TOUCH_GESTURE_GAP_MS)
        // after a finalized gesture is hover/bounce re-trigger, not a deliberate
        // tap. A pending double-tap second tap is ALWAYS allowed through so the
        // double-tap timing defined in config.h is never altered.
        const bool pendingSecondTap =
            m_doubleTapPending && (now - m_doubleTapStart <= DOUBLE_TAP_WINDOW_MS);
        if (!pendingSecondTap && (now - m_touchLastGestureEndMs) < TOUCH_GESTURE_GAP_MS) {
            return;
        }
        m_touchActive = true;
        m_touchPressStart = now;
        m_setupHoldTriggered = false;
#if TOUCH_DIAGNOSTICS_ENABLED
        LOG_INFO("TouchDiag", "TOUCH_DOWN raw=%d trans=%lu press_at=%lu",
                 static_cast<int>(m_touchRaw),
                 static_cast<unsigned long>(m_touchTransitionCount),
                 static_cast<unsigned long>(now));
#endif
    } else if (touched && m_touchActive) {
        // --- Still pressed: arm the 5-second setup hold --------------------
        // Trigger inside the press so the user sees AURA SETUP at exactly
        // SETUP_HOLD_MS without having to release first, and set the guard so
        // the eventual release never dispatches a tap/double-tap afterwards.
        if (!m_setupHoldTriggered && (now - m_touchPressStart >= SETUP_HOLD_MS)) {
            m_setupHoldTriggered = true;
            m_doubleTapPending = false;   // a hold must never become a tap
            if (m_autoSleepActive) {
                wakeFromSleep();          // setup needs a live, lit screen
            }
            LOG_DEBUG("ConversationManager", "SETUP_HOLD 5000ms reached");
            handleVeryLongPress();        // enter AURA SETUP (purple)
        }
    } else if (!touched && m_touchActive) {
        // --- Release -------------------------------------------------------
        m_touchActive = false;
        m_touchLastGestureEndMs = now;
#if TOUCH_DIAGNOSTICS_ENABLED
        LOG_INFO("TouchDiag", "TOUCH_UP raw=%d trans=%lu release_at=%lu hold_ms=%lu",
                 static_cast<int>(m_touchRaw),
                 static_cast<unsigned long>(m_touchTransitionCount),
                 static_cast<unsigned long>(now),
                 static_cast<unsigned long>(now - m_touchPressStart));
#endif

        // A completed 5-second setup hold has already handled the gesture.
        if (m_setupHoldTriggered) {
            m_setupHoldTriggered = false;
            return;
        }

        // Touch wakes AURA from auto-sleep; nothing else happens.
        if (m_autoSleepActive) {
            wakeFromSleep();
            return;
        }

        const unsigned long holdMs = now - m_touchPressStart;

        // Sub-debounce bounce / accidental brushing: ignore.
        if (holdMs < TAP_MIN_MS) return;

        if (holdMs < TAP_MAX_MS) {
            // Quick tap: single or double tap.
            if (m_doubleTapPending &&
                (now - m_doubleTapStart <= DOUBLE_TAP_WINDOW_MS)) {
                m_doubleTapPending = false;
                LOG_DEBUG("ConversationManager", "DOUBLE_TAP");
                handleDoubleTap();          // 2 quick taps — cancel + idle
            } else {
                // First tap: mark pending; run() dispatches handleTap() once
                // the double-tap window expires without a second tap.
                m_doubleTapPending = true;
                LOG_DEBUG("ConversationManager", "SINGLE_TAP pending");
                m_doubleTapStart = now;
            }
            return;
        }

        // A hold shorter than SETUP_HOLD_MS intentionally does nothing:
        // it must never be a tap, a privacy toggle, or a setup trigger.
        LOG_DEBUG("ConversationManager", "HOLD(%lu) ignored", holdMs);
    }

#if TOUCH_DIAGNOSTICS_ENABLED
    // Rate-limited live telemetry (no Serial flood): one line per interval.
    if (now - m_touchDiagLastLogMs >= TOUCH_DIAG_INTERVAL_MS) {
        m_touchDiagLastLogMs = now;
        LOG_INFO("TouchDiag",
                 "TOUCH RAW:%d DEBOUNCED:%d PRESS:%s HOLD_MS:%lu TRANS:%lu",
                 static_cast<int>(m_touchRaw),
                 static_cast<int>(m_touchDebounced),
                 m_touchActive ? "DOWN" : "UP",
                 m_touchActive ? static_cast<unsigned long>(now - m_touchPressStart) : 0UL,
                 static_cast<unsigned long>(m_touchTransitionCount));
    }
#endif
}

// Single-tap button handler
void ConversationManager::processButtonPress() noexcept {
    // Ignore repeated single taps while already in a voice state
    // (LISTENING/RECORDING=TRANSCRIBING/PROCESSING=THINKING/SPEAKING).
    if (m_currentState == ConversationState::LISTENING ||
        m_currentState == ConversationState::TRANSCRIBING ||
        m_currentState == ConversationState::THINKING ||
        m_currentState == ConversationState::SPEAKING) {
        return;
    }

    // Muted/privacy mode: touch never opens the microphone.
    if (m_privacyMode || m_setupMode) return;

    if (m_currentState == ConversationState::IDLE || m_currentState == ConversationState::COMPLETED) {
        m_lastWakeSource = "touch";
        LOG_DEBUG("ConversationManager", "MIC_ON (single tap)");
        if (m_pushToTalkEnabled) startMic();
        startConversation();
    }
}

// ============================================================================
// Gesture Handlers
// ============================================================================

void ConversationManager::handleTap() noexcept {
    m_lastTouchEvent = "single";
#if TOUCH_DIAGNOSTICS_ENABLED
    LOG_INFO("TouchDiag", "GESTURE: SINGLE TAP");
#endif
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::TOUCH_SINGLE, "ConversationManager", "");
    }
    processButtonPress();
}

void ConversationManager::handleDoubleTap() noexcept {
    m_lastTouchEvent = "double";
#if TOUCH_DIAGNOSTICS_ENABLED
    LOG_INFO("TouchDiag", "GESTURE: DOUBLE TAP");
#endif
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::TOUCH_DOUBLE, "ConversationManager", "");
    }
    cancelActiveResponse();
}

void ConversationManager::handleVeryLongPress() noexcept {
    m_lastTouchEvent = "very_long";
#if TOUCH_DIAGNOSTICS_ENABLED
    LOG_INFO("TouchDiag", "GESTURE: SETUP HOLD");
#endif
    enterSetupMode();   // 5s continuous hold — AURA SETUP
}

// ============================================================================
// Response Cancellation
// ============================================================================

void ConversationManager::cancelActiveResponse() noexcept {
    if (audioManager.isInitialized()) {
        audioManager.cancelPlayback();
    }
    textToSpeech.stop();
    speechToText.cancelRecognition();
    geminiClient.cancelRequest();   // clear any queued AI request
    // Stop recording immediately if a capture is in progress (double tap).
    if (m_micActive) {
        stopMic();
    }
    if (m_followUpActive) {
        endFollowUp();
    }
    m_endingByTap = true;
    resetConversation();
    changeState(ConversationState::IDLE);
    m_lastWakeSource = "";
    LOG_INFO("ConversationManager", "Voice interaction cancelled (double tap / setup hold)");
}

// ============================================================================
// Setup Mode
// ============================================================================

bool ConversationManager::isSetupMode() const noexcept {
    return m_setupMode;
}

void ConversationManager::enterSetupMode() noexcept {
    if (m_setupMode) {
        // Already in setup (e.g. a second 5s hold): toggle back out.
        exitSetupMode();
        return;
    }
    m_setupMode = true;
    m_lastWakeSource = "setup";
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::ENTER_SETUP, "ConversationManager", "");
    }

    // Force-stop any active voice interaction so setup owns the hardware:
    // cancel STT/AI/TTS, stop recording, then disable VAD.
    if (m_currentState == ConversationState::LISTENING ||
        m_currentState == ConversationState::TRANSCRIBING ||
        m_currentState == ConversationState::THINKING ||
        m_currentState == ConversationState::SPEAKING) {
        cancelActiveResponse();
    }
    if (m_micActive) {
        stopMic();
    }
    if (audioManager.isInitialized()) {
        audioManager.stopRecording();
        audioManager.setVoiceActivityMonitoring(false);
    }

    auraSystem.setMood(AuraMood::SETUP);             // purple, provisioning

    if (!wifiManager.isAccessPointMode()) {
        wifiManager.startAccessPoint(Secrets::AP_SSID, Secrets::AP_PASSWORD);
    }

    if (displayManager.isInitialized()) {
        displayManager.setMicStatus(MicStatus::MUTED);
        displayManager.setMicMuted(true);
        displayManager.pinStatus();
        // Show the SSID the provisioning (captive) AP actually uses when it
        // comes up, so the holder knows what to connect to.
        displayManager.showMessage("AURA SETUP", WiFi.softAPSSID());
    }
    LOG_INFO("ConversationManager", "Entered setup mode (AP: %s)", Secrets::AP_SSID);
}

void ConversationManager::exitSetupMode() noexcept {
    if (!m_setupMode) return;
    m_setupMode = false;
    if (wifiManager.isAccessPointMode()) {
        wifiManager.stopAccessPoint();
    }
    if (audioManager.isInitialized()) {
        audioManager.setVoiceActivityMonitoring(true);
    }
    auraSystem.enterIdle();
    if (displayManager.isInitialized()) {
        displayManager.setMicMuted(false);
        displayManager.unpinStatus();
        displayManager.showFace();
    }
    LOG_INFO("ConversationManager", "Exited setup mode");
}

// ============================================================================
// Interaction telemetry (companion app)
// ============================================================================

const String& ConversationManager::getLastWakeSource() const noexcept {
    return m_lastWakeSource;
}

const String& ConversationManager::getLastTouchEvent() const noexcept {
    return m_lastTouchEvent;
}

unsigned long ConversationManager::getLastVoiceTimestampMs() const noexcept {
    return m_lastVoiceStartTime;
}

// ============================================================================
// Voice-Triggered Wake
// ============================================================================

void ConversationManager::ensureEventSubscriptions() noexcept {
    if (m_eventSubscribed || !eventBus.isInitialized()) return;
    eventBus.subscribe(EventType::VOICE_DETECTED, ForwardVoiceDetectedEvent,
                       EventPriority::NORMAL_PRIORITY, "ConvManager-VAD");
    m_eventSubscribed = true;
}

void ConversationManager::onVoiceDetected() noexcept {
    if (!m_initialized || m_privacyMode || m_setupMode) return;
    // The microphone must NOT continuously activate conversations. Hands-free
    // start only runs when an explicit wake-word engine is enabled (architecture
    // ready); otherwise the mic stays ready but only records after a touch.
    if (!m_wakeWordEnabled) return;
    // Only hands-free wake from an idle conversation.
    if (m_currentState != ConversationState::IDLE && m_currentState != ConversationState::COMPLETED) {
        return;
    }
    unsigned long now = millis();
    // Debounce: ignore brief re-triggers of the same utterance.
    if (m_lastVoiceStartTime != 0 && (now - m_lastVoiceStartTime) < kVoiceReTriggerDebounceMs) {
        return;
    }
    m_lastVoiceStartTime = now;
    m_lastWakeSource = "voice";
    if (m_pushToTalkEnabled) {
        startMic();
    }
    startConversation();
    LOG_INFO("ConversationManager", "Voice-triggered wake");
}

// ============================================================================
// Silent Mode
// ============================================================================

bool ConversationManager::isSilentMode() const noexcept {
    return m_silentMode;
}

void ConversationManager::setSilentMode(bool enabled) noexcept {
    if (m_silentMode == enabled) return;
    m_silentMode = enabled;
    if (settingsManager.isInitialized()) {
        settingsManager.setSilentMode(enabled);
        settingsManager.save();
    }
    if (eventBus.isInitialized()) {
        String data = "{\"enabled\":" + String(enabled ? "true" : "false") + "}";
        eventBus.publish(enabled ? EventType::SILENT_MODE_ENABLED : EventType::SILENT_MODE_DISABLED,
                         "ConversationManager", data);
    }
    soundManager.playConfirmation();
    LOG_INFO("ConversationManager", "Silent mode %s", enabled ? "enabled" : "disabled");
}

void ConversationManager::toggleSilentMode() noexcept {
    setSilentMode(!m_silentMode);
}

// ============================================================================
// Privacy Mode
// ============================================================================

bool ConversationManager::isPrivacyMode() const noexcept {
    return m_privacyMode;
}

void ConversationManager::setPrivacyMode(bool enabled) noexcept {
    if (m_privacyMode == enabled) return;
    m_privacyMode = enabled;
    if (enabled) {
        m_lastWakeSource = "privacy";
        // Stop any active conversation (recording, TTS, STT) and return to idle.
        if (m_currentState != ConversationState::IDLE &&
            m_currentState != ConversationState::COMPLETED &&
            m_currentState != ConversationState::ERROR) {
            cancelActiveResponse();
        }
        stopMic();
        m_wakeWordEnabled = false;
        // Disable hardware voice-activity detection, recording, and network requests
        if (audioManager.isInitialized()) {
            audioManager.stopRecording();
            audioManager.setVoiceActivityMonitoring(false);
        }
        if (speechToText.isInitialized()) speechToText.stopRecognition();
        auraSystem.privacy();
        displayManager.setMicMuted(true);
        displayManager.pinStatus();
    } else {
        // Re-enable hardware voice-activity detection so voice can wake the device
        if (audioManager.isInitialized()) {
            audioManager.setVoiceActivityMonitoring(true);
        }
        auraSystem.enterIdle();
        displayManager.setMicMuted(false);
        displayManager.unpinStatus();
    }
    if (settingsManager.isInitialized()) {
        settingsManager.setPrivacyMode(enabled);
        settingsManager.save();
    }
    if (eventBus.isInitialized()) {
        String data = "{\"enabled\":" + String(enabled ? "true" : "false") + "}";
        eventBus.publish(enabled ? EventType::PRIVACY_MODE_ENABLED : EventType::PRIVACY_MODE_DISABLED,
                         "ConversationManager", data);
    }
    soundManager.playConfirmation();
    if (enabled) {
        displayManager.showMessage("Microphone Muted", "Unmute via companion app");
    } else {
        displayManager.showFace();
    }
    LOG_INFO("ConversationManager", "Privacy mode %s", enabled ? "enabled" : "disabled");
}

void ConversationManager::togglePrivacyMode() noexcept {
    setPrivacyMode(!m_privacyMode);
}

// ============================================================================
// Quick Command Mode
// ============================================================================

bool ConversationManager::isQuickCommandMode() const noexcept {
    return m_quickCommandMode;
}

void ConversationManager::enterQuickCommandMode() noexcept {
    if (m_quickCommandMode) return;
    m_quickCommandMode = true;
    if (m_pushToTalkEnabled) startMic();
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::QUICK_COMMAND_ACTIVATED, "ConversationManager", "");
    }
    displayManager.setStateLabel(DisplayState::LISTENING, "QUICK CMD");
    LOG_INFO("ConversationManager", "Quick command mode entered");
    // Start listening for a single command
    startConversation();
}

void ConversationManager::exitQuickCommandMode() noexcept {
    if (!m_quickCommandMode) return;
    m_quickCommandMode = false;
    if (m_pushToTalkEnabled) stopMic();
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::QUICK_COMMAND_DEACTIVATED, "ConversationManager", "");
    }
    displayManager.setStateLabel(DisplayState::LISTENING, "");
    LOG_INFO("ConversationManager", "Quick command mode exited");
}

void ConversationManager::handleQuickCommand(const String& transcript) noexcept {
    // Route recognized command — matches are processed by the AI / function router.
    // Quick command mode auto-exits in run() when state goes IDLE.
    LOG_INFO("ConversationManager", "Quick command: %s", transcript.c_str());
}

// ============================================================================
// Notification Queue
// ============================================================================

void ConversationManager::enqueueNotification(const String& message) noexcept {
    if (m_notificationQueue.size() >= kMaxNotifications) {
        m_notificationQueue.erase(m_notificationQueue.begin());
    }
    m_notificationQueue.push_back(message);
    if (eventBus.isInitialized()) {
        String data = "{\"count\":" + String(m_notificationQueue.size()) + ",\"message\":\"" + message + "\"}";
        eventBus.publish(EventType::NOTIFICATION_QUEUED, "ConversationManager", data);
    }
}

size_t ConversationManager::getNotificationCount() const noexcept {
    return m_notificationQueue.size();
}

const String& ConversationManager::peekNotification(size_t index) const noexcept {
    static const String kEmpty;
    if (index >= m_notificationQueue.size()) return kEmpty;
    return m_notificationQueue[index];
}

void ConversationManager::dequeueNotification() noexcept {
    if (!m_notificationQueue.empty()) {
        m_notificationQueue.erase(m_notificationQueue.begin());
    }
}

void ConversationManager::clearNotifications() noexcept {
    m_notificationQueue.clear();
}

// ============================================================================
// Auto Sleep
// ============================================================================

bool ConversationManager::isAutoSleepActive() const noexcept {
    return m_autoSleepActive;
}

void ConversationManager::wakeFromSleep() noexcept {
    if (!m_autoSleepActive) return;
    m_autoSleepActive = false;
    m_lastActivityTime = millis();
    if (displayManager.isInitialized()) {
        displayManager.faceNotify(FaceEvent::WAKE);
        displayManager.wake();
    }
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::AUTO_SLEEP_EXITED, "ConversationManager", "");
    }
    LOG_INFO("ConversationManager", "Woke from auto-sleep");
}

void ConversationManager::checkAutoSleep() noexcept {
    if (m_currentState != ConversationState::IDLE) {
        m_lastActivityTime = millis();
        return;
    }
    unsigned long timeoutMs = (settingsManager.isInitialized()
        ? settingsManager.getAutoSleepTimeoutMin() : 15) * 60000UL;
    if (timeoutMs == 0) return; // disabled
    unsigned long now = millis();
    if (!m_autoSleepActive && (now - m_lastActivityTime >= timeoutMs)) {
        m_autoSleepActive = true;
        if (eventBus.isInitialized()) {
            eventBus.publish(EventType::AUTO_SLEEP_ENTERED, "ConversationManager", "");
        }
        LOG_INFO("ConversationManager", "Auto-sleep activated");
    }
}

// ============================================================================
// Context Reminder
// ============================================================================

void ConversationManager::checkContextReminder() noexcept {
    if (m_currentState != ConversationState::IDLE) return;
    if (m_contextReminderShown) return;
    if (m_lastConversationEndTime == 0) return;
    unsigned long now = millis();
    if (now - m_lastConversationEndTime >= kContextReminderIdleMs) {
        m_contextReminderShown = true;
        if (eventBus.isInitialized()) {
            eventBus.publish(EventType::CONTEXT_REMINDER_TRIGGERED, "ConversationManager", "");
        }
        displayManager.showMessage("Resume conversation?", "Tap to continue");
        LOG_INFO("ConversationManager", "Context reminder triggered");
    }
}

// ============================================================================
// Push-to-Talk Implementation
// ============================================================================

void ConversationManager::startMic() noexcept {
    if (m_micActive) return;
    if (audioManager.isInitialized()) {
        audioManager.startRecording();
    }
    m_micActive = true;
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::START_RECORDING, "ConversationManager", "");
    }
    displayManager.setMicMuted(false);
    LOG_DEBUG("ConversationManager", "Mic started");
}

void ConversationManager::stopMic() noexcept {
    if (!m_micActive) return;
    if (audioManager.isInitialized()) {
        audioManager.stopRecording();
    }
    if (speechToText.isInitialized()) {
        speechToText.stopRecognition();
    }
    m_micActive = false;
    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::STOP_RECORDING, "ConversationManager", "");
    }
    displayManager.setMicMuted(true);
    LOG_DEBUG("ConversationManager", "Mic stopped");
}

void ConversationManager::startFollowUp() noexcept {
    m_followUpActive = true;
    m_followUpStart = millis();
    LOG_DEBUG("ConversationManager", "Follow-up window started (10s)");
}

void ConversationManager::endFollowUp() noexcept {
    m_followUpActive = false;
    m_followUpStart = 0;
    LOG_DEBUG("ConversationManager", "Follow-up window ended");
}

bool ConversationManager::isPushToTalkEnabled() const noexcept {
    return m_pushToTalkEnabled;
}

bool ConversationManager::isContinuousListeningEnabled() const noexcept {
    return m_continuousListeningEnabled;
}

bool ConversationManager::isMicActive() const noexcept {
    return m_micActive;
}

void ConversationManager::enableContinuousListening() noexcept {
    if (m_continuousListeningEnabled) return;

    m_continuousListeningEnabled = true;
    m_wakeWordEnabled = true;
    startMic();
    soundManager.playConfirmation();
    if (settingsManager.isInitialized()) {
        settingsManager.setContinuousListeningEnabled(true);
        settingsManager.save();
    }
    LOG_INFO("ConversationManager", "Continuous listening ENABLED");
}

void ConversationManager::disableContinuousListening() noexcept {
    if (!m_continuousListeningEnabled) return;

    m_continuousListeningEnabled = false;
    m_wakeWordEnabled = false;
    if (m_currentState == ConversationState::IDLE || m_currentState == ConversationState::COMPLETED) {
        stopMic();
    }
    soundManager.playTouch();
    if (settingsManager.isInitialized()) {
        settingsManager.setContinuousListeningEnabled(false);
        settingsManager.save();
    }
    LOG_INFO("ConversationManager", "Continuous listening DISABLED");
}

void ConversationManager::toggleContinuousListening() noexcept {
    if (m_continuousListeningEnabled) {
        disableContinuousListening();
    } else {
        enableContinuousListening();
    }
}

void ConversationManager::setConversationTimeout(unsigned long timeoutMs) noexcept {
    m_conversationTimeoutMs = timeoutMs;
}

void ConversationManager::setAutoSpeak(bool enabled) noexcept {
    m_autoSpeak = enabled;
}

void ConversationManager::setAutoListen(bool enabled) noexcept {
    m_autoListen = enabled;
}

void ConversationManager::enableContinuousConversation() noexcept {
    m_continuousConversation = true;
}

void ConversationManager::disableContinuousConversation() noexcept {
    m_continuousConversation = false;
}

void ConversationManager::enableBargeIn() noexcept {
    m_bargeInEnabled = true;
}

void ConversationManager::disableBargeIn() noexcept {
    m_bargeInEnabled = false;
}

bool ConversationManager::isBusy() const noexcept {
    return m_currentState != ConversationState::IDLE && m_currentState != ConversationState::COMPLETED && m_currentState != ConversationState::ERROR;
}

bool ConversationManager::isListening() const noexcept {
    return m_currentState == ConversationState::LISTENING || m_currentState == ConversationState::TRANSCRIBING;
}

bool ConversationManager::isSpeaking() const noexcept {
    return m_currentState == ConversationState::SPEAKING;
}

bool ConversationManager::isInitialized() const noexcept {
    return m_initialized;
}

bool ConversationManager::isBargeInEnabled() const noexcept {
    return m_bargeInEnabled;
}

ConversationState ConversationManager::getState() const noexcept {
    return m_currentState;
}

ConversationError ConversationManager::getError() const noexcept {
    return m_lastError;
}

const ConversationResult& ConversationManager::getResult() const noexcept {
    return m_result;
}

const std::vector<HistoryEntry>& ConversationManager::getHistory() const noexcept {
    return m_history;
}

void ConversationManager::clearHistory() noexcept {
    m_history.clear();
}

void ConversationManager::changeState(ConversationState newState) noexcept {
    if (m_currentState == newState) return;

    // Forward chain: IDLE -> LISTENING -> TRANSCRIBING -> THINKING -> SPEAKING
    // -> COMPLETED -> IDLE. Cancellation (double tap / mute) may return to
    // IDLE from ANY active state, so column 0 (IDLE) is valid everywhere.
    static constexpr bool validTransition[8][8] = {
        {0,1,0,0,0,0,0,1},  // IDLE -> LISTENING, ERROR
        {1,0,1,0,0,1,0,1},  // LISTENING -> IDLE, TRANSCRIBING, PAUSED, COMPLETED, ERROR
        {1,0,0,1,1,1,1,1},  // TRANSCRIBING -> IDLE, THINKING, SPEAKING, PAUSED, COMPLETED, ERROR
        {1,0,0,0,1,1,1,1},  // THINKING -> IDLE, SPEAKING, PAUSED, COMPLETED, ERROR
        {1,0,0,0,0,1,1,1},  // SPEAKING -> IDLE, PAUSED, COMPLETED, ERROR
        {1,1,0,1,1,0,0,1},  // PAUSED -> IDLE, LISTENING, THINKING, SPEAKING, ERROR
        {1,1,0,0,0,0,0,1},  // COMPLETED -> IDLE, LISTENING, ERROR
        {1,0,0,0,0,0,0,0}   // ERROR -> IDLE
    };

    if (!validTransition[static_cast<uint8_t>(m_currentState)][static_cast<uint8_t>(newState)]) {
        LOG_WARN("ConversationManager", "Invalid transition %d -> %d", static_cast<int>(m_currentState), static_cast<int>(newState));
        return;
    }

    LOG_DEBUG("ConversationManager", "State %d -> %d", static_cast<int>(m_currentState), static_cast<int>(newState));

    // Audio cues on state entry
    if (soundManager.isInitialized()) {
        if (newState == ConversationState::LISTENING) {
            soundManager.playListening();
        } else if (newState == ConversationState::THINKING) {
            soundManager.playThinking();
        } else if (newState == ConversationState::SPEAKING) {
            soundManager.stopThinking();
        } else if (newState == ConversationState::IDLE || newState == ConversationState::COMPLETED) {
            soundManager.stopThinking();
        }
    }

    m_currentState = newState;
    m_stateStartTime = millis();
}

void ConversationManager::setError(ConversationError error) noexcept {
    if (m_lastError == error) return;
    m_lastError = error;
    m_result.error = error;
    LOG_ERROR("ConversationManager", "Error %d", static_cast<int>(error));

    AuraErrorSeverity sev = AuraErrorSeverity::ERROR;
    const char* code = "CONV_UNKNOWN";
    const char* title = "Conversation failed";
    switch (error) {
        case ConversationError::STT_ERROR:
            code = "CONV_STT"; title = "Speech recognition failed"; break;
        case ConversationError::GEMINI_ERROR:
            code = "CONV_GEMINI"; title = "Language model request failed"; break;
        case ConversationError::TTS_ERROR:
            code = "CONV_TTS"; title = "Speech synthesis failed"; break;
        case ConversationError::TIMEOUT:
            code = "CONV_TIMEOUT"; title = "Conversation timed out"; sev = AuraErrorSeverity::WARNING; break;
        case ConversationError::NETWORK:
            code = "CONV_NETWORK"; title = "Network failure during conversation"; sev = AuraErrorSeverity::WARNING; break;
        case ConversationError::UNKNOWN:
        default:
            code = "CONV_UNKNOWN"; title = "Conversation error"; break;
    }
    errorManager.report(sev, "Conversation", code, title, "");
}

void ConversationManager::resetConversation() noexcept {
    m_result.clear();
    m_lastError = ConversationError::NONE;
    m_sttTriggered = false;
    m_geminiTriggered = false;
    m_ttsTriggered = false;
}

void ConversationManager::storeHistoryEntry() noexcept {
    if (m_result.userText.isEmpty() && m_result.assistantText.isEmpty()) return;

    if (m_history.size() >= kMaxHistoryEntries) {
        m_history.erase(m_history.begin());
    }
    m_history.emplace_back(m_result.userText, m_result.assistantText, m_result.timestamp, m_result.latencyMs);
}

void ConversationManager::handleListening() noexcept {
    if (!speechToText.isInitialized()) {
        // No STT backend: keep the mic live so touch still owns the hardware
        // (double tap / 5s hold still stop or switch modes). Nothing to
        // transcribe, so simply stay LISTENING until the gesture ends it.
        return;
    }

    if (!m_sttTriggered) {
        if (speechToText.startRecognition(RecognitionMode::ONESHOT)) {
            m_sttTriggered = true;
            LOG_INFO("ConversationManager", "STT started");
        } else {
            setError(ConversationError::STT_ERROR);
            changeState(ConversationState::ERROR);
        }
        return;
    }

    if (!speechToText.isBusy()) {
        const SpeechResult& sttResult = speechToText.getResult();
        if (sttResult.error != SpeechError::NONE) {
            setError(ConversationError::STT_ERROR);
            changeState(ConversationState::ERROR);
            return;
        }

        m_result.userText = sttResult.transcript;
        m_result.timestamp = millis();
        speechToText.clearResult();
        m_sttTriggered = false;

        if (m_result.userText.isEmpty()) {
            LOG_WARN("ConversationManager", "Empty transcript");
            if (m_followUpActive) {
                if (millis() - m_followUpStart >= kFollowUpDurationMs) {
                    endFollowUp();
                    stopMic();
                    changeState(ConversationState::COMPLETED);
                } else {
                    m_sttTriggered = false;
                }
            } else if (m_continuousConversation) {
                changeState(ConversationState::LISTENING);
            } else {
                changeState(ConversationState::COMPLETED);
            }
            return;
        }

        LOG_INFO("ConversationManager", "User said: %s", m_result.userText.c_str());
        if (m_followUpActive) {
            m_followUpActive = false;
        }
        changeState(ConversationState::TRANSCRIBING);
    }
}

void ConversationManager::handleTranscribing() noexcept {
    if (!m_geminiTriggered) {
        String enrichedPrompt = m_result.userText;
        // Build rich context preamble with system state, context, and memories
        String contextPreamble;
        if (contextManager.isInitialized()) {
            contextPreamble = contextManager.getContextPreamble();
            // Inject context-specific memories
            String ctxMemories = contextManager.getContextMemories();
            if (!ctxMemories.isEmpty()) {
                contextPreamble += " Current context memories:\n" + ctxMemories;
            }
        }
        // Inject relevant memories from long-term storage by semantic search
        if (memoryManager.isInitialized()) {
            String memoryContext;
            auto results = memoryManager.semanticSearch(m_result.userText, 4);
            for (const auto& mem : results) {
                if (!mem.value.isEmpty()) {
                    memoryContext += " I recall: " + mem.key + " is \"" + mem.value + "\"";
                    if (!mem.summary.isEmpty()) memoryContext += " (" + mem.summary + ")";
                    memoryContext += ".";
                }
            }
            if (!memoryContext.isEmpty()) {
                contextPreamble += "[Memories:" + memoryContext + "] ";
            }
        }
        if (!contextPreamble.isEmpty()) {
            enrichedPrompt = "[Context: " + contextPreamble + "] " + m_result.userText;
        }
        // Enrich with V2.1 intelligence context
        String extraCtx;
        if (decisionManager.isInitialized()) {
            auto recent = decisionManager.getRecentDecisions(3);
            if (!recent.empty()) {
                extraCtx += " Recent decisions:";
                for (const auto& d : recent) {
                    extraCtx += " " + d.question + "(" + d.chosenOptionId + ")";
                }
            }
        }
        if (executiveAssistant.isInitialized()) {
            auto active = executiveAssistant.getActiveRecommendations();
            if (!active.empty()) {
                extraCtx += " Active recommendations:";
                for (size_t i = 0; i < active.size() && i < 3; ++i) {
                    extraCtx += " " + active[i].title;
                }
            }
        }
        if (predictionManager.isInitialized()) {
            auto predictions = predictionManager.getActivePredictions(0.5f);
            if (!predictions.empty()) {
                extraCtx += " Predictions:";
                for (size_t i = 0; i < predictions.size() && i < 2; ++i) {
                    extraCtx += " " + predictions[i].targetName + "(" + String(predictions[i].probability * 100, 0) + "%)";
                }
            }
        }
        if (workspaceManager.isInitialized()) {
            auto activeWs = workspaceManager.getActiveWorkspace();
            if (activeWs) {
                extraCtx += " Active workspace: " + activeWs->name;
            }
        }
        // V3.0 - Event Bus status
        if (eventBus.isInitialized()) {
            extraCtx += " Event Bus: " + String(eventBus.pendingCount()) + " pending events;";
        }

        // V3.0 - Study context
        if (studyManager.isInitialized()) {
            auto dueSubjects = studyManager.getDueSubjects();
            if (!dueSubjects.empty()) {
                extraCtx += " Study reminders (" + String(dueSubjects.size()) + " subjects due):";
                for (const auto& s : dueSubjects) {
                    extraCtx += " " + s.name + " (mastery: " + String(s.masteryLevel) + "%)";
                }
            }
            extraCtx += " Total study time: " + String(studyManager.totalStudyMinutes()) + " minutes;";
        }

        if (!extraCtx.isEmpty()) {
            enrichedPrompt = "[Intelligence" + extraCtx + "] " + enrichedPrompt;
        }
        // Inject active personality system prompt with professional assistant instruction
        if (personalityManager.isInitialized()) {
            String sysPrompt = personalityManager.getActiveProfile().systemPrompt;
            sysPrompt += "\n\nYou are AURA, a professional AI personal assistant. Guidelines:\n";
            sysPrompt += "- Be concise, calm, and professional. Never childish or overly verbose.\n";
            sysPrompt += "- Maintain conversation continuity. If the user refers to something you discussed earlier (pronouns like 'it', 'that', 'they'), resolve from context.\n";
            sysPrompt += "- If you are unsure or the user's intent is unclear, ask one brief clarifying question rather than guessing.\n";
            sysPrompt += "- After answering, offer a natural follow-up question or action suggestion when appropriate.\n";
            sysPrompt += "- If the user interrupts or changes topic, adapt immediately without referencing the interruption.\n";
            sysPrompt += "- Use the provided context and memories to give informed, personalized responses.\n";
            sysPrompt += "- Never repeat yourself. Never state the obvious. Never apologize unless you made an error.\n";
            geminiClient.setSystemPrompt(sysPrompt);
        }
        if (geminiClient.isInitialized() && geminiClient.sendPrompt(enrichedPrompt)) {
            m_geminiTriggered = true;
            LOG_INFO("ConversationManager", "Gemini request sent");
        } else {
            // Gemini unavailable or failed - try offline fallback
            if (tinyAIManager.process(enrichedPrompt, m_result.assistantText)) {
                LOG_INFO("ConversationManager", "Offline AI responded");
                AssistantOutput::route(m_result.assistantText);
                if (m_autoSpeak && !m_result.assistantText.isEmpty()) {
                    changeState(ConversationState::SPEAKING);
                } else {
                    changeState(ConversationState::COMPLETED);
                }
            } else {
                setError(ConversationError::GEMINI_ERROR);
                changeState(ConversationState::ERROR);
            }
        }
        return;
    }

    if (!geminiClient.isBusy()) {
        const GeminiResponse& geminiResult = geminiClient.getResponse();
        if (geminiResult.error != GeminiError::NONE) {
            setError(ConversationError::GEMINI_ERROR);
            changeState(ConversationState::ERROR);
            return;
        }

        m_result.assistantText = geminiResult.responseText;
        geminiClient.clearResponse();
        m_geminiTriggered = false;

        LOG_INFO("ConversationManager", "Gemini responded: %s", m_result.assistantText.c_str());

        AssistantOutput::route(m_result.assistantText);

        if (m_autoSpeak && !m_result.assistantText.isEmpty()) {
            changeState(ConversationState::SPEAKING);
        } else {
            changeState(ConversationState::COMPLETED);
        }
    }
}

void ConversationManager::handleThinking() noexcept {
    // Delegates to handleTranscribing() which checks geminiClient.isBusy()
    // and processes the response when ready. TRANSCRIBING and THINKING share
    // the same wait-for-Gemini logic; the state distinction exists for
    // external observers (display, LED ring) to show different UI.
    handleTranscribing();
}

void ConversationManager::handleSpeaking() noexcept {
    if (!textToSpeech.isInitialized()) {
        // TTS unavailable: silently complete the turn. The reply was already
        // delivered to the OLED via AssistantOutput::route().
        LOG_WARN("ConversationManager", "TTS unavailable - skipping speech");
        m_ttsTriggered = false;
        changeState(ConversationState::COMPLETED);
        return;
    }

    const bool speakEnabled = settingsManager.isInitialized()
        ? settingsManager.getOutputToSpeaker() : true;

    if (!m_ttsTriggered) {
        // Skip TTS when the speaker output is disabled (output routing) or
        // silent mode is active. OLED + Companion display is handled earlier
        // by AssistantOutput::route().
        if (m_silentMode || !speakEnabled) {
            LOG_INFO("ConversationManager", "Speaker off — skipping TTS");
            m_ttsTriggered = true;
            return;
        }
        // Set TTS voice from active personality
        if (personalityManager.isInitialized()) {
            const auto& profile = personalityManager.getActiveProfile();
            if (!profile.voice.isEmpty()) {
                textToSpeech.setVoice(profile.voice);
            }
        }
        // Build framed speech with conversational bridge
        String speechText = m_result.assistantText;
        if (personalityManager.isInitialized() && speechText.length() > 15) {
            const String& style = personalityManager.getActiveProfile().responseStyle;
            if (style == "formal-friendly") {
                speechText = "Here's what I found, sir. " + speechText;
            } else if (style == "professional") {
                speechText = "Here are the results. " + speechText;
            } else if (style == "educational") {
                speechText = "Great question! " + speechText;
            } else if (style == "technical") {
                speechText = "Here's what I've got. " + speechText;
            } else if (style == "casual-warm") {
                speechText = "Here you go! " + speechText;
            }
        }
        if (textToSpeech.speak(speechText)) {
            m_ttsTriggered = true;
            LOG_INFO("ConversationManager", "TTS started");
        } else {
            setError(ConversationError::TTS_ERROR);
            changeState(ConversationState::ERROR);
        }
        return;
    }

    // Skip TTS when speaker disabled / silent mode: go directly to completed
    if (m_silentMode || !speakEnabled) {
        m_ttsTriggered = false;
        changeState(ConversationState::COMPLETED);
        return;
    }

    if (!textToSpeech.isBusy()) {
        m_ttsTriggered = false;
        LOG_INFO("ConversationManager", "TTS completed");
        changeState(ConversationState::COMPLETED);
    }
}

void ConversationManager::extractEntitiesFromConversation() noexcept {
    if (!memoryManager.isInitialized()) return;
    if (m_result.userText.isEmpty()) return;

    const String& text = m_result.userText;
    String lower; lower.reserve(text.length());
    for (size_t i = 0; i < text.length(); ++i) {
        lower += static_cast<char>(tolower(text[i]));
    }

    // Extract user name: "my name is X", "I'm X", "call me X"
    int idx;
    idx = lower.indexOf("my name is ");
    if (idx < 0) idx = lower.indexOf("i'm ");
    if (idx < 0) idx = lower.indexOf("call me ");
    if (idx >= 0) {
        String name = text.substring(idx + 1); // skip past "my" / "i" / "call"
        int space2 = name.indexOf(' ', name.indexOf(' ') + 1);
        if (space2 > 0) name = name.substring(0, space2);
        name.trim();
        if (name.length() > 1 && name.length() < 30) {
            memoryManager.remember(MemoryCategory::USER, "user_name", name, 200, true);
        }
    }

    // Extract preferences: "I like X", "I love X", "my favorite X is Y"
    idx = lower.indexOf("i like ");
    if (idx < 0) idx = lower.indexOf("i love ");
    if (idx < 0) idx = lower.indexOf("my favorite ");
    if (idx >= 0) {
        String pref;
        if (lower.indexOf("my favorite ") >= 0) {
            int sep = lower.indexOf(" is ");
            if (sep > 0) {
                pref = text.substring(lower.indexOf("my favorite ") + 12, sep);
                String val = text.substring(sep + 4);
                int end = val.indexOf('.');
                if (end > 0) val = val.substring(0, end);
                pref.trim(); val.trim();
                if (!pref.isEmpty() && !val.isEmpty() && pref.length() < 40) {
                    memoryManager.remember(MemoryCategory::PREFERENCE, "favorite_" + pref, val, 150);
                }
            }
        } else {
            int start = (lower.indexOf("i like ") >= 0) ? lower.indexOf("i like ") + 7 : lower.indexOf("i love ") + 7;
            pref = text.substring(start);
            int end = pref.indexOf('.');
            if (end > 0) pref = pref.substring(0, end);
            pref.trim();
            if (!pref.isEmpty() && pref.length() < 60) {
                memoryManager.remember(MemoryCategory::PREFERENCE, "likes", pref, 120);
            }
        }
    }

    // Extract project: "I am working on X", "my project is X"
    idx = lower.indexOf("i am working on ");
    if (idx < 0) idx = lower.indexOf("i'm working on ");
    if (idx < 0) idx = lower.indexOf("my project is ");
    if (idx >= 0) {
        int start = text.indexOf(' ') + 1; // skip "I" / "My"
        start = text.indexOf(' ', start) + 1; // skip verb
        if (lower.indexOf("my project is ") >= 0) {
            start = lower.indexOf("my project is ") + 14;
        }
        String proj = text.substring(start);
        int end = proj.indexOf('.');
        if (end > 0) proj = proj.substring(0, end);
        proj.trim();
        if (!proj.isEmpty() && proj.length() < 60) {
            memoryManager.remember(MemoryCategory::PROJECT, "project", proj, 150);
        }
    }

    // Extract facts: "remember that X", "I have X", "don't forget X"
    idx = lower.indexOf("remember that ");
    if (idx >= 0) {
        String fact = text.substring(idx + 14);
        int end = fact.indexOf('.');
        if (end > 0) fact = fact.substring(0, end);
        fact.trim();
        if (!fact.isEmpty() && fact.length() < 200) {
            memoryManager.remember(MemoryCategory::FACT, "fact", fact, 100);
        }
    }
}

String ConversationManager::inferMood(const String& text) const noexcept {
    if (text.isEmpty()) return "neutral";
    String lower; lower.reserve(text.length());
    for (size_t i = 0; i < text.length(); ++i) {
        lower += static_cast<char>(tolower(text[i]));
    }

    // Positive indicators
    const char* positiveWords[] = {"great", "good", "awesome", "nice", "love", "happy",
        "thanks", "perfect", "wonderful", "amazing", "fantastic", "excellent", "glad", "best"};
    for (const auto& word : positiveWords) {
        int idx = lower.indexOf(word);
        if (idx >= 0) {
            // Check it's a whole word match
            char prev = (idx > 0) ? lower[idx - 1] : ' ';
            char next = (idx + static_cast<int>(strlen(word)) < static_cast<int>(lower.length()))
                ? lower[idx + strlen(word)] : ' ';
            if (!isAlpha(prev) && !isAlpha(next)) {
                return "positive";
            }
        }
    }

    // Negative indicators
    const char* negativeWords[] = {"bad", "sad", "angry", "frustrated", "terrible",
        "hate", "annoying", "wrong", "horrible", "awful", "upset", "disappointed"};
    for (const auto& word : negativeWords) {
        int idx = lower.indexOf(word);
        if (idx >= 0) {
            char prev = (idx > 0) ? lower[idx - 1] : ' ';
            char next = (idx + static_cast<int>(strlen(word)) < static_cast<int>(lower.length()))
                ? lower[idx + strlen(word)] : ' ';
            if (!isAlpha(prev) && !isAlpha(next)) {
                return "negative";
            }
        }
    }

    return "neutral";
}

void ConversationManager::handleCompletion() noexcept {
    m_result.latencyMs = millis() - m_conversationStartTime;
    m_result.timestamp = millis();
    m_result.error = ConversationError::NONE;

    storeHistoryEntry();

    // Auto-extract entities from user's speech into long-term memory
    extractEntitiesFromConversation();

    // Infer mood from conversation and update context
    if (contextManager.isInitialized() && !m_result.userText.isEmpty()) {
        String mood = inferMood(m_result.userText);
        contextManager.setMood(mood);
    }

    // Write conversation results back to ContextManager for feedback loop
    if (contextManager.isInitialized()) {
        // Increment conversation count
        size_t count = contextManager.getContext().conversationCount + 1;
        contextManager.setConversationCount(count);
        // Store user query as last conversation summary
        contextManager.setLastConversation(m_result.userText);
        // Infer topic from first few words of user input
        if (!m_result.userText.isEmpty()) {
            int spaceIdx = m_result.userText.indexOf(' ', 40);
            String topic = (spaceIdx > 0) ? m_result.userText.substring(0, spaceIdx) : m_result.userText;
            contextManager.setConversationTopic(topic);
        }
    }

    LOG_INFO("ConversationManager", "Turn completed (%d ms)", m_result.latencyMs);

    // Push-to-talk: start follow-up window after a non-empty conversation
    if (m_pushToTalkEnabled && !m_continuousListeningEnabled && !m_result.userText.isEmpty() && !m_endingByTap) {
        m_endingByTap = false;
        startFollowUp();
        resetConversation();
        changeState(ConversationState::LISTENING);
    } else if (m_continuousConversation && m_autoListen) {
        resetConversation();
        changeState(ConversationState::LISTENING);
    } else {
        m_endingByTap = false;
        if (!m_continuousListeningEnabled && m_micActive) {
            stopMic();
        }
        changeState(ConversationState::IDLE);
    }
}

void ConversationManager::handleTimeout() noexcept {
    if (m_conversationTimeoutMs == 0) return;

    unsigned long now = millis();

    if (m_currentState != ConversationState::IDLE && m_currentState != ConversationState::COMPLETED && m_currentState != ConversationState::ERROR && m_currentState != ConversationState::PAUSED) {
        if (now - m_conversationStartTime >= m_conversationTimeoutMs) {
            LOG_WARN("ConversationManager", "Conversation timeout");
            setError(ConversationError::TIMEOUT);
            stopConversation();
            return;
        }
    }

    if (m_currentState != ConversationState::IDLE && m_currentState != ConversationState::COMPLETED && m_currentState != ConversationState::ERROR && m_currentState != ConversationState::PAUSED) {
        if (now - m_stateStartTime >= kStateTimeoutMs) {
            // Degraded backends must not turn a long idle mic wait into a red
            // ERROR. With no STT provider the list is inherently unbounded, so
            // end the turn cleanly (mic off) and return to the idle face.
            if (m_currentState == ConversationState::LISTENING && !speechToText.isInitialized()) {
                LOG_WARN("ConversationManager", "LISTENING idle timeout (no STT backend) - returning to idle");
                if (m_micActive) stopMic();
                resetConversation();
                changeState(ConversationState::IDLE);
                return;
            }
            LOG_WARN("ConversationManager", "State %d timeout", static_cast<int>(m_currentState));
            setError(ConversationError::TIMEOUT);
            changeState(ConversationState::ERROR);
        }
    }
}

void ConversationManager::updateDisplay() noexcept {
    static DisplayState lastDisplayState = DisplayState::HOME;

    DisplayState targetState = DisplayState::HOME;
    bool forceUpdate = false;

    switch (m_currentState) {
        case ConversationState::IDLE:
            targetState = DisplayState::HOME;
            break;
        case ConversationState::LISTENING:
            targetState = DisplayState::LISTENING;
            break;
        case ConversationState::TRANSCRIBING:
        case ConversationState::THINKING:
            targetState = DisplayState::THINKING;
            break;
        case ConversationState::SPEAKING:
            targetState = DisplayState::SPEAKING;
            break;
        case ConversationState::ERROR:
            targetState = DisplayState::ERROR;
            forceUpdate = true;
            break;
        default:
            break;
    }

    // Live mic/voice icon state (updates independently of the base screen).
    MicStatus micStatus;
    switch (m_currentState) {
        case ConversationState::IDLE:
        case ConversationState::COMPLETED:
            micStatus = (m_privacyMode || m_setupMode || audioManager.isMuted())
                        ? MicStatus::MUTED : MicStatus::IDLE;
            break;
        case ConversationState::LISTENING:
            micStatus = MicStatus::LISTENING;
            break;
        case ConversationState::TRANSCRIBING:
            micStatus = MicStatus::RECORDING;
            break;
        case ConversationState::THINKING:
            micStatus = MicStatus::PROCESSING;
            break;
        case ConversationState::SPEAKING:
            micStatus = MicStatus::SPEAKING;
            break;
        case ConversationState::ERROR:
            micStatus = MicStatus::ERROR;
            break;
        default:
            micStatus = MicStatus::IDLE;
            break;
    }
    displayManager.setMicStatus(micStatus);

    if (targetState != lastDisplayState || forceUpdate) {
        // Update aura (face + ring together) in sync with the display state.
        switch (targetState) {
            case DisplayState::HOME: {
                if (m_privacyMode || m_setupMode) {
                    // Muted/setup: keep the red/purple ring and status screen.
                    break;
                }
                String pName = personalityManager.isInitialized()
                    ? personalityManager.getActiveProfile().name.c_str() : "";
                String activity = contextManager.isInitialized()
                    ? contextManager.getContext().currentTopic : "";
                if (activity.isEmpty() && contextManager.isInitialized()) {
                    activity = contextManager.getContext().lastConversation;
                    if (activity.length() > 20) activity = activity.substring(0, 18) + "...";
                }
                size_t convCount = contextManager.isInitialized()
                    ? contextManager.getContext().conversationCount : 0;
                size_t memCount = memoryManager.isInitialized()
                    ? memoryManager.memoryCount() : 0;
                size_t remCount = 0;
                std::vector<Reminder> reminders;
                if (reminderManager.isInitialized()) {
                    remCount = reminderManager.getReminders(reminders);
                }
                displayManager.updateHomeData(pName, activity, convCount, memCount, remCount);
                displayManager.showHome();
                auraSystem.enterIdle();
                break;
            }
            case DisplayState::LISTENING:
                // Touch-triggered listening: solid green ring + "Listening...".
                auraSystem.listen();
                break;
            case DisplayState::THINKING:
                // Recording (mic/STT capture) is cyan; AI processing is yellow.
                if (m_currentState == ConversationState::TRANSCRIBING) {
                    auraSystem.record();
                } else {
                    auraSystem.think();
                }
                break;
            case DisplayState::SPEAKING:
                auraSystem.speak();
                break;
            case DisplayState::ERROR:
                auraSystem.error();
                break;
            default:
                break;
        }
        lastDisplayState = targetState;
    }
}

bool ConversationManager::checkSubmoduleErrors() noexcept {
    if (speechToText.getError() != SpeechError::NONE) return true;
    if (geminiClient.getError() != GeminiError::NONE) return true;
    if (textToSpeech.getError() != TTSError::NONE) return true;
    return false;
}