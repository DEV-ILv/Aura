#include "screen_manager.h"

ScreenManager screenManager;

ScreenManager::ScreenManager() noexcept
    : m_initialized(false)
    , m_homeScreen(ScreenID::DASHBOARD) {
    m_stack.reserve(kMaxStackDepth);
}

ScreenManager::~ScreenManager() noexcept = default;

bool ScreenManager::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    LOG_INFO(kLogCategory, "ScreenManager initialized");
    return true;
}

void ScreenManager::update() noexcept {}

void ScreenManager::pushScreen(const UIScreen& screen) noexcept {
    if (!m_initialized) return;

    if (screen.clearHistory) {
        m_stack.clear();
    }

    if (m_stack.size() >= kMaxStackDepth) {
        m_stack.erase(m_stack.begin());
    }

    NavigationEvent nav;
    nav.from = m_stack.empty() ? ScreenID::NONE : m_stack.back().id;
    nav.to = screen.id;
    nav.animated = screen.animated;
    nav.replace = false;

    m_stack.push_back(screen);

    if (eventBus.isInitialized()) {
        String data = "{\"from\":" + String(static_cast<uint8_t>(nav.from))
            + ",\"to\":" + String(static_cast<uint8_t>(nav.to))
            + ",\"animated\":" + String(nav.animated ? "true" : "false") + "}";
        eventBus.publish(EventType::SCREEN_CHANGED, "ScreenManager", data);
    }

    LOG_INFO(kLogCategory, "Pushed screen: %s (depth: %u)",
             screenIdToName(screen.id), m_stack.size());
}

void ScreenManager::replaceScreen(const UIScreen& screen) noexcept {
    if (!m_initialized) return;

    NavigationEvent nav;
    nav.from = m_stack.empty() ? ScreenID::NONE : m_stack.back().id;
    nav.to = screen.id;
    nav.replace = true;
    nav.animated = screen.animated;

    if (!m_stack.empty()) {
        m_stack.back() = screen;
    } else {
        m_stack.push_back(screen);
    }

    if (eventBus.isInitialized()) {
        String data = "{\"from\":" + String(static_cast<uint8_t>(nav.from))
            + ",\"to\":" + String(static_cast<uint8_t>(nav.to))
            + ",\"replace\":true}";
        eventBus.publish(EventType::SCREEN_CHANGED, "ScreenManager", data);
    }

    LOG_INFO(kLogCategory, "Replaced screen with: %s", screenIdToName(screen.id));
}

void ScreenManager::popScreen() noexcept {
    if (!m_initialized || m_stack.size() <= 1) {
        goHome();
        return;
    }

    NavigationEvent nav;
    nav.from = m_stack.back().id;
    m_stack.pop_back();
    nav.to = m_stack.back().id;
    nav.animated = true;

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::SCREEN_POPPED, "ScreenManager",
            "{\"to\":" + String(static_cast<uint8_t>(nav.to)) + "}");
    }

    LOG_INFO(kLogCategory, "Popped to: %s (depth: %u)",
             screenIdToName(nav.to), m_stack.size());
}

void ScreenManager::goHome() noexcept {
    if (!m_initialized) return;
    UIScreen home;
    home.id = m_homeScreen;
    home.clearHistory = true;
    pushScreen(home);
}

void ScreenManager::goBack() noexcept {
    popScreen();
}

void ScreenManager::clearStack() noexcept {
    m_stack.clear();
    LOG_INFO(kLogCategory, "Navigation stack cleared");
}

const UIScreen& ScreenManager::getCurrentScreen() const noexcept {
    if (m_stack.empty()) {
        static UIScreen empty;
        return empty;
    }
    return m_stack.back();
}

ScreenID ScreenManager::getCurrentScreenId() const noexcept {
    return m_stack.empty() ? ScreenID::NONE : m_stack.back().id;
}

size_t ScreenManager::stackDepth() const noexcept {
    return m_stack.size();
}

bool ScreenManager::isInitialized() const noexcept {
    return m_initialized;
}

void ScreenManager::setHomeScreen(ScreenID homeId) noexcept {
    m_homeScreen = homeId;
}

ScreenID ScreenManager::getHomeScreen() const noexcept {
    return m_homeScreen;
}