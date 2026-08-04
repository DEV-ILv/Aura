#ifndef AURA_WEB_PORTAL_H
#define AURA_WEB_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <map>
#include <mbedtls/sha256.h>

#include "config.h"
#include "logger.h"
#include "wifi_manager.h"
#include "secrets.h"
#include "tiny_ai_manager.h"

/**
 * @class WebPortal
 * @brief Comprehensive web interface module for AURA AI Desktop Assistant.
 *
 * Provides:
 * - Captive portal for initial Wi-Fi configuration
 * - Device dashboard and system information display
 * - RESTful API for device control and status
 * - OTA firmware update interface
 * - Network diagnostics and device management
 * - Secure web-based settings configuration
 *
 * @note Non-blocking architecture - all operations are event-driven.
 * @note Thread-safe for FreeRTOS environment.
 * @note Production-quality with full error handling.
 */
class WebPortal
{
public:
    /**
     * @brief Default constructor.
     *
     * Initializes WebPortal with default configuration.
     * Does not start the server - call initialize() and start().
     */
    WebPortal() noexcept;

    /**
     * @brief Destructor.
     *
     * Stops the web server and cleans up resources.
     */
    ~WebPortal() noexcept;

    // Delete copy semantics
    WebPortal(const WebPortal&) = delete;
    WebPortal& operator=(const WebPortal&) = delete;

    // Delete move semantics
    WebPortal(WebPortal&&) = delete;
    WebPortal& operator=(WebPortal&&) = delete;

    /**
     * @brief Initializes the WebPortal module.
     *
     * Sets up internal structures, configures routes, and prepares
     * the web server for operation.
     *
     * @return true if initialization succeeded, false otherwise.
     *
     * @note Must be called before start().
     * @note Idempotent - safe to call multiple times.
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Starts the web server.
     *
     * Begins listening for HTTP requests on the configured port.
     * Optionally begins captive portal if requested.
     *
     * @return true if server started successfully, false otherwise.
     *
     * @note initialize() must be called first.
     * @note Non-blocking operation.
     */
    [[nodiscard]] bool start() noexcept;

    /**
     * @brief Stops the web server.
     *
     * Stops listening for HTTP requests and frees resources.
     *
     * @return true if server stopped successfully, false otherwise.
     *
     * @note Safe to call even if not started.
     * @note Non-blocking operation.
     */
    [[nodiscard]] bool stop() noexcept;

    /**
     * @brief Updates the WebPortal.
     *
     * Must be called regularly from the main loop to:
     * - Handle incoming HTTP requests
     * - Process captive portal DNS
     * - Update client activity tracking
     * - Manage timeout conditions
     *
     * @note Non-blocking - returns quickly.
     * @note Safe to call from FreeRTOS task.
     *
     * @see handleClient()
     */
    void update() noexcept;

    /**
     * @brief Processes incoming HTTP requests.
     *
     * Handles one pending client request. Called by update().
     *
     * @note Non-blocking - processes only one client per call.
     * @note May be called from interrupt context.
     */
    void handleClient() noexcept;

    /**
     * @brief Checks if the web server is currently running.
     *
     * @return true if server is active and listening, false otherwise.
     *
     * @note [[nodiscard]] - caller should check the result.
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Activates the captive portal.
     *
     * Starts DNS spoofing to intercept all DNS requests and redirect
     * to the device's IP address for initial Wi-Fi configuration.
     *
     * @return true if captive portal activated successfully, false otherwise.
     *
     * @note Typically used during initial setup or Wi-Fi disconnection.
     * @note start() must be called first.
     */
    [[nodiscard]] bool beginCaptivePortal() noexcept;

    /**
     * @brief Deactivates the captive portal.
     *
     * Stops DNS spoofing and captive portal functionality.
     *
     * @return true if captive portal deactivated successfully, false otherwise.
     *
     * @note Safe to call even if captive portal not active.
     */
    [[nodiscard]] bool stopCaptivePortal() noexcept;

