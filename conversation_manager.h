#ifndef AURA_CONVERSATION_MANAGER_H
#define AURA_CONVERSATION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "sarvam_stt.h"
#include "gemini_client.h"
#include "sarvam_tts.h"
#include "display_manager.h"
#include "logger.h"
#include "config.h"
#include "tiny_ai_manager.h"
#include "settings_manager.h"

/**
 * @enum ConversationState
 * @brief Conversation workflow states
 */
enum class ConversationState : uint8_t {
    IDLE,           ///< Waiting for wake word or button
    LISTENING,      ///< SpeechToText recording
    TRANSCRIBING,   ///< SpeechToText processing
    THINKING,       ///< GeminiClient processing
    SPEAKING,       ///< TextToSpeech playing
    PAUSED,         ///< Conversation paused
    COMPLETED,      ///< Turn completed
    ERROR           ///< Error occurred
};

/**
 * @enum ConversationError
 * @brief Conversation error codes
 */
enum class ConversationError : uint8_t {
    NONE,           ///< No error
    STT_ERROR,      ///< Speech-to-text failure
    GEMINI_ERROR,   ///< Gemini API failure
    TTS_ERROR,      ///< Text-to-speech failure
    TIMEOUT,        ///< Conversation timeout
    NETWORK,        ///< Network failure
    UNKNOWN         ///< Unspecified error
};

/**
 * @struct ConversationResult
 * @brief Conversation turn result
 */
struct ConversationResult {
    String userText;           ///< Recognized user speech
    String assistantText;      ///< Generated assistant response
    unsigned long latencyMs;   ///< End-to-end latency
    unsigned long timestamp;   ///< Completion timestamp
    ConversationError error;   ///< Error code if failed

    ConversationResult() noexcept
        : userText(""), assistantText(""), latencyMs(0), timestamp(0), error(ConversationError::NONE) {}

    void clear() noexcept {
        userText.clear();
        assistantText.clear();
        latencyMs = 0;
        timestamp = 0;
        error = ConversationError::NONE;
    }
};

/**
 * @struct HistoryEntry
 * @brief Single conversation history entry
 */
struct HistoryEntry {
    String userText;
    String assistantText;
    unsigned long timestamp;
    unsigned long latencyMs;

    HistoryEntry() noexcept : userText(""), assistantText(""), timestamp(0), latencyMs(0) {}
    HistoryEntry(const String& u, const String& a, unsigned long t, unsigned long l) noexcept
        : userText(u), assistantText(a), timestamp(t), latencyMs(l) {}
};

/**
 * @class ConversationManager
 * @brief Single authority for conversation workflow orchestration
 *
 * Coordinates:
 * - SpeechToText for recognition
 * - GeminiClient for AI responses
 * - TextToSpeech for synthesis
 * - DisplayManager for visual feedback
 *
 * Non-blocking, production-quality conversation manager for ESP32.
 */
class ConversationManager {
public:
    /**
     * @brief Constructor
     */
    ConversationManager() noexcept;

    /**
     * @brief Destructor
     */
    ~ConversationManager() noexcept;

    // Delete copy semantics
    ConversationManager(const ConversationManager&) = delete;
    ConversationManager& operator=(const ConversationManager&) = delete;

    // Delete move semantics
    ConversationManager(ConversationManager&&) = delete;
    ConversationManager& operator=(ConversationManager&&) = delete;

    /**
     * @brief Initialize conversation manager
     * @return true if initialization successful
     * @note Must be called after all submodules are initialized
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Main update loop - process state machine
     * @note Call regularly from loop(), non-blocking
     */
    void run() noexcept;

    /**
     * @brief Alias for run() for scheduler compatibility
     */
    void update() noexcept;

    /**
     * @brief Start new conversation turn
     * @return true if started successfully
     */
    [[nodiscard]] bool startConversation() noexcept;

    /**
     * @brief Stop current conversation
     */
    void stopConversation() noexcept;

    /**
     * @brief Pause conversation
     */
    void pauseConversation() noexcept;

    /**
     * @brief Resume paused conversation
     */
    void resumeConversation() noexcept;

    /**
     * @brief Cancel conversation and return to IDLE
     */
    void cancelConversation() noexcept;

    /**
     * @brief Process wake word detection
     * @note Called by wake word detector
     */
    void processWakeWord() noexcept;

