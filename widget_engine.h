#ifndef AURA_WIDGET_ENGINE_H
#define AURA_WIDGET_ENGINE_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "ui_event_types.h"
#include "event_bus.h"

/**
 * @brief Callback for widget data updates
 */
using WidgetDataCallback = void (*)(UIWidget& widget);

/**
 * @class WidgetEngine
 * @brief Modular widget system with independent refresh
 *
 * Manages a registry of all UI widgets. Widgets are data-only;
 * renderers interpret them. Each widget tracks its own dirty state
 * for partial redraw support.
 */
class WidgetEngine {
public:
    WidgetEngine() noexcept;
    ~WidgetEngine() noexcept;

    WidgetEngine(const WidgetEngine&) = delete;
    WidgetEngine& operator=(const WidgetEngine&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    // Registration
    bool registerWidget(WidgetID id, WidgetDataCallback dataCallback = nullptr) noexcept;
    void unregisterWidget(WidgetID id) noexcept;

    // Data access
    UIWidget* getWidget(WidgetID id) noexcept;
    const UIWidget* getWidget(WidgetID id) const noexcept;
    std::vector<UIWidget*> getWidgetsByRow(uint8_t row) noexcept;

    // State
    void showWidget(WidgetID id) noexcept;
    void hideWidget(WidgetID id) noexcept;
    bool isWidgetVisible(WidgetID id) const noexcept;

    // Dirty tracking for partial redraw
    void markDirty(WidgetID id) noexcept;
    void markAllDirty() noexcept;
    bool isDirty(WidgetID id) const noexcept;
    std::vector<WidgetID> getDirtyWidgets() noexcept;

    // Grid layout
    void setWidgetPosition(WidgetID id, uint8_t col, uint8_t row, uint8_t w, uint8_t h) noexcept;
    void setGridSize(uint8_t cols, uint8_t rows) noexcept;

    // Settings
    void enableWidget(WidgetID id, bool enabled) noexcept;
    bool isWidgetEnabled(WidgetID id) const noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] size_t widgetCount() const noexcept;

private:
    static constexpr const char* kLogCategory = "WidgetEngine";
    static constexpr size_t kMaxWidgets = static_cast<size_t>(WidgetID::COUNT);

    struct WidgetRecord {
        UIWidget widget;
        WidgetDataCallback dataCallback;
        bool enabled;
        bool dirty;

        WidgetRecord() noexcept : dataCallback(nullptr), enabled(true), dirty(true) {}
    };

    bool m_initialized;
    WidgetRecord m_widgets[kMaxWidgets];
    bool m_widgetRegistered[kMaxWidgets];
    uint8_t m_gridCols;
    uint8_t m_gridRows;
    unsigned long m_lastUpdateTime;
};

extern WidgetEngine widgetEngine;

#endif // AURA_WIDGET_ENGINE_H