    /**
     * @brief Registers standard HTTP routes.
     *
     * Sets up handlers for:
     * - Root / (dashboard)
     * - /status (device status)
     * - /wifi (Wi-Fi configuration)
     * - /settings (device settings)
     * - /restart (device restart)
     * - /factoryReset (factory reset)
     * - /ota (firmware update)
     * - 404 Not Found
     *
     * @return true if all routes registered successfully, false otherwise.
     *
     * @note Called during initialize().
     */
    [[nodiscard]] bool registerRoutes() noexcept;

    /**
     * @brief Registers RESTful API routes.
     *
     * Sets up JSON API endpoints for:
     * - /api/status (GET device status)
     * - /api/wifi (GET/POST Wi-Fi info)
     * - /api/settings (GET/POST device settings)
     * - /api/system (GET system information)
     * - /api/network (GET network diagnostics)
     *
     * @return true if all API routes registered successfully, false otherwise.
     *
     * @note Called during initialize().
     */
    [[nodiscard]] bool registerApiRoutes() noexcept;

    /**
     * @brief Restarts the device.
     *
     * Triggers a system reset after a brief delay to allow
     * the HTTP response to be sent.
     *
     * @note Non-blocking - schedules restart via FreeRTOS.
     * @note Device will restart within 1-2 seconds.
     */
    void restartDevice() noexcept;

    /**
     * @brief Performs factory reset.
     *
     * Resets all settings to factory defaults and erases stored configuration.
     * Device will automatically restart after reset completes.
     *
     * @note Non-blocking - schedules reset via FreeRTOS.
     * @note All user settings will be lost.
     * @note Device will restart within 1-2 seconds.
     */
    void factoryReset() noexcept;

    /**
     * @brief Retrieves the HTTP server port.
     *
     * @return Server port number (typically 80).
     *
     * @note [[nodiscard]] - caller should check the result.
     */
    [[nodiscard]] uint16_t getPort() const noexcept;

    /**
     * @brief Retrieves the device IP address.
     *
     * @return IP address as string (e.g., "192.168.1.100").
     *
     * @note Returns empty string if not connected.
     * @note [[nodiscard]] - caller should check the result.
     */
    [[nodiscard]] String getIPAddress() const noexcept;

    /**
     * @brief Retrieves the device hostname.
     *
     * @return Hostname string (e.g., "aura-device").
     *
     * @note [[nodiscard]] - caller should check the result.
     */
    [[nodiscard]] String getHostname() const noexcept;

private:
    // Private helper methods

    /**
     * @brief Handles root page request (/).
     *
     * Serves the main dashboard HTML.
     */
    void handleRoot() noexcept;

    /**
     * @brief Handles /status page request.
     *
     * Displays device status information.
     */
    void handleStatus() noexcept;

    /**
     * @brief Handles /wifi page request.
     *
     * Displays Wi-Fi configuration interface.
     */
    void handleWifi() noexcept;

    /**
     * @brief Handles Wi-Fi configuration form submission.
     *
     * Processes POST request to save Wi-Fi credentials.
     */
    void handleSaveWifi() noexcept;

    /**
     * @brief Handles /settings page request.
     *
     * Displays device settings interface.
     */
    void handleSettings() noexcept;

    /**
     * @brief Handles settings form submission.
     *
     * Processes POST request to save device settings.
     */
    void handleSaveSettings() noexcept;

    /**
     * @brief Handles /restart request.
     *
     * Displays restart confirmation page.
     */
    void handleRestart() noexcept;

    /**
     * @brief Handles /factoryReset request.
     *
     * Displays factory reset confirmation page.
     */
    void handleFactoryReset() noexcept;

    /**
     * @brief Handles /ota firmware update request.
     *
     * Displays OTA update interface.
     */
    void handleOTA() noexcept;

    /**
     * @brief Handles 404 Not Found errors.
     *
     * Returns appropriate error response for unknown routes.
     */
    void handleNotFound() noexcept;

    /**
     * @brief Handles /api/status REST endpoint.
     *
     * Returns device status as JSON.
     */
    void handleApiStatus() noexcept;