    /**
     * @brief Process button press
     * @note Called by button handler
     */
    void processButtonPress() noexcept;

    /**
     * @brief Enable/disable wake word activation
     * @param enabled True to enable wake word
     */
    void setWakeWordEnabled(bool enabled) noexcept;

    /**
     * @brief Check if wake word is enabled
     * @return true if enabled
     */
    [[nodiscard]] bool isWakeWordEnabled() const noexcept;

    /**
     * @brief Add a wake word phrase
     * @param phrase Wake word string
     * @return true if added
     */
    bool addWakeWordPhrase(const String& phrase) noexcept;

    /**
     * @brief Remove a wake word phrase by index
     * @param index Phrase index
     */
    void removeWakeWordPhrase(size_t index) noexcept;

    /**
     * @brief Get all wake word phrases
     * @return Const reference to phrase vector
     */
    [[nodiscard]] const std::vector<String>& getWakeWordPhrases() const noexcept;

    /**
     * @brief Set wake word detection sensitivity
     * @param sensitivity 0.0 (least) to 1.0 (most sensitive)
     */
    void setWakeWordSensitivity(float sensitivity) noexcept;

    /**
     * @brief Get wake word sensitivity
     * @return Current sensitivity
     */
    [[nodiscard]] float getWakeWordSensitivity() const noexcept;

    /**
     * @brief Set wake word cooldown period
     * @param cooldownMs Cooldown in milliseconds
     */
    void setWakeWordCooldown(unsigned long cooldownMs) noexcept;

    /**
     * @brief Get wake word cooldown period
     * @return Cooldown in milliseconds
     */
    [[nodiscard]] unsigned long getWakeWordCooldown() const noexcept;

    /**
     * @brief Check if a wake word was recently detected
     * @return true if within cooldown period
     */
    [[nodiscard]] bool isWakeWordCooldownActive() const noexcept;

    /**
     * @brief Get wake word detection statistics
     * @return JSON string with stats
     */
    [[nodiscard]] String getWakeWordStatsJson() const noexcept;

    /**
     * @brief Clear wake word statistics
     */
    void resetWakeWordStats() noexcept;

    /**
     * @brief Check if audio energy exceeds signal threshold
     * @return true if signal detected
     */
    [[nodiscard]] bool checkAudioSignal() const noexcept;

    /**
     * @brief Set conversation timeout
     * @param timeoutMs Timeout in milliseconds (0 = no timeout)
     */
    void setConversationTimeout(unsigned long timeoutMs) noexcept;

    /**
     * @brief Enable/disable auto-speak after Gemini response
     * @param enabled True to auto-speak
     */
    void setAutoSpeak(bool enabled) noexcept;

    /**
     * @brief Enable/disable auto-listen after TTS completes
     * @param enabled True to auto-listen
     */
    void setAutoListen(bool enabled) noexcept;

    /**
     * @brief Enable continuous conversation mode
     */
    void enableContinuousConversation() noexcept;

    /**
     * @brief Disable continuous conversation mode
     */
    void disableContinuousConversation() noexcept;

    /**
     * @brief Enable barge-in (interrupt TTS with new speech)
     */
    void enableBargeIn() noexcept;

    /**
     * @brief Disable barge-in
     */
    void disableBargeIn() noexcept;

    // ========================================================================
    // Push-to-Talk API
    // ========================================================================

    /**
     * @brief Check if push-to-talk mode is enabled
     * @return true if push-to-talk is active
     */
    [[nodiscard]] bool isPushToTalkEnabled() const noexcept;

    /**
     * @brief Enable continuous listening mode (mic always on)
     */
    void enableContinuousListening() noexcept;

    /**
     * @brief Disable continuous listening mode, return to push-to-talk
     */
    void disableContinuousListening() noexcept;

    /**
     * @brief Check if continuous listening mode is active
     * @return true if continuous listening is enabled
     */
    [[nodiscard]] bool isContinuousListeningEnabled() const noexcept;

    /**
     * @brief Toggle continuous listening mode on/off
     */
    void toggleContinuousListening() noexcept;

    /**
     * @brief Check if microphone is currently capturing audio
     * @return true if mic is active
     */
    [[nodiscard]] bool isMicActive() const noexcept;

    /**
     * @brief Update touch sensor state (call from run())
     */
    void processTouch() noexcept;

    // ========================================================================
    // Interaction Refinement API
    // ========================================================================

