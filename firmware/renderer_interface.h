#ifndef AURA_RENDERER_INTERFACE_H
#define AURA_RENDERER_INTERFACE_H

#include <Arduino.h>
#include <cstdint>
#include "ui_event_types.h"

/**
 * @brief Capabilities a renderer reports to the framework
 */
struct RendererCapabilities {
    bool supportsTouch;
    bool supportsColor;
    bool supportsAnimation;
    bool supportsInput;
    bool supportsPartialRedraw;
    bool supportsOverlays;
    bool supportsDialogs;
    uint16_t displayWidth;
    uint16_t displayHeight;
    uint8_t bitDepth;
    uint8_t maxFramerate;

    RendererCapabilities() noexcept
        : supportsTouch(false), supportsColor(false), supportsAnimation(false),
          supportsInput(false), supportsPartialRedraw(false),
          supportsOverlays(false), supportsDialogs(false),
          displayWidth(0), displayHeight(0), bitDepth(1), maxFramerate(0) {}
};

/**
 * @brief RenderMode describes how a renderer should handle a screen update
 */
enum class RenderMode : uint8_t {
    FULL_REDRAW,
    PARTIAL_UPDATE,
    INCREMENTAL
};

/**
 * @brief Abstract interface for all display renderers
 *
 * Every renderer (OLED, LCD, Touchscreen, Web, Mobile) implements
 * this interface. The Core Assistant never knows which renderer
 * is active.
 */
class RendererInterface {
public:
    virtual ~RendererInterface() = default;

    /**
     * @brief Initialize the renderer hardware
     * @return true if initialization succeeded
     */
    virtual bool initialize() noexcept = 0;

    /**
     * @brief Shutdown the renderer and release resources
     */
    virtual void shutdown() noexcept = 0;

    /**
     * @brief Render a complete screen
     * @param screen The screen data to render
     * @param mode Rendering mode
     */
    virtual void renderScreen(const UIScreen& screen, RenderMode mode = RenderMode::FULL_REDRAW) noexcept = 0;

    /**
     * @brief Render a single widget
     * @param widget The widget data to render
     */
    virtual void renderWidget(const UIWidget& widget) noexcept = 0;

    /**
     * @brief Show a notification overlay
     * @param notification Notification data
     */
    virtual void showNotification(const UINotification& notification) noexcept = 0;

    /**
     * @brief Show a dialog overlay
     * @param dialog Dialog data
     */
    virtual void showDialog(const UIDialog& dialog) noexcept = 0;

    /**
     * @brief Show an overlay on top of current screen
     * @param overlay Overlay data
     */
    virtual void showOverlay(const UIOverlay& overlay) noexcept = 0;

    /**
     * @brief Dismiss current overlay
     */
    virtual void dismissOverlay() noexcept = 0;

    /**
     * @brief Main update loop — call from system update()
     */
    virtual void update() noexcept = 0;

    /**
     * @brief Enter low-power sleep mode
     */
    virtual void sleep() noexcept = 0;

    /**
     * @brief Wake from sleep mode
     */
    virtual void wake() noexcept = 0;

    /**
     * @brief Get renderer capabilities
     * @return Capabilities struct
     */
    virtual RendererCapabilities getCapabilities() const noexcept = 0;

    /**
     * @brief Check if renderer is initialized
     */
    virtual bool isInitialized() const noexcept = 0;

    /**
     * @brief Get renderer name
     */
    virtual const char* getName() const noexcept = 0;
};

#endif // AURA_RENDERER_INTERFACE_H