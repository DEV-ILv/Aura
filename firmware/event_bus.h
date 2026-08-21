#ifndef AURA_EVENT_BUS_H
#define AURA_EVENT_BUS_H

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"
#include "logger.h"

enum class EventPriority : uint8_t {
    LOW_PRIORITY = 0,
    NORMAL_PRIORITY = 128,
    HIGH_PRIORITY = 192,
    CRITICAL_PRIORITY = 255
};

enum class EventType : uint8_t {
    // System events
    SYSTEM_STARTUP,
    SYSTEM_SHUTDOWN,
    SYSTEM_ERROR,
    SYSTEM_LOW_MEMORY,

    // Memory events
    MEMORY_STORED,
    MEMORY_UPDATED,
    MEMORY_DELETED,
    MEMORY_EVICTED,

    // Conversation events
    CONVERSATION_STARTED,
    CONVERSATION_ENDED,
    MESSAGE_RECEIVED,
    MESSAGE_SENT,

    // Skill events
    SKILL_TRIGGERED,
    SKILL_COMPLETED,
    SKILL_FAILED,

    // Automation events
    AUTOMATION_TRIGGERED,
    AUTOMATION_COMPLETED,

    // Workspace events
    WORKSPACE_CREATED,
    WORKSPACE_UPDATED,
    WORKSPACE_COMPLETED,

    // Wake word events
    WAKE_WORD_DETECTED,
    WAKE_WORD_FALSE_POSITIVE,
    WAKE_WORD_SENSITIVITY_CHANGED,

    // Voice activity detection (VAD) events
    VOICE_DETECTED,          ///< Speech threshold crossed on the passive mic
    VOICE_ENDED,              ///< Speech dropped back below the threshold
    START_RECORDING,          ///< Mic capture began
    STOP_RECORDING,           ///< Mic capture ended
    TOUCH_SINGLE,             ///< Single tap: wake + manual listening
    TOUCH_DOUBLE,             ///< Double tap: cancel AI response + idle
    TOUCH_LONG,               ///< Long press: toggle privacy mode
    ENTER_SETUP,              ///< Very long press: enter provisioning/setup

    // ESP-NOW events
    ESPNOW_NODE_DISCOVERED,
    ESPNOW_NODE_PAIRED,
    ESPNOW_NODE_DISCONNECTED,
    ESPNOW_NODE_HEARTBEAT,
    ESPNOW_MESSAGE_RECEIVED,
    ESPNOW_OTA_REQUEST,
    ESPNOW_OTA_CHUNK,
    ESPNOW_OTA_COMPLETE,

    // WebSocket events
    WS_CLIENT_CONNECTED,
    WS_CLIENT_DISCONNECTED,
    WS_DASHBOARD_UPDATE,

    // Custom events
    USER_DEFINED,
    TIMER_EVENT,

    // Interaction refinement events
    SILENT_MODE_ENABLED,
    SILENT_MODE_DISABLED,
    PRIVACY_MODE_ENABLED,
    PRIVACY_MODE_DISABLED,
    QUICK_COMMAND_ACTIVATED,
    QUICK_COMMAND_DEACTIVATED,
    AUTO_SLEEP_ENTERED,
    AUTO_SLEEP_EXITED,
    NIGHT_MODE_ENTERED,
    NIGHT_MODE_EXITED,
    NOTIFICATION_QUEUED,
    NOTIFICATION_TRIGGERED,
    INPUT_EVENT,
    FOLLOW_UP_STARTED,
    FOLLOW_UP_ENDED,
    LOW_ACTIVITY_ENTERED,
    LOW_ACTIVITY_EXITED,
    IDLE_PERSONALITY_MESSAGE,
    CONTEXT_REMINDER_TRIGGERED,

    // Dashboard events
    DASHBOARD_WIDGET_UPDATED,
    DASHBOARD_SLEEP,
    DASHBOARD_WAKE,