    /**
     * @brief Handles /api/wifi REST endpoint.
     *
     * Returns or updates Wi-Fi information as JSON.
     */
    void handleApiWifi() noexcept;

    /**
     * @brief Handles /api/settings REST endpoint.
     *
     * Returns or updates device settings as JSON.
     */
    void handleApiSettings() noexcept;

    // ========================================================================
    // New Feature API Handlers
    // ========================================================================

    void handleApiPlugins() noexcept;
    void handleApiPluginEnable() noexcept;
    void handleApiPluginDisable() noexcept;
    void handleApiSkills() noexcept;
    void handleApiSkillAdd() noexcept;
    void handleApiSkillDelete() noexcept;
    void handleApiSkillEnable() noexcept;
    void handleApiSkillDisable() noexcept;
    void handleApiPersonality() noexcept;
    void handleApiPersonalityActive() noexcept;
    void handleApiContext() noexcept;
    void handleApiPerformance() noexcept;
    void handleApiDiagnostics() noexcept;
    void handleApiCrashes() noexcept;
    void handleApiCrashAck() noexcept;
    void handleApiCrashClear() noexcept;
    void handleApiSummaries() noexcept;
    void handleApiSummaryToday() noexcept;
    void handleApiSummaryDelete() noexcept;
    void handleApiMemoriesRanked() noexcept;
    void handleApiMemoriesSearch() noexcept;
    void handleApiMemoriesMaintenance() noexcept;

    // V2.0 Feature API Handlers
    void handleApiKnowledgeGraph() noexcept;
    void handleApiKnowledgeGraphNode() noexcept;
    void handleApiKnowledgeGraphEdge() noexcept;
    void handleApiKnowledgeGraphTraverse() noexcept;
    void handleApiGoals() noexcept;
    void handleApiGoalCreate() noexcept;
    void handleApiGoalDelete() noexcept;
    void handleApiGoalUpdate() noexcept;
    void handleApiHabits() noexcept;
    void handleApiHabitCreate() noexcept;
    void handleApiHabitDelete() noexcept;
    void handleApiHabitToggle() noexcept;
    void handleApiPlanner() noexcept;
    void handleApiPlannerTask() noexcept;
    void handleApiPlannerSuggest() noexcept;
    void handleApiAutomations() noexcept;
    void handleApiAutomationCreate() noexcept;
    void handleApiAutomationDelete() noexcept;
    void handleApiAutomationToggle() noexcept;
    void handleApiReflections() noexcept;
    void handleApiReflectionRun() noexcept;
    void handleApiFunctions() noexcept;
    void handleApiFunctionExecute() noexcept;

    // Auth handlers
    void handleApiAuthLogin() noexcept;
    void handleApiAuthLogout() noexcept;
    void handleApiAuthStatus() noexcept;
    void handleApiAuthChangePassword() noexcept;

    // Startup greeting
    void handleApiStartupSettings() noexcept;

    // Offline AI
    void handleApiOfflineAIStatus() noexcept;
    void handleApiOfflineAIEnable() noexcept;
    void handleApiOfflineAIDisable() noexcept;
    void handleApiOfflineAITest() noexcept;

    // Context
    void handleApiContextHistory() noexcept;

    // Timeline
    void handleApiTimeline() noexcept;
    void handleApiTimelineSearch() noexcept;

    // Briefing
    void handleApiBriefingMorning() noexcept;
    void handleApiBriefingEvening() noexcept;

    // Semantic Search
    void handleApiSemanticSearch() noexcept;

    // Graph Relationships
    void handleApiGraphRelationships() noexcept;

    // V2.1 Intelligence Layer API handlers
    void handleApiDecisions() noexcept;
    void handleApiDecisionMake() noexcept;
    void handleApiDecisionExplain() noexcept;
    void handleApiDecisionRank() noexcept;

    void handleApiLearning() noexcept;
    void handleApiLearningObserve() noexcept;
    void handleApiPatterns() noexcept;

