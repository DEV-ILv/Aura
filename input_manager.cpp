#include "input_manager.h"

InputManager inputManager;

InputManager::InputManager() noexcept
    : m_initialized(false), m_pendingCount(0) {
    for (auto& s : m_subscribers) s.active = false;
    for (auto& e : m_pendingEvents) e.type = InputEventType::NONE;
}

InputManager::~InputManager() noexcept = default;

bool InputManager::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    LOG_INFO(kLogCategory, "InputManager initialized");
    return true;
}

void InputManager::update() noexcept {
    if (!m_initialized) return;

    for (size_t i = 0; i < m_pendingCount; ++i) {
        const InputEvent& event = m_pendingEvents[i];
        for (size_t j = 0; j < kMaxSubscribers; ++j) {
            if (m_subscribers[j].active && m_subscribers[j].type == event.type) {
                m_subscribers[j].callback(event);
            }
        }
    }
    m_pendingCount = 0;
}

void InputManager::postEvent(const InputEvent& event) noexcept {
    if (m_pendingCount >= kMaxPendingEvents) return;
    m_pendingEvents[m_pendingCount++] = event;

    if (eventBus.isInitialized()) {
        String data = "{\"type\":" + String(static_cast<uint8_t>(event.type))
            + ",\"source\":\"" + event.source + "\"}";
        eventBus.publish(EventType::INPUT_EVENT, "InputManager", data);
    }
}

void InputManager::subscribe(InputEventType type, InputCallback callback) noexcept {
    for (auto& s : m_subscribers) {
        if (!s.active) {
            s.type = type;
            s.callback = callback;
            s.active = true;
            return;
        }
    }
}

void InputManager::unsubscribe(InputEventType type) noexcept {
    for (auto& s : m_subscribers) {
        if (s.active && s.type == type) {
            s.active = false;
            return;
        }
    }
}

bool InputManager::isInitialized() const noexcept {
    return m_initialized;
}