#ifndef SECRETS_H
#define SECRETS_H

// ============================================================================
// AURA AI Desktop Assistant - Single Source of Truth for All Secrets
// ============================================================================
// WARNING: DO NOT commit real keys or passwords to version control.
// Use environment variables, NVS (Preferences), or a local overrides file.
//
// To add a new secret:
//   1. Add the constant below
//   2. Use Secrets::CONSTANT_NAME everywhere in the codebase
//   3. Never hardcode the value in source files
// ============================================================================

namespace Secrets {

// ============================================================================
// AI SERVICES - Google API Keys
// ============================================================================
// These keys are loaded from NVS at runtime. The compile-time values here
// serve as defaults only. Configure via Web Portal > Settings.
// ============================================================================

constexpr char GEMINI_API_KEY[]    = "";
constexpr char GOOGLE_STT_API_KEY[] = "";
constexpr char GOOGLE_TTS_API_KEY[] = "";

// ============================================================================
// NETWORK - Access Point (captive portal fallback)
// ============================================================================
// Used when no Wi-Fi credentials are stored. Web portal starts in AP mode
// with these credentials for initial device setup.
// ============================================================================

constexpr char AP_SSID[]           = "AURA_Setup";
constexpr char AP_PASSWORD[]       = "";

// ============================================================================
// WEB PORTAL - Default Admin Credentials
// ============================================================================
// Used only on first boot before any credentials are saved to NVS.
// When empty, unique credentials are generated at runtime from the device
// MAC address (see generateDefaultPassword() helpers in system_manager
// and web_portal).
// The user should change these immediately via the web portal settings.
// ============================================================================

constexpr char WEB_USERNAME[]      = "admin";
constexpr char WEB_PASSWORD[]      = "";

// ============================================================================
// OTA - Update Server Authentication
// ============================================================================
// HTTP Basic Auth credentials for the OTA firmware update server.
// Set at runtime via OtaManager::setAuthentication().
// ============================================================================

constexpr char OTA_USERNAME[]      = "";
constexpr char OTA_PASSWORD[]      = "";

// ============================================================================
// FUTURE SERVICES - Placeholder Keys
// ============================================================================
// Reserved for future API integrations. Empty by default.
// Populate when the corresponding service is implemented.
// ============================================================================

constexpr char WEATHER_API_KEY[]   = "";
constexpr char NEWS_API_KEY[]      = "";
constexpr char MQTT_USERNAME[]     = "";
constexpr char MQTT_PASSWORD[]     = "";
constexpr char MQTT_BROKER_URL[]   = "";
constexpr char WEBHOOK_URL[]       = "";
constexpr char OAUTH_CLIENT_ID[]   = "";
constexpr char OAUTH_CLIENT_SECRET[] = "";

}  // namespace Secrets

#endif  // SECRETS_H
