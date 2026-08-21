#ifndef AURA_RENDERER_MANAGER_H
#define AURA_RENDERER_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "renderer_interface.h"
#include "ui_event_types.h"

/**
 * @class RendererManager
 * @brief Detects available displays, selects and initializes the appropriate renderer
 *
 * Boot sequence:
 * 1. Detect display hardware (OLED, LCD, Touch, None)
 * 2. Select best renderer for detected hardware
 * 3. Initialize the selected renderer
 * 4. Report capabilities to the framework
 */
class RendererManager {
public:
    RendererManager() noexcept;
    ~RendererManager() noexcept;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;
    void shutdown() noexcept;

    RendererInterface* getActiveRenderer() noexcept;
    RendererType getActiveRendererType() const noexcept;
    const RendererCapabilities& getCapabilities() const noexcept;

    bool isHeadless() const noexcept;
    bool supportsTouch() const noexcept;
    bool supportsColor() const noexcept;
    bool supportsAnimation() const noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

    // Manual override (for testing)
    void setRenderer(RendererType type) noexcept;

private:
    RendererType detectDisplay() noexcept;
    RendererType detectTouch() noexcept;

    static constexpr const char* kLogCategory = "RendererManager";

    bool m_initialized;
    RendererType m_activeType;
    RendererInterface* m_activeRenderer;
    RendererCapabilities m_capabilities;
};

extern RendererManager rendererManager;

#endif // AURA_RENDERER_MANAGER_H