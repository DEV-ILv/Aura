#include "assistant_output.h"

#include <time.h>

#include "web_portal.h"
#include "settings_manager.h"
#include "display_manager.h"

namespace {

// ============================================================================
// Escape a string for safe embedding inside a JSON string value.
// ============================================================================
String escapeJson(const String& s) noexcept {
    String out;
    out.reserve(s.length() + 16);
    for (unsigned i = 0; i < s.length(); ++i) {
        const char c = s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) out += ' ';  // strip other control characters
                else out += c;
                break;
        }
    }
    return out;
}

// ============================================================================
// Current local time as an ISO8601 timestamp (fallback if clock not synced).
// ============================================================================
String iso8601Now() noexcept {
    time_t now = time(nullptr);
    struct tm t;
    if (getLocalTime(&t, 1)) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
        return String(buf);
    }
    return "1970-01-01T00:00:00";
}

}  // namespace

namespace AssistantOutput {

void route(const String& text) noexcept {
    if (text.isEmpty()) return;

    // --- Companion App (WebSocket, non-blocking, drop-safe) ---
    if (settingsManager.isInitialized() && settingsManager.getOutputToCompanion()) {
        String json;
        json.reserve(96 + text.length());
        json = "{\"type\":\"assistant_response\",\"timestamp\":\"";
        json += iso8601Now();
        json += "\",\"source\":\"AURA\",\"text\":\"";
        json += escapeJson(text);
        json += "\"}";
        webPortal.broadcastAssistantResponse(json);
    }

    // --- OLED display (independent toggle) ---
    if (settingsManager.isInitialized() && settingsManager.getOutputToOled()) {
        displayManager.showMessage("Response", text, "");
    }
}

}  // namespace AssistantOutput
