#include "oled_renderer.h"
#include <WiFi.h>
#include "memory_manager.h"
#include "reminder_manager.h"
#include "context_manager.h"
#include "storage_manager.h"
#include "performance_manager.h"

OledRenderer oledRenderer;

OledRenderer::OledRenderer() noexcept
    : m_initialized(false), m_overlayActive(false) {}

OledRenderer::~OledRenderer() noexcept = default;

bool OledRenderer::initialize() noexcept {
    if (m_initialized) return true;
    // DisplayManager is initialized by SystemManager — we just need it ready
    if (!displayManager.isInitialized()) {
        LOG_ERROR(kLogCategory, "DisplayManager not initialized");
        return false;
    }
    m_initialized = true;
    LOG_INFO(kLogCategory, "OledRenderer initialized");
    return true;
}

void OledRenderer::shutdown() noexcept {
    m_initialized = false;
}

void OledRenderer::renderScreen(const UIScreen& screen, RenderMode mode) noexcept {
    if (!m_initialized) return;

    switch (screen.id) {
        case ScreenID::DASHBOARD:
            renderDashboardScreen(screen);
            break;
        case ScreenID::ASSISTANT_CHAT:
            renderAssistantScreen(screen);
            break;
        case ScreenID::DEVICE_HEALTH:
            renderHealthScreen(screen);
            break;
        case ScreenID::NOTIFICATION_CENTER:
            displayManager.showNotification("Notifications", screen.data, 5000);
            break;
        default:
            renderMessageScreen(screen);
            break;
    }
}

void OledRenderer::renderWidget(const UIWidget& widget) noexcept {
    if (!m_initialized) return;

    // Map WidgetEngine widgets to DisplayManager dashboard data
    switch (widget.id) {
        case WidgetID::CLOCK:
            displayManager.updateWidgetData(DisplayManager::WidgetType::CLOCK, widget.value);
            break;
        case WidgetID::GREETING:
            displayManager.updateWidgetData(DisplayManager::WidgetType::GREETING, widget.value);
            break;
        case WidgetID::NEXT_REMINDER:
            displayManager.updateWidgetData(DisplayManager::WidgetType::NEXT_REMINDER, widget.value);
            break;
        case WidgetID::PROJECTS:
            displayManager.updateWidgetData(DisplayManager::WidgetType::PROJECT_STATUS, widget.value);
            break;
        case WidgetID::STUDY_PROGRESS:
            displayManager.updateWidgetData(DisplayManager::WidgetType::TODAYS_STUDY, widget.value);
            break;
        case WidgetID::ANALYTICS_SUMMARY:
            displayManager.updateWidgetData(DisplayManager::WidgetType::DAILY_PROGRESS, widget.value);
            break;
        case WidgetID::WIFI_STATUS:
        case WidgetID::STORAGE_STATUS:
        case WidgetID::HEAP_STATUS: {
            uint32_t freeHeap = ESP.getFreeHeap();
            uint32_t totalHeap = ESP.getHeapSize();
            uint32_t usedKB = 0, totalKB = 0;
            if (storageManager.isInitialized()) {
                size_t used, free, total;
                storageManager.getStatistics(StorageType::SD_CARD, used, free, total);
                usedKB = used / 1024;
                totalKB = total / 1024;
            }
            displayManager.setDashboardNumericData(freeHeap, totalHeap, usedKB, totalKB,
                WiFi.RSSI(), WiFi.isConnected());
            break;
        }
        case WidgetID::ACTIVE_CONTEXT: {
            String ctx = contextManager.isInitialized() ? contextManager.getAssistantContextName() : "";
            displayManager.updateWidgetData(DisplayManager::WidgetType::ACTIVE_CONTEXT, ctx);
            break;
        }
        case WidgetID::NOTIFICATIONS:
            displayManager.updateWidgetData(DisplayManager::WidgetType::NOTIFICATIONS,
                String(widget.progress));
            break;
        default:
            break;
    }
}

void OledRenderer::showNotification(const UINotification& notification) noexcept {
    if (!m_initialized) return;
    displayManager.showNotification(notification.title, notification.message, notification.durationMs);
}

void OledRenderer::showDialog(const UIDialog& dialog) noexcept {
    if (!m_initialized) return;
    displayManager.showMessage(dialog.title, dialog.message, "");
}

void OledRenderer::showOverlay(const UIOverlay& overlay) noexcept {
    if (!m_initialized) return;
    m_overlayActive = true;
    m_overlayText = overlay.content;
    displayManager.showMessage("", m_overlayText, "");
}

void OledRenderer::dismissOverlay() noexcept {
    m_overlayActive = false;
    m_overlayText = "";
    displayManager.showHome();
}

void OledRenderer::update() noexcept {
    // DisplayManager.update() is called from SystemManager — no duplicate call
}

void OledRenderer::sleep() noexcept {
    displayManager.sleep();
}

void OledRenderer::wake() noexcept {
    displayManager.wake();
}

RendererCapabilities OledRenderer::getCapabilities() const noexcept {
    RendererCapabilities caps;
    caps.displayWidth = kWidth;
    caps.displayHeight = kHeight;
    caps.bitDepth = 1;
    caps.maxFramerate = 30;
    caps.supportsPartialRedraw = true;
    caps.supportsAnimation = false;
    caps.supportsTouch = false;
    caps.supportsColor = false;
    caps.supportsInput = false;
    caps.supportsOverlays = true;
    caps.supportsDialogs = true;
    return caps;
}

bool OledRenderer::isInitialized() const noexcept {
    return m_initialized;
}

const char* OledRenderer::getName() const noexcept {
    return "OLED";
}

void OledRenderer::renderDashboardScreen(const UIScreen& screen) noexcept {
    displayManager.showHome();
}

void OledRenderer::renderAssistantScreen(const UIScreen& screen) noexcept {
    if (screen.data.indexOf("listening") >= 0) {
        displayManager.showListening();
    } else if (screen.data.indexOf("thinking") >= 0) {
        displayManager.showThinking();
    } else if (screen.data.indexOf("speaking") >= 0) {
        displayManager.showSpeaking();
    } else {
        displayManager.showHome();
    }
}

void OledRenderer::renderHealthScreen(const UIScreen& screen) noexcept {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t usedMB = 0, totalMB = 0;
    if (storageManager.isInitialized()) {
        size_t used, free, total;
        storageManager.getStatistics(StorageType::SD_CARD, used, free, total);
        usedMB = used / (1024 * 1024);
        totalMB = total / (1024 * 1024);
    }
    displayManager.showStorageStatus("SD", usedMB, totalMB);
}

void OledRenderer::renderMessageScreen(const UIScreen& screen) noexcept {
    if (!screen.title.isEmpty() || !screen.data.isEmpty()) {
        displayManager.showMessage(screen.title, screen.data);
    }
}