    // UI events
    RENDERER_CHANGED,
    SCREEN_CHANGED,
    SCREEN_POPPED,

    // Device Health events
    HEALTH_HEAP_LOW,
    HEALTH_WIFI_WEAK,
    HEALTH_SD_MISSING,
    HEALTH_FRAGMENTATION_HIGH,
    HEALTH_CPU_HIGH,
    HEALTH_TEMP_HIGH,
    HEALTH_FLASH_LOW,
    HEALTH_STATUS,

    // Smart Search events
    SEARCH_STARTED,
    SEARCH_COMPLETED,
    SEARCH_FAILED,

    // Knowledge Base events
    KNOWLEDGE_FACT_EXTRACTED,
    KNOWLEDGE_FACT_UPDATED,
    KNOWLEDGE_DUPLICATE_DETECTED,
    KNOWLEDGE_RELATIONSHIP_DISCOVERED,

    // Context Switch events
    CONTEXT_SWITCHED,
    CONTEXT_SUGGESTED,

    // Multi-Device events
    DEVICE_DISCOVERED,
    DEVICE_PAIRED,
    DEVICE_UNPAIRED,
    DEVICE_HEARTBEAT,
    DEVICE_SHARED_REMINDER,
    DEVICE_SHARED_MEMORY,
    DEVICE_HANDOVER_STARTED,
    DEVICE_HANDOVER_COMPLETED,

    // Analytics events
    ANALYTICS_RECORD_ADDED,
    ANALYTICS_TREND_DETECTED,
    ANALYTICS_MILESTONE_REACHED,

    // Study events
    STUDY_SESSION_STARTED,
    STUDY_SESSION_COMPLETED,

    // Executive Assistant events
    EXECUTIVE_SUGGESTION,
    EXECUTIVE_INSIGHT,
    EXECUTIVE_PATTERN_DETECTED,
    EXECUTIVE_DAILY_BRIEF,
    EXECUTIVE_URGENT_ACTION,

    COUNT  // Sentinel
};

struct Event {
    EventType type;
    String source;          // Module that emitted the event
    String data;            // JSON payload
    EventPriority priority;
    unsigned long timestamp;
    String id;              // Unique event ID

    Event() noexcept : type(EventType::COUNT), priority(EventPriority::NORMAL_PRIORITY), timestamp(0) {}
};

// Callback function signature
using EventCallback = void (*)(const Event& event);

struct EventHandler {
    EventType eventType;
    EventCallback callback;
    EventPriority minPriority;
    String handlerId;

    EventHandler() noexcept : eventType(EventType::COUNT), callback(nullptr), minPriority(EventPriority::LOW_PRIORITY) {}
};

class EventBus {
public:
    EventBus() noexcept;
    ~EventBus() noexcept;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] bool subscribe(EventType type, EventCallback callback, EventPriority minPriority = EventPriority::LOW_PRIORITY, const String& handlerId = "") noexcept;
    [[nodiscard]] bool unsubscribe(const String& handlerId) noexcept;
    void publish(const Event& event) noexcept;
    void publish(EventType type, const String& source, const String& data = "", EventPriority priority = EventPriority::NORMAL_PRIORITY) noexcept;

    [[nodiscard]] size_t pendingCount() const noexcept;
    [[nodiscard]] size_t handlerCount() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    void clearPending() noexcept;

private:
    static constexpr const char* kLogCategory = "EventBus";
    static constexpr size_t kMaxPendingEvents = 64;
    static constexpr size_t kMaxHandlers = 32;
    static constexpr unsigned long kPublishIntervalMs = 50;

    String generateId() noexcept;
    size_t findHandler(const String& handlerId) const noexcept;

    bool m_initialized;
    SemaphoreHandle_t m_pendingMutex;
    std::vector<Event> m_pending;
    std::vector<EventHandler> m_handlers;
    unsigned long m_lastPublishTime;
    unsigned long m_lastIdCounter;
};

extern EventBus eventBus;

#endif // AURA_EVENT_BUS_H