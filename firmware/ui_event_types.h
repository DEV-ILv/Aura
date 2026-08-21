#ifndef AURA_UI_EVENT_TYPES_H
#define AURA_UI_EVENT_TYPES_H

#include <Arduino.h>
#include <vector>
#include <cstdint>

// ============================================================================
// Screen Types
// ============================================================================

enum class ScreenID : uint8_t {
    NONE = 0,
    DASHBOARD,
    ASSISTANT_CHAT,
    SEARCH,
    SETTINGS,
    KNOWLEDGE_BASE,
    PROJECTS,
    STUDY,
    ANALYTICS,
    TIMELINE,
    DEVICE_HEALTH,
    MEMORY_BROWSER,
    CONVERSATION_HISTORY,
    NOTIFICATION_CENTER,
    REMINDERS,
    DOCUMENTS,
    AUTOMATIONS,
    EXECUTIVE_BRIEF,
    COUNT
};

inline const char* screenIdToName(ScreenID id) noexcept {
    switch (id) {
        case ScreenID::DASHBOARD:             return "Dashboard";
        case ScreenID::ASSISTANT_CHAT:        return "Assistant";
        case ScreenID::SEARCH:                return "Search";
        case ScreenID::SETTINGS:              return "Settings";
        case ScreenID::KNOWLEDGE_BASE:        return "Knowledge";
        case ScreenID::PROJECTS:              return "Projects";
        case ScreenID::STUDY:                 return "Study";
        case ScreenID::ANALYTICS:             return "Analytics";
        case ScreenID::TIMELINE:              return "Timeline";
        case ScreenID::DEVICE_HEALTH:         return "Health";
        case ScreenID::MEMORY_BROWSER:        return "Memory";
        case ScreenID::CONVERSATION_HISTORY:  return "History";
        case ScreenID::NOTIFICATION_CENTER:   return "Notifications";
        case ScreenID::REMINDERS:             return "Reminders";
        case ScreenID::DOCUMENTS:             return "Documents";
        case ScreenID::AUTOMATIONS:           return "Automations";
        case ScreenID::EXECUTIVE_BRIEF:       return "Briefing";
        default:                              return "";
    }
}

struct UIScreen {
    ScreenID id;
    String   title;
    String   data;               // JSON payload with screen-specific data
    uint8_t  priority;           // Higher = more important
    bool     clearHistory;       // Clear navigation stack before pushing
    bool     animated;           // Use transition animation

    UIScreen() noexcept
        : id(ScreenID::NONE), priority(0), clearHistory(false), animated(true) {}
};

// ============================================================================
// Widget Types
// ============================================================================

enum class WidgetID : uint8_t {
    NONE = 255,
    CLOCK = 0,
    GREETING,
    WEATHER,
    STUDY_PROGRESS,
    PROJECTS,
    TIMELINE,
    NOTIFICATIONS,
    SEARCH_BAR,
    MEMORY_SNIPPET,
    KNOWLEDGE_SNIPPET,
    ANALYTICS_SUMMARY,
    WIFI_STATUS,
    STORAGE_STATUS,
    HEAP_STATUS,
    CPU_STATUS,
    ACTIVE_CONTEXT,
    ASSISTANT_STATUS,
    NEXT_REMINDER,
    COUNT
};

struct UIWidget {
    WidgetID   id;
    uint8_t    column;           // Grid column position
    uint8_t    row;              // Grid row position
    uint8_t    width;            // Width in grid units
    uint8_t    height;           // Height in grid units
    bool       visible;
    String     title;
    String     value;            // Primary display value
    String     secondary;        // Secondary information
    uint8_t    progress;         // 0-100 progress indicator
    String     icon;             // Icon identifier
    uint32_t   lastUpdated;      // Timestamp of last data change
    uint32_t   backgroundColor;  // ARGB color (for color renderers)
    uint32_t   foregroundColor;

    UIWidget() noexcept
        : id(WidgetID::COUNT), column(0), row(0), width(1), height(1),
          visible(true), progress(0), lastUpdated(0),
          backgroundColor(0), foregroundColor(0xFFFFFFFF) {}
};

// ============================================================================
// Notification Types
// ============================================================================

enum class NotificationPriority : uint8_t {
    PRIORITY_LOW,
    NORMAL,
    PRIORITY_HIGH,
    CRITICAL
};

struct UINotification {
    String              id;
    String              title;
    String              message;
    NotificationPriority priority;
    unsigned long       durationMs;       // 0 = persistent until dismissed
    unsigned long       timestamp;
    bool                dismissible;
    String              source;           // Module that created it
    String              action;           // Optional action identifier

    UINotification() noexcept
        : priority(NotificationPriority::NORMAL), durationMs(5000),
          timestamp(0), dismissible(true) {}
};

// ============================================================================
// Dialog Types
// ============================================================================

struct UIDialogButton {
    String label;
    String action;
    bool   isDefault;
    bool   isDestructive;

    UIDialogButton() noexcept : isDefault(false), isDestructive(false) {}
};

struct UIDialog {
    String                   id;
    String                   title;
    String                   message;
    std::vector<UIDialogButton> buttons;
    bool                     dismissible;

    UIDialog() noexcept : dismissible(true) {}
};

// ============================================================================
// Overlay Types
// ============================================================================

struct UIOverlay {
    String  id;
    String  content;           // JSON or text content
    uint8_t opacity;           // 0-255
    bool    dismissible;
    unsigned long durationMs;  // Auto-dismiss timeout (0 = manual)

    UIOverlay() noexcept : opacity(200), dismissible(true), durationMs(0) {}
};

// ============================================================================
// Input Types
// ============================================================================

enum class InputEventType : uint8_t {
    NONE,
    TAP,
    DOUBLE_TAP,
    LONG_PRESS,
    SWIPE_UP,
    SWIPE_DOWN,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    DRAG_START,
    DRAG_MOVE,
    DRAG_END,
    KEY_PRESS,
    KEY_RELEASE,
    VOICE_COMMAND,
    BUTTON_PRESS,
    ROTARY_ENCODER
};

enum class KeyCode : uint8_t {
    NONE = 0,
    ENTER,
    ESCAPE,
    BACKSPACE,
    TAB,
    SPACE,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    HOME,
    END,
    VOLUME_UP,
    VOLUME_DOWN,
    MUTE,
    CUSTOM
};

struct InputEvent {
    InputEventType type;
    uint16_t       x;               // Touch/mouse X position
    uint16_t       y;               // Touch/mouse Y position
    uint16_t       xEnd;            // End X for swipe/drag
    uint16_t       yEnd;            // End Y for swipe/drag
    KeyCode        keyCode;
    String         keyChar;         // Character for text input
    unsigned long  timestamp;
    String         source;          // "touch", "keyboard", "voice", etc.

    InputEvent() noexcept
        : type(InputEventType::NONE), x(0), y(0), xEnd(0), yEnd(0),
          keyCode(KeyCode::NONE), timestamp(0) {}
};

// ============================================================================
// Navigation Types
// ============================================================================

struct NavigationEvent {
    ScreenID from;
    ScreenID to;
    bool     animated;
    bool     replace;          // Replace current instead of push

    NavigationEvent() noexcept
        : from(ScreenID::NONE), to(ScreenID::NONE),
          animated(true), replace(false) {}
};

// ============================================================================
// Renderer Enumeration
// ============================================================================

enum class RendererType : uint8_t {
    NONE = 0,
    OLED,
    LCD,
    TOUCHSCREEN,
    WEB,
    MOBILE,
    DESKTOP,
    HEADLESS
};

#endif // AURA_UI_EVENT_TYPES_H