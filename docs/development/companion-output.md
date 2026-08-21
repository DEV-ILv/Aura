# Companion App Output (Assistant Responses)

How the AURA device streams assistant responses to the Flutter Companion app over
WebSocket, and what the Companion app must implement to receive them.

Firmware side is implemented and gated by the **Companion App** toggle in
`Settings -> Output`. This document is the integration spec for the Flutter
Companion app (a separate repo).

## Transport

- Protocol: WebSocket (the ESP32 uses `ESP_Async_WebServer`/`AsyncWebSocket`).
- URI: `ws://<device-ip>:81`
- The device IP is the one assigned by Wi-Fi (HTTP portal runs on port `80`).

## Authentication handshake

The WebSocket server requires a valid session token before it will send anything
to a client. Unauthenticated clients are disconnected immediately.

1. Obtain a session token by logging in over HTTP:

   ```
   POST http://<device-ip>/api/auth/login
   Content-Type: application/json

   { "username": "<user>", "password": "<pass>" }
   ```

   A successful response returns the session token (field `token`).

2. Connect to `ws://<device-ip>:81`.

3. Immediately send the first message:

   ```json
   { "type": "auth", "token": "<session-token>" }
   ```

4. On success the device responds with dashboard + module-status snapshot
   messages. On failure the server sends
   `{"type":"error","message":"unauthorized"}` and closes the connection.

> Non-browser clients (like Flutter) must perform this token handshake; browser
> clients additionally must send a matching `Origin`. The Flutter app should
> implement the token handshake so it is not treated as an unauthenticated client.

## Assistant response message

When the assistant produces a reply (Gemini or offline fallback) and the
**Companion App** output toggle is on, the device broadcasts a message of the form:

```json
{
  "type": "assistant_response",
  "timestamp": "2026-08-05T14:23:09",
  "source": "AURA",
  "text": "The reply text, already JSON-escaped."
}
```

Field notes:

- `timestamp` is local device time in ISO8601 (`YYYY-MM-DDTHH:MM:SS`). If the
  device clock is not yet synchronized it sends the sentinel `1970-01-01T00:00:00`.
- `source` is always `AURA`.
- `text` is the final assistant reply (no streaming deltas).
- The broadcast is **non-blocking and drop-safe**: if no Companion client is
  connected, the message is silently dropped (it is not reproducible later, so a
  connected app should be the expected state when this feature is in use).

## Flutter app requirements (Assistant Conversation page)

Add an "Assistant" / "Chat" page to the Companion app that shows the live stream:

- **Connectivity**
  - Maintain the WebSocket connection to `ws://<host>:81`.
  - Run the auth handshake on connect and after every reconnect.
  - Auto-reconnect with backoff (e.g. `reconnect_tries: -1`, `reconnect_delay`),
    and re-run the token handshake after each reconnect. The device's
    AsyncWebSocket handles the reconnect gracefully; the app drives reconnecting.
  - Show a clear disconnected state and a retry affordance.
- **Message rendering**
  - Render only messages with `type == "assistant_response"`.
  - Show `text` in a chat bubble (assistant side), with `timestamp` as a
    secondary time label.
  - Auto-scroll to the newest message when a new one arrives (unless the user has
    scrolled up — then show a "new messages" pill).
- **User features**
  - Copy the message text on long-press / a copy icon.
  - Clear the conversation (clear + confirm).
  - Search within the conversation feed.
  - Persist the conversation locally (e.g. `sqflite`/`hive`) and support export
    (share as text file) and delete.
  - No remote/Supabase sync in this phase (firmware does not push to the cloud);
    the device processes and the app displays. Cloud sync is a future step.
- **Toggles**
  - The Companion output can be independently enabled/disabled on the device via
    `Settings -> Output -> Companion App` on the web portal
    (`api/settings` `output_companion`). The app should surface a friendly hint if
    no messages arrive on a healthy connection.

## Reference

- Firmware WebSocket port: `WS_PORT = 81` (`config.h`).
- Broadcast entry point: `AssistantOutput::route()` (`assistant_output.cpp`).
- Message broadcast: `WebPortal::broadcastAssistantResponse()`.
- Auth handshake: `WebPortal::handleWebSocketAuthMessage()` (`web_portal.cpp`).