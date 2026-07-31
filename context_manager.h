#ifndef AURA_CONTEXT_MANAGER_H
#define AURA_CONTEXT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"

/**
 * @struct SystemContext
 * @brief Live system context snapshot
 */
struct SystemContext {
    unsigned long timestamp;     ///< Context capture timestamp
    bool wifiConnected;          ///< WiFi connection state
    int32_t wifiRSSI;            ///< WiFi signal strength
    String localIP;              ///< Device IP address
    uint32_t freeHeap;           ///< Free heap in bytes
    uint32_t totalHeap;          ///< Total heap in bytes
    uint32_t flashUsage;         ///< Flash usage estimate
    size_t memoryCount;          ///< Number of stored memories
    size_t reminderCount;        ///< Number of active reminders
    size_t conversationCount;    ///< Number of conversations today
    String lastConversation;     ///< Last conversation summary
    String systemState;          ///< Current system state string
    String activePersonality;    ///< Active personality profile ID
    uint8_t batteryLevel;        ///< Battery level (0-100, future)
    float cpuFreqMHz;            ///< Current CPU frequency
    bool sdMounted;              ///< SD card mounted

    // User context fields
    String currentActivity;      ///< What the user is doing
    String currentProject;       ///< Active project name
    String currentSubject;       ///< Current subject area
    String currentLocation;      ///< Logical location (e.g. "home", "work")
    String currentTopic;         ///< Active conversation topic
    String currentMood;          ///< Inferred mood
    String currentWorkSession;   ///< Work session id
    String currentDevice;        ///< Device being used
    String currentFocusState;    ///< Focused / Distracted / Idle
    String currentTask;          ///< Current active task
    size_t contextEntryCount;    ///< Number of context history entries

    SystemContext() noexcept
        : timestamp(0), wifiConnected(false), wifiRSSI(0),
          freeHeap(0), totalHeap(0), flashUsage(0),
          memoryCount(0), reminderCount(0), conversationCount(0),
          batteryLevel(0), cpuFreqMHz(0), sdMounted(false),
          contextEntryCount(0) {}
};

enum class AssistantContext : uint8_t {
    GENERAL = 0,
    STUDY,
    PROJECTS,
    CODING,
    RESEARCH,
    MAINTENANCE,
    MEETING,
    COUNT
};

/**
 * @class ContextManager
 * @brief Maintains live system context for AI conversation
 *
 * Provides a snapshot of current device state including
 * time, WiFi, storage, reminders, memory, and system info.
 * ConversationManager can request context via getContext().
 */
class ContextManager {
public:
    ContextManager() noexcept;
    ~ContextManager() noexcept;

    ContextManager(const ContextManager&) = delete;
    ContextManager& operator=(const ContextManager&) = delete;
    ContextManager(ContextManager&&) = delete;
    ContextManager& operator=(ContextManager&&) = delete;

    /**
     * @brief Initialize context manager
     * @return true if initialized
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Update context snapshot
     */
    void update() noexcept;

    /**
     * @brief Get current system context as JSON string
     * @return JSON-formatted context string
     */
    [[nodiscard]] String getContextJson() const noexcept;

    /**
     * @brief Get current system context struct
     * @return Const reference to context
     */
    [[nodiscard]] const SystemContext& getContext() const noexcept;

    /**
     * @brief Refresh context snapshot now
     */
    void refresh() noexcept;

    /**
     * @brief Check if initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    // --- User context API ---

    void setActivity(const String& activity) noexcept;
    void setProject(const String& project) noexcept;
    void setSubject(const String& subject) noexcept;
    void setLocation(const String& location) noexcept;
    void setConversationTopic(const String& topic) noexcept;
    void setMood(const String& mood) noexcept;
    void setWorkSession(const String& session) noexcept;
    void setDevice(const String& device) noexcept;
    void setFocusState(const String& state) noexcept;
    void setTask(const String& task) noexcept;
    void setConversationCount(size_t count) noexcept;
    void setLastConversation(const String& summary) noexcept;

    [[nodiscard]] String getUserContextJson() const noexcept;

    /**
     * @brief Get context as a crafted natural-language preamble for Gemini
     * @return Human-readable context string like "It's 2:30 PM. WiFi is connected..."
     */
    [[nodiscard]] String getContextNaturalLanguage() const noexcept;

    void clearExpired(unsigned long timeoutMs = 3600000UL) noexcept;

    void saveContextHistory() noexcept;
    [[nodiscard]] String getContextHistory() const noexcept;
    [[nodiscard]] String getContextAsJson() const noexcept;

    // --- Context Switching ---

    void setAssistantContext(AssistantContext ctx) noexcept;
    [[nodiscard]] AssistantContext getAssistantContext() const noexcept;
    [[nodiscard]] String getAssistantContextName() const noexcept;
    [[nodiscard]] bool switchToContext(AssistantContext ctx) noexcept;
    [[nodiscard]] AssistantContext suggestContext() noexcept;

    /**
     * @brief Get a natural-language context preamble for Gemini conversation
     * @return Context string including current assistant context
     */
    [[nodiscard]] String getContextPreamble() const noexcept;

    /**
     * @brief Inject relevant memories from the current context into conversation
     * @return Concatenated memory strings relevant to current context
     */
    [[nodiscard]] String getContextMemories() const noexcept;

    /**
     * @brief Count context switches today
     * @return Number of context switches
     */
    [[nodiscard]] uint8_t getContextSwitchCount() const noexcept;

private:
    static constexpr const char* kLogCategory = "ContextManager";
    static constexpr unsigned long kRefreshIntervalMs = 3000UL;
    static constexpr size_t kMaxHistory = 100;

    struct ContextSnapshot {
        unsigned long timestamp;
        String activity;
        String project;
        String subject;
        String topic;
        String mood;
        String task;
    };

    void inferActivity() noexcept;

    bool m_initialized;
    SystemContext m_context;
    unsigned long m_lastRefresh;
    std::vector<ContextSnapshot> m_history;
    AssistantContext m_assistantContext;
    unsigned long m_contextSwitchTime;
    uint8_t m_contextSwitchCount;
    unsigned long m_lastContextSwitchDate;
};

extern ContextManager contextManager;

#endif // AURA_CONTEXT_MANAGER_H
