#ifndef AURA_SCREEN_MANAGER_H
#define AURA_SCREEN_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "ui_event_types.h"
#include "event_bus.h"

/**
 * @class ScreenManager
 * @brief Manages screen definitions and navigation stack
 *
 * The ScreenManager owns the screen stack and navigation history.
 * Screens are data-only — they describe what to display.
 * Renderers interpret the screen data for their specific hardware.
 *
 * Navigation: push, pop, replace, goHome, goBack.
 */
class ScreenManager {
public:
    ScreenManager() noexcept;
    ~ScreenManager() noexcept;

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    // Navigation
    void pushScreen(const UIScreen& screen) noexcept;
    void replaceScreen(const UIScreen& screen) noexcept;
    void popScreen() noexcept;
    void goHome() noexcept;
    void goBack() noexcept;
    void clearStack() noexcept;

    // Accessors
    [[nodiscard]] const UIScreen& getCurrentScreen() const noexcept;
    [[nodiscard]] ScreenID getCurrentScreenId() const noexcept;
    [[nodiscard]] size_t stackDepth() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;

    // Home screen
    void setHomeScreen(ScreenID homeId) noexcept;
    [[nodiscard]] ScreenID getHomeScreen() const noexcept;

private:
    static constexpr const char* kLogCategory = "ScreenManager";
    static constexpr size_t kMaxStackDepth = 16;

    bool m_initialized;
    std::vector<UIScreen> m_stack;
    ScreenID m_homeScreen;
};

extern ScreenManager screenManager;

#endif // AURA_SCREEN_MANAGER_H