#ifndef AURA_ASSISTANT_OUTPUT_H
#define AURA_ASSISTANT_OUTPUT_H

#include <Arduino.h>

// ============================================================================
// Assistant output routing.
//
// Routes an assistant response to its independent output targets according to
// the user's Output settings:
//   * Companion App  - non-blocking WebSocket broadcast (drop-safe when the
//                      app is disconnected; reconnect handled by AsyncWebSocket)
//   * OLED display   - show the response text on the display
//
// Speaker (TTS) routing is deliberately handled by ConversationManager's
// state machine so playback order/state is preserved.
// ============================================================================

namespace AssistantOutput {

/**
 * @brief Route an assistant response to all enabled outputs.
 * @param text Assistant response text (raw, unescaped).
 * @note Never blocks AI processing. If the Companion App is disconnected the
 *       message is dropped safely and normal operation continues.
 */
void route(const String& text) noexcept;

}  // namespace AssistantOutput

#endif  // AURA_ASSISTANT_OUTPUT_H