    /** @name Silent Mode */
    /**@{*/
    [[nodiscard]] bool isSilentMode() const noexcept;
    void setSilentMode(bool enabled) noexcept;
    void toggleSilentMode() noexcept;
    /**@}*/

    /** @name Privacy Mode */
    /**@{*/
    [[nodiscard]] bool isPrivacyMode() const noexcept;
    void setPrivacyMode(bool enabled) noexcept;
    void togglePrivacyMode() noexcept;
    /**@}*/

    /** @name Setup Mode */
    /**@{*/
    [[nodiscard]] bool isSetupMode() const noexcept;
    void enterSetupMode() noexcept;
    void exitSetupMode() noexcept;
    /**@}*/

    /** @name Interaction telemetry (companion app) */
    /**@{*/
    [[nodiscard]] const String& getLastWakeSource() const noexcept;
    [[nodiscard]] const String& getLastTouchEvent() const noexcept;
    [[nodiscard]] unsigned long getLastVoiceTimestampMs() const noexcept;
    /**@}*/

    /** @brief Hands-free wake handler, invoked from the VOICE_DETECTED event. */
    void onVoiceDetected() noexcept;

    /** @name Quick Command */
    /**@{*/
    [[nodiscard]] bool isQuickCommandMode() const noexcept;
    void enterQuickCommandMode() noexcept;
    void exitQuickCommandMode() noexcept;
    /**@}*/

    /** @name Notification Queue */
    /**@{*/
    void enqueueNotification(const String& message) noexcept;
    size_t getNotificationCount() const noexcept;
    const String& peekNotification(size_t index) const noexcept;
    void dequeueNotification() noexcept;
    void clearNotifications() noexcept;
    /**@}*/

    /** @name Auto Sleep */
    /**@{*/
    [[nodiscard]] bool isAutoSleepActive() const noexcept;
    void wakeFromSleep() noexcept;
    /**@}*/

    // ========================================================================

    /**
     * @brief Check if conversation is active
     * @return true if not IDLE
     */
    [[nodiscard]] bool isBusy() const noexcept;

    /**
     * @brief Check if currently listening
     * @return true if in LISTENING or TRANSCRIBING state
     */
    [[nodiscard]] bool isListening() const noexcept;

    /**
     * @brief Check if currently speaking
     * @return true if in SPEAKING state
     */
    [[nodiscard]] bool isSpeaking() const noexcept;

    /**
     * @brief Check if module is initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Check if barge-in is enabled
     * @return true if enabled
     */
    [[nodiscard]] bool isBargeInEnabled() const noexcept;

    /**
     * @brief Get current state
     * @return Current ConversationState
     */
    [[nodiscard]] ConversationState getState() const noexcept;

    /**
     * @brief Get last error code
     * @return Current ConversationError
     */
    [[nodiscard]] ConversationError getError() const noexcept;

    /**
     * @brief Get last conversation result
     * @return Const reference to ConversationResult
     */
    [[nodiscard]] const ConversationResult& getResult() const noexcept;

    /**
     * @brief Get conversation history
     * @return Const reference to history vector
     */
    [[nodiscard]] const std::vector<HistoryEntry>& getHistory() const noexcept;

    /**
     * @brief Clear conversation history
     */
    void clearHistory() noexcept;

private:
    // State management
    void changeState(ConversationState newState) noexcept;
    void setError(ConversationError error) noexcept;
    void resetConversation() noexcept;
    void storeHistoryEntry() noexcept;

    // Workflow handlers
    void handleListening() noexcept;
    void handleTranscribing() noexcept;
    void handleThinking() noexcept;
    void handleSpeaking() noexcept;
    void handleCompletion() noexcept;
    void handleTimeout() noexcept;

    // Push-to-Talk helpers
    void startMic() noexcept;
    void stopMic() noexcept;
    void startFollowUp() noexcept;
    void endFollowUp() noexcept;

    // Interaction refinement helpers
void handleTap() noexcept;
void handleDoubleTap() noexcept;
void handleVeryLongPress() noexcept;
void handleRestartHold() noexcept;
    void handleQuickCommand(const String& transcript) noexcept;
    void checkAutoSleep() noexcept;
    void checkContextReminder() noexcept;
    void cancelActiveResponse() noexcept;
    void ensureEventSubscriptions() noexcept;