    void handleApiRecommendations() noexcept;
    void handleApiRecommendDismiss() noexcept;
    void handleApiRecommendAct() noexcept;
    void handleApiRecommendGenerate() noexcept;

    void handleApiPredictions() noexcept;
    void handleApiPredictionRun() noexcept;

    void handleApiDocuments() noexcept;
    void handleApiDocumentUpload() noexcept;
    void handleApiDocumentDelete() noexcept;
    void handleApiDocumentContent() noexcept;
    void handleApiDocumentSearch() noexcept;

    void handleApiWorkspaces() noexcept;
    void handleApiWorkspaceCreate() noexcept;
    void handleApiWorkspaceDelete() noexcept;
    void handleApiWorkspaceActivate() noexcept;
    void handleApiWorkspaceMember() noexcept;

    void handleApiVault() noexcept;
    void handleApiVaultSet() noexcept;
    void handleApiVaultDelete() noexcept;
    void handleApiVaultBackup() noexcept;

    void handleApiDeveloper() noexcept;
    void handleApiDeveloperExport() noexcept;

    // WebSocket handlers
    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) noexcept;
    void webSocketBroadcast(String json) noexcept;
    void webSocketBroadcastDashboard() noexcept;
    void webSocketBroadcastModuleStatus() noexcept;

    // V3.1 Wake Word API handlers
    void handleApiWakeWordStatus() noexcept;
    void handleApiWakeWordSettings() noexcept;
    void handleApiWakeWordPhrases() noexcept;
    void handleApiWakeWordAddPhrase() noexcept;
    void handleApiWakeWordRemovePhrase() noexcept;
    void handleApiWakeWordResetStats() noexcept;
    void handleApiWakeWordCalibrate() noexcept;

    // V3.1 ESP-NOW API handlers
    void handleApiEspNowStatus() noexcept;
    void handleApiEspNowNodes() noexcept;
    void handleApiEspNowPair() noexcept;
    void handleApiEspNowUnpair() noexcept;
    void handleApiEspNowDiscovery() noexcept;

    // V3.1 Dashboard API handlers
    void handleApiDashboardSummary() noexcept;
    void handleApiDashboardRecent() noexcept;
    void handleApiMemoryPin() noexcept;
    void handleApiMemoryArchive() noexcept;
    void handleApiMemoryPinned() noexcept;
    void handleApiMemoryArchived() noexcept;
    void handleApiMemoryRevisions() noexcept;
    void handleApiMemoryRestore() noexcept;
    void handleApiMemoryCompare() noexcept;
    void handleApiSkillsStudioUpdate() noexcept;
    void handleApiSkillsStudioDuplicate() noexcept;
    void handleApiSkillsStudioExport() noexcept;
    void handleApiSkillsStudioImport() noexcept;
    void handleApiAutomationNLPatterns() noexcept;
    void handleApiAutomationNLMatch() noexcept;
    void handleApiStudySubjects() noexcept;
    void handleApiStudySessions() noexcept;
    void handleApiStudyFlashcards() noexcept;
    void handleApiStudyStats() noexcept;
    void handleApiCompanionDevices() noexcept;
    void handleApiCompanionMessages() noexcept;
    void handleApiPluginsMarketplace() noexcept;
    void handleApiPluginsMarketplaceRegister() noexcept;

    // Version
    void handleApiVersion() noexcept;

    // Auth helpers
    bool isAuthenticated() noexcept;
    bool isAuthenticatedOrReject() noexcept;
    String generateSessionToken() noexcept;
    String generateRandomPassword() noexcept;
    bool secureEquals(const String& a, const String& b) noexcept;
    bool loadAuthCredentials() noexcept;
    bool saveAuthCredentials(const String& username, const String& password) noexcept;
    void sendToAuthenticatedClients(String json) noexcept;
    void handleWebSocketAuthMessage(uint8_t num, const String& msg) noexcept;
    bool verifyWebOtaSignature(const uint8_t hash[32], const String& sigHex) noexcept;
    void resetLoginTracker(const String& ip) noexcept;
    bool isLoginLockedOut(const String& ip) noexcept;
    void recordFailedLogin(const String& ip) noexcept;

    /**
     * @brief Serves the SPA entry point (index.html).
     *
     * Reads /index.html from SPIFFS and sends it with the correct
     * Content-Type. Falls back to an inline minimal page if the file
     * is not found.
     */
    void serveSPA() noexcept;

    /**
     * @brief Serves a static file from SPIFFS.
     *
     * Reads the given path from SPIFFS and sends it with the
     * appropriate MIME type based on file extension.
     *
     * @param path File path to serve (e.g. "/css/theme.css").
     */
    void serveStaticFile(const String& path) noexcept;

    /**
     * @brief Sends JSON response.
     *
     * Sets appropriate content-type and sends JSON formatted response.
     *
     * @param json JSON formatted string.
     * @param code HTTP status code (default 200).
     */
    void sendJson(const String& json, const int code = 200) noexcept;

    /**
     * @brief Sends success response.
     *
     * Sends JSON success message with optional data.
     *
     * @param message Success message.
     * @param data Optional additional data.
     */
    void sendSuccess(const String& message, const String& data = "") noexcept;

    /**
     * @brief Sends error response.
     *
     * Sends JSON error message with appropriate HTTP status code.
     *
     * @param message Error message.
     * @param code HTTP error code (default 400).
     */
    void sendError(const String& message, const int code = 400) noexcept;

    // Private member variables

    /// WebServer instance for HTTP handling
    WebServer m_server;

    /// WebSocket server instance for live dashboard
    WebSocketsServer m_webSocket;

    /// DNSServer instance for captive portal DNS
    DNSServer m_dnsServer;

    /// HTTP server port (typically 80)
    static constexpr uint16_t m_port{80};

    /// Server running state flag
    bool m_running{false};

    /// Captive portal active state flag
    bool m_captivePortalActive{false};

    /// OTA update in progress flag
    bool m_otaInProgress{false};

    /// Timestamp of last client activity (milliseconds)
    unsigned long m_lastClientActivity{0};

    /// Total number of HTTP requests processed
    uint32_t m_requestCounter{0};

    /// Client inactivity timeout (milliseconds)
    static constexpr unsigned long m_clientTimeout{300000UL};  // 5 minutes

    /// DNS server instance state
    static constexpr uint16_t m_dnsPort{53};

    // WebSocket state
    unsigned long m_lastWsPublish;
    unsigned long m_lastWsPing;

    // WebSocket per-client authentication state (WS_MAX_CLIENTS from config.h)
    bool m_wsAuth[WS_MAX_CLIENTS];

    // Optional Web-OTA signature verification state
    mbedtls_sha256_context m_otaSha256;
    bool m_otaHashing;

    // Auth state
    String m_authUsername;
    String m_authPassword;
    bool m_authMustChange{false};
    String m_sessionToken;
    unsigned long m_sessionCreated{0};
    Preferences m_authPrefs;
    static constexpr unsigned long kSessionTimeoutMs{3600000UL};  // 1 hour
    static constexpr const char* kAuthNamespace{"auraauth"};
    static constexpr uint32_t kAuthCredVersion{3};  // bump to re-migrate auth NVS layout
#if AURA_DEVELOPMENT_MODE
    static constexpr const char* kDefaultUsername{AURA_DEV_WEB_USERNAME};
#else
    static constexpr const char* kDefaultUsername{Secrets::WEB_USERNAME};
#endif

    // Per-IP login rate limiting
    struct LoginTracker {
        uint8_t attempts{0};
        unsigned long lockoutUntil{0};
    };
    std::map<String, LoginTracker> m_loginTrackers;
    static constexpr uint8_t kMaxLoginAttempts{5};
    static constexpr unsigned long kLoginLockoutDurationMs{30000UL};
    static constexpr size_t kMaxTrackedIps{32};
};

/**
 * @brief Global web portal instance
 */
extern WebPortal webPortal;

#endif // AURA_WEB_PORTAL_H