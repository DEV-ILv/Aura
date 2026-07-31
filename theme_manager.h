#ifndef AURA_THEME_MANAGER_H
#define AURA_THEME_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "settings_manager.h"

enum class ThemeMode : uint8_t {
    DARK,
    LIGHT,
    AUTO
};

struct ThemeColors {
    uint32_t background;
    uint32_t surface;
    uint32_t primary;
    uint32_t secondary;
    uint32_t accent;
    uint32_t text;
    uint32_t textSecondary;
    uint32_t textOnPrimary;
    uint32_t error;
    uint32_t warning;
    uint32_t success;
    uint32_t info;
    uint32_t border;
    uint32_t shadow;
};

class ThemeManager {
public:
    ThemeManager() noexcept;
    ~ThemeManager() noexcept;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    void setTheme(ThemeMode mode) noexcept;
    ThemeMode getTheme() const noexcept;
    const ThemeColors& getColors() const noexcept;

    void setAccentColor(uint32_t color) noexcept;
    uint32_t getAccentColor() const noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

private:
    void buildDarkTheme() noexcept;
    void buildLightTheme() noexcept;
    void applyAccent() noexcept;

    static constexpr const char* kLogCategory = "ThemeManager";

    bool m_initialized;
    ThemeMode m_mode;
    ThemeColors m_colors;
    uint32_t m_accentColor;
};

extern ThemeManager themeManager;

#endif