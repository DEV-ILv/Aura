#include "ui_framework.h"
#include "screen_manager.h"
#include "widget_engine.h"
#include "animation_engine.h"
#include "theme_manager.h"
#include "input_manager.h"
#include "renderer_manager.h"

UIFramework uiFramework;

namespace {

void HandleNotificationEvent(const Event& e) {
    auto* renderer = rendererManager.getActiveRenderer();
    if (renderer) {
        UINotification notif;
        notif.title = e.source;
        notif.message = e.data;
        notif.durationMs = 3000;
        renderer->showNotification(notif);
    }
}

} // namespace

UIFramework::UIFramework() noexcept
    : m_initialized(false), m_headless(true) {}

UIFramework::~UIFramework() noexcept {
    shutdown();
}

#define CHECK_INIT(CALL, name) do { \
    LOG_DEBUG(kLogCategory, "Initializing " name); \
    if (!(CALL)) { \
        LOG_ERROR(kLogCategory, "Failed to initialize " name); \
        return false; \
    } \
} while(0)

bool UIFramework::initialize() noexcept {
    if (m_initialized) return true;

    LOG_INFO(kLogCategory, "Booting UI Framework");

    // 1. Theme (no dependencies)
    CHECK_INIT(themeManager.initialize(), "ThemeManager");

    // 2. Animation engine (standalone)
    CHECK_INIT(animationEngine.initialize(), "AnimationEngine");

    // 3. Widget engine (needs animation)
    CHECK_INIT(widgetEngine.initialize(), "WidgetEngine");

    // 4. Input (standalone)
    CHECK_INIT(inputManager.initialize(), "InputManager");

    // 5. Screen manager (needs widget engine)
    CHECK_INIT(screenManager.initialize(), "ScreenManager");

    // 6. Renderer manager (detects hardware, initializes renderer)
    CHECK_INIT(rendererManager.initialize(), "RendererManager");

    m_headless = rendererManager.isHeadless();

    // Subscribe to relevant events
    if (eventBus.isInitialized()) {
        eventBus.subscribe(EventType::NOTIFICATION_TRIGGERED, HandleNotificationEvent);
    }

    m_initialized = true;
    LOG_INFO(kLogCategory, "UI Framework ready (headless=%s)", m_headless ? "yes" : "no");
    return true;
}

void UIFramework::update() noexcept {
    if (!m_initialized) return;

    inputManager.update();
    animationEngine.update();
    screenManager.update();
    widgetEngine.update();
    rendererManager.update();

    // If headless, skip render
    if (m_headless) return;

    // Get current screen and render it
    UIScreen currentScreen = screenManager.getCurrentScreen();
    if (currentScreen.id != ScreenID::NONE) {
        currentScreen = screenManager.getCurrentScreen();
    }
}

void UIFramework::shutdown() noexcept {
    if (!m_initialized) return;
    rendererManager.shutdown();
    screenManager.clearStack();
    animationEngine.stopAll();
    m_initialized = false;
}

ScreenManager& UIFramework::getScreenManager() noexcept {
    return screenManager;
}

WidgetEngine& UIFramework::getWidgetEngine() noexcept {
    return widgetEngine;
}

AnimationEngine& UIFramework::getAnimationEngine() noexcept {
    return animationEngine;
}

ThemeManager& UIFramework::getThemeManager() noexcept {
    return themeManager;
}

InputManager& UIFramework::getInputManager() noexcept {
    return inputManager;
}

RendererManager& UIFramework::getRendererManager() noexcept {
    return rendererManager;
}

void UIFramework::showScreen(ScreenID id, const String& data) noexcept {
    if (!m_initialized) return;
    UIScreen screen;
    screen.id = id;
    screen.data = data;
    screenManager.replaceScreen(screen);

    auto* renderer = rendererManager.getActiveRenderer();
    if (renderer) {
        UIScreen screen;
        screen.id = id;
        screen.data = data;
        renderer->renderScreen(screen);
    }
}

void UIFramework::showNotification(const String& title, const String& message, unsigned long durationMs) noexcept {
    if (!m_initialized) return;
    auto* renderer = rendererManager.getActiveRenderer();
    if (renderer) {
        UINotification notif;
        notif.title = title;
        notif.message = message;
        notif.durationMs = durationMs;
        renderer->showNotification(notif);
    }
}

void UIFramework::pushWidgetUpdate(const UIWidget& widget) noexcept {
    if (!m_initialized) return;
    widgetEngine.markDirty(widget.id);

    auto* renderer = rendererManager.getActiveRenderer();
    if (renderer) {
        renderer->renderWidget(widget);
    }
}

bool UIFramework::isInitialized() const noexcept {
    return m_initialized;
}

bool UIFramework::isHeadless() const noexcept {
    return m_headless;
}