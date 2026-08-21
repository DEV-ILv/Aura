#include "widget_engine.h"

WidgetEngine widgetEngine;

WidgetEngine::WidgetEngine() noexcept
    : m_initialized(false)
    , m_gridCols(4)
    , m_gridRows(6)
    , m_lastUpdateTime(0) {
    for (auto& reg : m_widgetRegistered) reg = false;
}

WidgetEngine::~WidgetEngine() noexcept = default;

bool WidgetEngine::initialize() noexcept {
    if (m_initialized) return true;

    for (size_t i = 0; i < kMaxWidgets; ++i) {
        m_widgets[i].widget.id = static_cast<WidgetID>(i);
    }

    m_initialized = true;
    LOG_INFO(kLogCategory, "WidgetEngine initialized (%u slots)", kMaxWidgets);
    return true;
}

void WidgetEngine::update() noexcept {
    if (!m_initialized) return;

    for (size_t i = 0; i < kMaxWidgets; ++i) {
        if (m_widgetRegistered[i] && m_widgets[i].enabled && m_widgets[i].dataCallback) {
            m_widgets[i].dataCallback(m_widgets[i].widget);
        }
    }
}

bool WidgetEngine::registerWidget(WidgetID id, WidgetDataCallback dataCallback) noexcept {
    size_t idx = static_cast<size_t>(id);
    if (idx >= kMaxWidgets) return false;

    m_widgetRegistered[idx] = true;
    m_widgets[idx].widget.id = id;
    m_widgets[idx].widget.lastUpdated = millis();
    m_widgets[idx].dataCallback = dataCallback;
    m_widgets[idx].dirty = true;

    LOG_DEBUG(kLogCategory, "Widget registered: %d", idx);
    return true;
}

void WidgetEngine::unregisterWidget(WidgetID id) noexcept {
    size_t idx = static_cast<size_t>(id);
    if (idx < kMaxWidgets) {
        m_widgetRegistered[idx] = false;
    }
}

UIWidget* WidgetEngine::getWidget(WidgetID id) noexcept {
    size_t idx = static_cast<size_t>(id);
    return (idx < kMaxWidgets && m_widgetRegistered[idx]) ? &m_widgets[idx].widget : nullptr;
}

const UIWidget* WidgetEngine::getWidget(WidgetID id) const noexcept {
    size_t idx = static_cast<size_t>(id);
    return (idx < kMaxWidgets && m_widgetRegistered[idx]) ? &m_widgets[idx].widget : nullptr;
}

std::vector<UIWidget*> WidgetEngine::getWidgetsByRow(uint8_t row) noexcept {
    std::vector<UIWidget*> result;
    for (size_t i = 0; i < kMaxWidgets; ++i) {
        if (m_widgetRegistered[i] && m_widgets[i].enabled && m_widgets[i].widget.row == row) {
            result.push_back(&m_widgets[i].widget);
        }
    }
    return result;
}

void WidgetEngine::showWidget(WidgetID id) noexcept {
    size_t idx = static_cast<size_t>(id);
    if (idx < kMaxWidgets) {
        m_widgets[idx].widget.visible = true;
        m_widgets[idx].dirty = true;
    }
}

void WidgetEngine::hideWidget(WidgetID id) noexcept {
    size_t idx = static_cast<size_t>(id);
    if (idx < kMaxWidgets) {
        m_widgets[idx].widget.visible = false;
        m_widgets[idx].dirty = true;
    }
}

bool WidgetEngine::isWidgetVisible(WidgetID id) const noexcept {
    size_t idx = static_cast<size_t>(id);
    return (idx < kMaxWidgets) ? m_widgets[idx].widget.visible : false;
}

void WidgetEngine::markDirty(WidgetID id) noexcept {
    size_t idx = static_cast<size_t>(id);
    if (idx < kMaxWidgets) m_widgets[idx].dirty = true;
}

void WidgetEngine::markAllDirty() noexcept {
    for (size_t i = 0; i < kMaxWidgets; ++i) {
        if (m_widgetRegistered[i]) m_widgets[i].dirty = true;
    }
}

bool WidgetEngine::isDirty(WidgetID id) const noexcept {
    size_t idx = static_cast<size_t>(id);
    return (idx < kMaxWidgets) ? m_widgets[idx].dirty : false;
}

std::vector<WidgetID> WidgetEngine::getDirtyWidgets() noexcept {
    std::vector<WidgetID> dirty;
    for (size_t i = 0; i < kMaxWidgets; ++i) {
        if (m_widgetRegistered[i] && m_widgets[i].dirty) {
            dirty.push_back(static_cast<WidgetID>(i));
            m_widgets[i].dirty = false;
        }
    }
    return dirty;
}

void WidgetEngine::setWidgetPosition(WidgetID id, uint8_t col, uint8_t row, uint8_t w, uint8_t h) noexcept {
    size_t idx = static_cast<size_t>(id);
    if (idx < kMaxWidgets && m_widgetRegistered[idx]) {
        m_widgets[idx].widget.column = col;
        m_widgets[idx].widget.row = row;
        m_widgets[idx].widget.width = w;
        m_widgets[idx].widget.height = h;
        m_widgets[idx].dirty = true;
    }
}

void WidgetEngine::setGridSize(uint8_t cols, uint8_t rows) noexcept {
    m_gridCols = cols;
    m_gridRows = rows;
}

void WidgetEngine::enableWidget(WidgetID id, bool enabled) noexcept {
    size_t idx = static_cast<size_t>(id);
    if (idx < kMaxWidgets && m_widgetRegistered[idx]) {
        m_widgets[idx].enabled = enabled;
        m_widgets[idx].dirty = true;
    }
}

bool WidgetEngine::isWidgetEnabled(WidgetID id) const noexcept {
    size_t idx = static_cast<size_t>(id);
    return (idx < kMaxWidgets) ? m_widgets[idx].enabled : false;
}

bool WidgetEngine::isInitialized() const noexcept {
    return m_initialized;
}

size_t WidgetEngine::widgetCount() const noexcept {
    size_t count = 0;
    for (size_t i = 0; i < kMaxWidgets; ++i) {
        if (m_widgetRegistered[i]) count++;
    }
    return count;
}