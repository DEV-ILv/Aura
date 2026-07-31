#include "renderer_manager.h"
#include <Wire.h>
#include "oled_renderer.h"

RendererManager rendererManager;

RendererManager::RendererManager() noexcept
    : m_initialized(false)
    , m_activeType(RendererType::NONE)
    , m_activeRenderer(nullptr) {}

RendererManager::~RendererManager() noexcept {
    shutdown();
}

bool RendererManager::initialize() noexcept {
    if (m_initialized) return true;

    RendererType detected = detectDisplay();

    if (detected == RendererType::OLED || detected == RendererType::LCD) {
        extern OledRenderer oledRenderer;
        m_activeRenderer = &oledRenderer;
        m_activeType = detected;
    }

    if (m_activeRenderer) {
        if (!m_activeRenderer->initialize()) {
            LOG_ERROR(kLogCategory, "Renderer initialization failed, falling back to headless");
            m_activeRenderer = nullptr;
            m_activeType = RendererType::HEADLESS;
        } else {
            m_capabilities = m_activeRenderer->getCapabilities();
        }
    } else {
        m_activeType = RendererType::HEADLESS;
        LOG_INFO(kLogCategory, "No display detected — running headless");
    }

    if (m_activeType != RendererType::HEADLESS) {
        RendererType touchType = detectTouch();
        if (touchType == RendererType::TOUCHSCREEN) {
            m_capabilities.supportsTouch = true;
        }
    }

    m_initialized = true;

    if (eventBus.isInitialized()) {
        String data = "{\"renderer\":\"" + String(static_cast<uint8_t>(m_activeType)) + "\"}";
        eventBus.publish(EventType::RENDERER_CHANGED, "RendererManager", data);
    }

    LOG_INFO(kLogCategory, "RendererManager initialized (renderer=%d, touch=%s)",
             static_cast<int>(m_activeType),
             m_capabilities.supportsTouch ? "yes" : "no");
    return true;
}

void RendererManager::update() noexcept {
    if (m_activeRenderer) {
        m_activeRenderer->update();
    }
}

void RendererManager::shutdown() noexcept {
    if (m_activeRenderer) {
        m_activeRenderer->shutdown();
        m_activeRenderer = nullptr;
    }
    m_activeType = RendererType::NONE;
    m_initialized = false;
}

RendererInterface* RendererManager::getActiveRenderer() noexcept {
    return m_activeRenderer;
}

RendererType RendererManager::getActiveRendererType() const noexcept {
    return m_activeType;
}

const RendererCapabilities& RendererManager::getCapabilities() const noexcept {
    return m_capabilities;
}

bool RendererManager::isHeadless() const noexcept {
    return m_activeType == RendererType::HEADLESS || m_activeRenderer == nullptr;
}

bool RendererManager::supportsTouch() const noexcept {
    return m_capabilities.supportsTouch;
}

bool RendererManager::supportsColor() const noexcept {
    return m_capabilities.supportsColor;
}

bool RendererManager::supportsAnimation() const noexcept {
    return m_capabilities.supportsAnimation;
}

bool RendererManager::isInitialized() const noexcept {
    return m_initialized;
}

void RendererManager::setRenderer(RendererType type) noexcept {
    if (m_activeRenderer) {
        m_activeRenderer->shutdown();
    }
    m_activeType = type;
    m_activeRenderer = nullptr;
    m_initialized = false;
}

RendererType RendererManager::detectDisplay() noexcept {
    // Try I2C OLED scan (SSD1306 default address 0x3C)
    Wire.beginTransmission(OLED_ADDRESS);
    if (Wire.endTransmission() == 0) {
        LOG_INFO(kLogCategory, "OLED display detected at 0x%02X", OLED_ADDRESS);
        return RendererType::OLED;
    }

    // Try alternative OLED address 0x3D
    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
        LOG_INFO(kLogCategory, "Display detected at 0x3D");
        return RendererType::OLED;
    }

    // No display detected
    return RendererType::NONE;
}

RendererType RendererManager::detectTouch() noexcept {
    // Touch detection placeholder
    // In production, query touch controller over I2C or SPI
    return RendererType::NONE;
}