    // Helpers
    void updateDisplay() noexcept;
    bool checkSubmoduleErrors() noexcept;
    void extractEntitiesFromConversation() noexcept;
    String inferMood(const String& text) const noexcept;

    // Member variables
    bool m_initialized;
    ConversationState m_currentState;
    ConversationState m_previousState;
    ConversationError m_lastError;
    ConversationResult m_result;

    // Wake word configuration
    bool m_wakeWordEnabled;
    std::vector<String> m_wakeWordPhrases;
    float m_wakeWordSensitivity;
    unsigned long m_wakeWordCooldownMs;
    unsigned long m_lastWakeWordTime;
    unsigned long m_lastNoiseFloorUpdate;

    // Wake word statistics
    uint32_t m_wakeWordTotalDetections;
    uint32_t m_wakeWordFalsePositives;
    uint32_t m_wakeWordIgnoredCooldown;
    float m_wakeWordAvgConfidence;

    // Push-to-Talk state
    bool m_pushToTalkEnabled;
    bool m_continuousListeningEnabled;
    bool m_micActive;
    bool m_followUpActive;
    unsigned long m_followUpStart;
    bool m_endingByTap;
    bool m_settingsRestored;

    // Interaction refinement modes
    bool m_silentMode;
    bool m_privacyMode;
    bool m_quickCommandMode;
    bool m_setupMode;

    // Voice-triggered wake / telemetry
    bool m_eventSubscribed;
    unsigned long m_lastVoiceStartTime;
    String m_lastWakeSource;
    String m_lastTouchEvent;

    // Touch gesture handling — exactly four gestures, spec-driven:
    // single tap / double tap / 5-second setup hold / 15-second restart hold.
    bool m_touchActive;
    bool m_touchLastRaw;
    unsigned long m_touchDebounceStart;
    unsigned long m_touchPressStart;
    unsigned long m_lastTouchPollTime;
    unsigned long m_lastTapTime;
    bool m_doubleTapPending;
    unsigned long m_doubleTapStart;

    // Touch diagnostics (dev-only, rate-limited; compiled out entirely when
    // TOUCH_DIAGNOSTICS_ENABLED == 0).
    bool m_touchRaw;
    bool m_touchDebounced;
    unsigned long m_touchTransitionCount;
    unsigned long m_touchDiagLastLogMs;
    unsigned long m_touchLastGestureEndMs;

    // Set the moment a continuous hold passes SETUP_HOLD_MS so the release
    // never fires a tap/double-tap as well. Guards enterSetupMode() from
    // being triggered twice (the very-long-press toggle trap).
    bool m_setupHoldTriggered;

    // Set the moment a continuous hold passes RESTART_HOLD_MS so the release
    // never fires a tap/double-tap and a finger kept down after the restart
    // can never re-trigger it within the same boot. Cleared on press/release.
    bool m_restartHoldTriggered;

    // Notification queue
    std::vector<String> m_notificationQueue;
    static constexpr size_t kMaxNotifications = 8;

    // Auto-sleep / idle tracking
    unsigned long m_lastActivityTime;
    unsigned long m_lastPeriodicCheckTime;
    bool m_autoSleepActive;

    // Context reminder
    unsigned long m_lastConversationEndTime;
    bool m_contextReminderShown;

    bool m_autoSpeak;
    bool m_autoListen;
    bool m_continuousConversation;
    bool m_bargeInEnabled;
    unsigned long m_conversationTimeoutMs;

    // Timing
    unsigned long m_stateStartTime;
    unsigned long m_conversationStartTime;

    // Submodule interaction flags
    bool m_sttTriggered;
    bool m_geminiTriggered;
    bool m_ttsTriggered;

    // Conversation history
    std::vector<HistoryEntry> m_history;
    static constexpr size_t kMaxHistoryEntries = 10;

    // Constants
    static constexpr unsigned long kDefaultConversationTimeoutMs = 60000UL;
    static constexpr unsigned long kStateTimeoutMs = 30000UL;
    static constexpr unsigned long kFollowUpDurationMs = 10000UL;
    static constexpr unsigned long kVoiceReTriggerDebounceMs = 800UL;
    static constexpr unsigned long kAutoSleepCheckIntervalMs = 10000UL;
    static constexpr unsigned long kContextReminderIdleMs = 600000UL; // 10 min
};

/**
 * @brief Global conversation manager instance
 */
extern ConversationManager conversationManager;

#endif // AURA_CONVERSATION_MANAGER_H