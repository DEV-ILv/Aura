#ifndef AURA_OLED_RENDERER_H
#define AURA_OLED_RENDERER_H

#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "renderer_interface.h"
#include "ui_event_types.h"
#include "display_manager.h"
#include "widget_engine.h"

/**
 * @class OledRenderer
 * @brief OLED renderer implementing RendererInterface
 *
 * Wraps the existing DisplayManager to implement the standard
 * RendererInterface. This allows the UI framework to communicate
 * with the OLED display through the same API as any other renderer.
 *
 * The existing DisplayManager public API remains unchanged for
 * backward compatibility.
 */
class OledRenderer : public RendererInterface {
public:
    OledRenderer() noexcept;
    ~OledRenderer() noexcept;

    // RendererInterface implementation
    bool initialize() noexcept override;
    void shutdown() noexcept override;
    void renderScreen(const UIScreen& screen, RenderMode mode = RenderMode::FULL_REDRAW) noexcept override;
    void renderWidget(const UIWidget& widget) noexcept override;
    void showNotification(const UINotification& notification) noexcept override;
    void showDialog(const UIDialog& dialog) noexcept override;
    void showOverlay(const UIOverlay& overlay) noexcept override;
    void dismissOverlay() noexcept override;
    void update() noexcept override;
    void sleep() noexcept override;
    void wake() noexcept override;
    RendererCapabilities getCapabilities() const noexcept override;
    bool isInitialized() const noexcept override;
    const char* getName() const noexcept override;

private:
    void renderDashboardScreen(const UIScreen& screen) noexcept;
    void renderAssistantScreen(const UIScreen& screen) noexcept;
    void renderHealthScreen(const UIScreen& screen) noexcept;
    void renderMessageScreen(const UIScreen& screen) noexcept;

    static constexpr const char* kLogCategory = "OledRenderer";
    static constexpr uint16_t kWidth = OLED_WIDTH;
    static constexpr uint16_t kHeight = OLED_HEIGHT;

    bool m_initialized;
    bool m_overlayActive;
    String m_overlayText;
};

extern OledRenderer oledRenderer;

#endif // AURA_OLED_RENDERER_H