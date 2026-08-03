#include "event_bus.h"

EventBus eventBus;

EventBus::EventBus() noexcept
    : m_initialized(false), m_lastPublishTime(0), m_lastIdCounter(0) {
}

EventBus::~EventBus() noexcept {
}

bool EventBus::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized");
    return true;
}

void EventBus::update() noexcept {
    if (!m_initialized || m_pending.empty()) return;

    unsigned long now = millis();
    if (now - m_lastPublishTime < kPublishIntervalMs) return;
    m_lastPublishTime = now;

    // Process one event per update cycle
    Event event = m_pending.front();
    m_pending.erase(m_pending.begin());

    for (const auto& handler : m_handlers) {
        if (handler.eventType == event.type &&
            handler.callback != nullptr &&
            static_cast<uint8_t>(event.priority) >= static_cast<uint8_t>(handler.minPriority)) {
            handler.callback(event);
        }
    }
}

bool EventBus::subscribe(EventType type, EventCallback callback, EventPriority minPriority, const String& handlerId) noexcept {
    if (!m_initialized || callback == nullptr) return false;
    if (m_handlers.size() >= kMaxHandlers) return false;

    EventHandler handler;
    handler.eventType = type;
    handler.callback = callback;
    handler.minPriority = minPriority;
    handler.handlerId = handlerId.isEmpty() ? String("handler_") + String(m_handlers.size()) : handlerId;

    m_handlers.push_back(handler);
    Logger::debug(kLogCategory, "Handler subscribed to event type %d", static_cast<int>(type));
    return true;
}

bool EventBus::unsubscribe(const String& handlerId) noexcept {
    size_t idx = findHandler(handlerId);
    if (idx == SIZE_MAX) return false;
    m_handlers.erase(m_handlers.begin() + static_cast<ptrdiff_t>(idx));
    return true;
}

void EventBus::publish(const Event& event) noexcept {
    if (!m_initialized) return;
    if (m_pending.size() >= kMaxPendingEvents) {
        Logger::warning(kLogCategory, "Pending queue full, dropping event");
        return;
    }

    Event ev = event;
    if (ev.id.isEmpty()) ev.id = generateId();
    if (ev.timestamp == 0) ev.timestamp = millis();

    m_pending.push_back(ev);
}

void EventBus::publish(EventType type, const String& source, const String& data, EventPriority priority) noexcept {
    Event ev;
    ev.type = type;
    ev.source = source;
    ev.data = data;
    ev.priority = priority;
    ev.timestamp = millis();
    ev.id = generateId();
    publish(ev);
}

size_t EventBus::pendingCount() const noexcept {
    return m_pending.size();
}

size_t EventBus::handlerCount() const noexcept {
    return m_handlers.size();
}

bool EventBus::isInitialized() const noexcept {
    return m_initialized;
}

void EventBus::clearPending() noexcept {
    m_pending.clear();
}

String EventBus::generateId() noexcept {
    unsigned long now = millis();
    m_lastIdCounter++;
    char buf[20];
    snprintf(buf, sizeof(buf), "evt_%lx_%lx", now, static_cast<unsigned long>(m_lastIdCounter));
    return String(buf);
}

size_t EventBus::findHandler(const String& handlerId) const noexcept {
    for (size_t i = 0; i < m_handlers.size(); ++i) {
        if (m_handlers[i].handlerId == handlerId) return i;
    }
    return SIZE_MAX;
}