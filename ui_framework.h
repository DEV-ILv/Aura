#ifndef AURA_UI_FRAMEWORK_H
#define AURA_UI_FRAMEWORK_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "ui_event_types.h"

// Forward declarations
class ScreenManager;
class WidgetEngine;
class AnimationEngine;
class ThemeManager;
class InputManager;
class RendererManager;

/**
 * @class UIFramework
 * @brief Top-level orchestration of the UI framework
 *
 * Owns all UI subsystem managers. Initialize this once from SystemManager;
 * it boots subsystems in dependency order, then updates them each loop.
 *
 * The Core Assistant communicates with the UI through UIFramework only.
 */
class UIFramework {
public:
    UIFramework() noexcept;
    ~UIFramework() noexcept;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    void shutdown() noexcept;

    // Accessors to subsystems
    ScreenManager&      getScreenManager() noexcept;
    WidgetEngine&       getWidgetEngine() noexcept;
    AnimationEngine&    getAnimationEngine() noexcept;
    ThemeManager&       getThemeManager() noexcept;
    InputManager&       getInputManager() noexcept;
    RendererManager&    getRendererManager() noexcept;

    // Convenience
    void showScreen(ScreenID id, const String& data = "") noexcept;
    void showNotification(const String& title, const String& message, unsigned long durationMs = 3000) noexcept;
    void pushWidgetUpdate(const UIWidget& widget) noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool isHeadless() const noexcept;

private:
    static constexpr const char* kLogCategory = "UIFramework";

    bool m_initialized;
    bool m_headless;
};

extern UIFramework uiFramework;

#endif // AURA_UI_FRAMEWORK_H