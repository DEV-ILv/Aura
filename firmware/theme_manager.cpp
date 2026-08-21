#include "theme_manager.h"

ThemeManager themeManager;

ThemeManager::ThemeManager() noexcept
    : m_initialized(false)
    , m_mode(ThemeMode::DARK)
    , m_accentColor(0x007AFF) {}

ThemeManager::~ThemeManager() noexcept = default;

bool ThemeManager::initialize() noexcept {
    if (m_initialized) return true;

    buildDarkTheme();
    if (m_mode == ThemeMode::LIGHT) buildLightTheme();
    applyAccent();

    m_initialized = true;
    LOG_INFO(kLogCategory, "ThemeManager initialized (mode=%d)", static_cast<int>(m_mode));
    return true;
}

void ThemeManager::update() noexcept {
    if (m_mode == ThemeMode::AUTO) {
        unsigned long hour = (millis() / 3600000UL) % 24;
        bool isNight = (hour < 6 || hour >= 20);
        ThemeMode target = isNight ? ThemeMode::DARK : ThemeMode::LIGHT;
        if (target != m_mode) setTheme(target);
    }
}

void ThemeManager::setTheme(ThemeMode mode) noexcept {
    if (m_mode == mode) return;
    m_mode = mode;

    if (mode == ThemeMode::DARK || mode == ThemeMode::AUTO) buildDarkTheme();
    else buildLightTheme();

    applyAccent();
    LOG_INFO(kLogCategory, "Theme changed to %s", mode == ThemeMode::DARK ? "dark" : mode == ThemeMode::LIGHT ? "light" : "auto");
}

ThemeMode ThemeManager::getTheme() const noexcept {
    return m_mode;
}

const ThemeColors& ThemeManager::getColors() const noexcept {
    return m_colors;
}

void ThemeManager::setAccentColor(uint32_t color) noexcept {
    m_accentColor = color;
    applyAccent();
}

uint32_t ThemeManager::getAccentColor() const noexcept {
    return m_accentColor;
}

bool ThemeManager::isInitialized() const noexcept {
    return m_initialized;
}

void ThemeManager::buildDarkTheme() noexcept {
    m_colors.background     = 0x1A1A2E;
    m_colors.surface        = 0x16213E;
    m_colors.primary        = 0x0F3460;
    m_colors.secondary      = 0x533483;
    m_colors.text           = 0xE8E8E8;
    m_colors.textSecondary  = 0x9A9A9A;
    m_colors.textOnPrimary  = 0xFFFFFF;
    m_colors.error          = 0xFF4444;
    m_colors.warning        = 0xFFAA00;
    m_colors.success        = 0x44CC44;
    m_colors.info           = 0x4488FF;
    m_colors.border         = 0x2A2A4A;
    m_colors.shadow         = 0x0A0A1A;
}

void ThemeManager::buildLightTheme() noexcept {
    m_colors.background     = 0xF5F5F5;
    m_colors.surface        = 0xFFFFFF;
    m_colors.primary        = 0x007AFF;
    m_colors.secondary      = 0x5856D6;
    m_colors.text           = 0x1A1A1A;
    m_colors.textSecondary  = 0x6E6E6E;
    m_colors.textOnPrimary  = 0xFFFFFF;
    m_colors.error          = 0xFF3B30;
    m_colors.warning        = 0xFF9500;
    m_colors.success        = 0x34C759;
    m_colors.info           = 0x007AFF;
    m_colors.border         = 0xD1D1D6;
    m_colors.shadow         = 0x00000020;
}

void ThemeManager::applyAccent() noexcept {
    if (m_accentColor != 0x007AFF) {
        m_colors.accent = m_accentColor;
    } else {
        m_colors.accent = m_colors.primary;
    }
}