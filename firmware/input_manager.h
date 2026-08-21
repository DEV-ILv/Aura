#ifndef AURA_INPUT_MANAGER_H
#define AURA_INPUT_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "logger.h"
#include "ui_event_types.h"
#include "event_bus.h"

using InputCallback = std::function<void(const InputEvent&)>;

/**
 * @class InputManager
 * @brief Abstract input event hub
 *
 * All input sources (touch, keyboard, voice, buttons) post events here.
 * Handlers subscribe to event types. The Core Assistant never knows
 * the input source.
 */
class InputManager {
public:
    InputManager() noexcept;
    ~InputManager() noexcept;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    void postEvent(const InputEvent& event) noexcept;
    void subscribe(InputEventType type, InputCallback callback) noexcept;
    void unsubscribe(InputEventType type) noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "InputManager";
    static constexpr size_t kMaxSubscribers = 16;
    static constexpr size_t kMaxPendingEvents = 32;

    struct InputSubscriber {
        InputEventType type;
        InputCallback callback;
        bool active;
    };

    bool m_initialized;
    InputSubscriber m_subscribers[kMaxSubscribers];
    InputEvent m_pendingEvents[kMaxPendingEvents];
    size_t m_pendingCount;
};

extern InputManager inputManager;

#endif