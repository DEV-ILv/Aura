#include "web_portal.h"
#include <uri/UriGlob.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <functional>
#include <new>
#include <ArduinoJson.h>
#include <mbedtls/pk.h>
#include "service_status_manager.h"
#include "storage_manager.h"
#include "audio_manager.h"
#include "display_manager.h"
#include "settings_manager.h"
#include "led_ring.h"
#include "aura_system.h"
#include "aura_mood.h"
#include "conversation_manager.h"
#include "esp_now_manager.h"
#include "plugin_manager.h"
#include "skill_manager.h"
#include "personality_manager.h"
#include "context_manager.h"
#include "security_manager.h"
#include "firmware_keys.h"

#include "performance_manager.h"
#include "crash_manager.h"
#include "uptime_monitor.h"
#include "diagnostics_manager.h"
#include "error_manager.h"
#include "memory_manager.h"
#include "knowledge_graph_manager.h"
#include "goal_manager.h"
#include "habit_manager.h"
#include "planner_manager.h"
#include "automation_manager.h"
#include "reflection_manager.h"
#include "gemini_client.h"
#include "function_router.h"
#include "startup_greeting_manager.h"
#include "timeline_manager.h"
#include "briefing_manager.h"
#include "semantic_search_manager.h"
#include "decision_manager.h"
#include "learning_manager.h"
#include "executive_assistant.h"
#include "prediction_manager.h"
#include "document_manager.h"
#include "workspace_manager.h"
#include "vault_manager.h"
#include "event_bus.h"
#include "study_manager.h"
#include "companion_manager.h"

namespace {
// Defined later in this translation unit; declared here for earlier call sites.
const char* conversationStateToString(const ConversationState state) noexcept;
}
#include "reminder_manager.h"
#include "json_helpers.h"

/// Global WebPortal instance
WebPortal webPortal;

// ============================================================================
// Constants
// ============================================================================

static constexpr unsigned long RESTART_DELAY_MS{1000};
static constexpr unsigned long FACTORY_RESET_DELAY_MS{1000};
static constexpr unsigned long OTA_RESTART_DELAY_MS{2000};
static constexpr size_t MAX_JSON_BUFFER{512};
static constexpr size_t HTML_BUFFER_JSON{256};

// ============================================================================
// Forward Declarations for Private Task Functions
// ============================================================================

static void restartTask(void* pvParameters);
static void factoryResetTask(void* pvParameters);
static void otaRestartTask(void* pvParameters);

// ============================================================================
// Constructor / Destructor
// ============================================================================

WebPortal::WebPortal() noexcept
    : m_server(m_port),
      m_webSocket(WS_PORT),
      m_dnsServer(),
      m_running(false),
      m_captivePortalActive(false),
      m_otaInProgress(false),
      m_lastClientActivity(0),
      m_requestCounter(0),
      m_lastWsPublish(0),
      m_lastWsPing(0),
      m_otaHashing(false)
{
    for (size_t i = 0; i < WS_MAX_CLIENTS; ++i) {
        m_wsAuth[i] = false;
    }
    mbedtls_sha256_init(&m_otaSha256);
}

WebPortal::~WebPortal() noexcept
{
    stop();
    stopCaptivePortal();
}

// ============================================================================
// Public API - Lifecycle
// ============================================================================

bool WebPortal::initialize() noexcept
{
    Logger::info("WebPortal", "Initializing web portal");

    try
    {
        if (!registerRoutes())
        {
            Logger::error("WebPortal", "Failed to register standard routes");
            return false;
        }

        if (!registerApiRoutes())
        {
            Logger::error("WebPortal", "Failed to register API routes");
            return false;
        }
    }
    catch (const std::bad_alloc&)
    {
        Logger::error("WebPortal", "Heap exhausted while registering routes; continuing with partial routes");
    }

    loadAuthCredentials();
    Logger::info("WebPortal", "Auth credentials loaded");

    Logger::info("WebPortal", "Web portal initialized successfully");
    return true;
}

bool WebPortal::start() noexcept
{
    if (m_running)
    {
        Logger::warning("WebPortal", "Server already running");
        return true;
    }

    // Register the auth token header with the (ESP32 core) WebServer. Without
    // this, _collectHeader() skips every non-listed header, so isAuthenticated()
    // can never see "X-Auth-Token" and every authenticated route returns 401.
    const char* authHeaderKeys[] = { "X-Auth-Token" };
    m_server.collectHeaders(authHeaderKeys, 1);

    m_server.begin();
    try
    {
        m_webSocket.begin();
    }
    catch (const std::bad_alloc&)
    {
        Logger::error("WebPortal", "Heap exhausted starting WebSocket server; HTTP only");
    }
    m_webSocket.onEvent(std::bind(&WebPortal::handleWebSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

    // Reject cross-origin WebSocket handshakes (CSRF protection). Browser
    // clients must send an Origin that matches this device's own address
    // (LAN IP, AP IP, or configured hostname). Non-browser clients that omit
    // Origin are still required to pass the token-based auth message before
    // any telemetry is sent (see handleWebSocketAuthMessage).
    static const char* noMandatoryHeaders[] = { nullptr };
    m_webSocket.onValidateHttpHeader(
        [this](String headerName, String headerValue) -> bool {
            if (headerName.equalsIgnoreCase("origin")) {
                String hostPart = headerValue;
                int scheme = hostPart.indexOf("://");
                if (scheme >= 0) hostPart = hostPart.substring(scheme + 3);
                int portPos = hostPart.indexOf(':');
                if (portPos >= 0) hostPart = hostPart.substring(0, portPos);

                String hostname = WiFi.getHostname();
                bool originOk =
                    (hostPart == WiFi.localIP().toString()) ||
                    (hostPart == WiFi.softAPIP().toString()) ||
                    (hostPart == hostname) ||
                    (hostPart == hostname + ".local");

                if (!originOk) {
                    Logger::warning("WebPortal", "WebSocket rejected: cross-origin handshake (origin=%s)",
                        headerValue.c_str());
                    return false;
                }
            }
            return true;
        },
        noMandatoryHeaders, 0);

    m_running = true;
    m_lastClientActivity = millis();

    Logger::info("WebPortal", "Web server started on port 80, WebSocket on port %d", WS_PORT);
    return true;
}

bool WebPortal::stop() noexcept
{
    if (!m_running)
    {
        return true;
    }

    m_server.stop();
    m_webSocket.close();
    m_running = false;

    Logger::info("WebPortal", "Web server stopped");
    return true;
}

void WebPortal::update() noexcept
{
    if (!m_running)
    {
        return;
    }

    if (m_captivePortalActive)
    {
        m_dnsServer.processNextRequest();
    }

    handleClient();

    // WebSocket event loop
    m_webSocket.loop();

    // Periodic dashboard broadcast to WebSocket clients
    unsigned long now = millis();
    if (now - m_lastWsPublish >= WS_PUBLISH_INTERVAL_MS) {
        m_lastWsPublish = now;
        webSocketBroadcastDashboard();
    }

    // Broadcast module status changes (headless detection, online/offline)
    webSocketBroadcastModuleStatus();

    // Push one-shot WebSocket alerts for new diagnostic ERROR/CRITICAL events
    if (errorManager.hasPendingBroadcast()) {
        webSocketBroadcast(errorManager.takeBroadcastJson());
    }

    // Periodic WebSocket ping to keep alive
    if (now - m_lastWsPing >= WS_PING_INTERVAL_MS) {
        m_lastWsPing = now;
        m_webSocket.broadcastPing();
    }

    m_lastClientActivity = millis();
}

void WebPortal::handleClient() noexcept
{
    if (!m_running)
    {
        return;
    }

    m_server.handleClient();
}

bool WebPortal::isRunning() const noexcept
{
    return m_running;
}

// ============================================================================
// Captive Portal
// ============================================================================

bool WebPortal::beginCaptivePortal() noexcept
{
    if (m_captivePortalActive)
    {
        Logger::warning("WebPortal", "Captive portal already active");
        return true;
    }

    if (!m_running)
    {
        Logger::error("WebPortal", "Cannot start captive portal: server not running");
        return false;
    }

    IPAddress apIP = WiFi.softAPIP();
    if (!m_dnsServer.start(m_dnsPort, "*", apIP))
    {
        Logger::error("WebPortal", "Failed to start DNS server");
        return false;
    }

    m_captivePortalActive = true;
    Logger::info("WebPortal", "Captive portal activated");
    return true;
}

bool WebPortal::stopCaptivePortal() noexcept
{
    if (!m_captivePortalActive)
    {
        return true;
    }

    m_dnsServer.stop();
    m_captivePortalActive = false;

    Logger::info("WebPortal", "Captive portal deactivated");
    return true;
}

// ============================================================================
// Route Registration
// ============================================================================

bool WebPortal::registerRoutes() noexcept
{
    m_server.on("/", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/status", HTTP_GET, [this]() { handleStatus(); });
    m_server.on("/wifi", HTTP_GET, [this]() { handleWifi(); });
    m_server.on("/wifi", HTTP_POST, [this]() { handleSaveWifi(); });
    m_server.on("/settings", HTTP_GET, [this]() { handleSettings(); });
    m_server.on("/settings", HTTP_POST, [this]() { handleSaveSettings(); });
    m_server.on("/restart", HTTP_POST, [this]() { handleRestart(); });
    m_server.on("/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
    m_server.on("/ota", HTTP_GET, [this]() { handleOTA(); });
    m_server.on("/ota", HTTP_POST, [this]() { handleOTA(); });

    // New feature SPA pages
    m_server.on("/plugins", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/skills", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/personality", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/performance", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/diagnostics", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/crashes", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/daily-summary", HTTP_GET, [this]() { handleRoot(); });

    // V2.0 Feature SPA pages
    m_server.on("/knowledge-graph", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/goals", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/habits", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/planner", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/automation", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/reflection", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/functions", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/startup-greeting", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/context", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/timeline", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/daily-briefing", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/semantic-search", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/relationship-explorer", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/offline-ai", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/decision-center", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/learning", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/recommendations", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/predictions", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/documents", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/workspaces", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/vault", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/developer", HTTP_GET, [this]() { handleRoot(); });

    // V3.0 Feature SPA pages
    m_server.on("/dashboard", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/memory-explorer", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/skill-studio", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/study", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/companion", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/plugins/marketplace", HTTP_GET, [this]() { handleRoot(); });

    // V3.1 Feature SPA pages
    m_server.on("/voice-settings", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/espnow-network", HTTP_GET, [this]() { handleRoot(); });

    // Captive portal common endpoints
    m_server.on("/generate_204", HTTP_GET, [this]() { m_server.send(204); });
    m_server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
    m_server.on("/connecttest.txt", HTTP_GET, [this]() { m_server.send(200, "text/plain", "Success"); });
    m_server.on("/fwlink", HTTP_GET, [this]() {
        m_server.sendHeader("Location", "http://192.168.4.1/");
        m_server.send(302);
    });

    m_server.onNotFound([this]() { handleNotFound(); });

    Logger::info("WebPortal", "Standard routes registered");
    return true;
}

bool WebPortal::registerApiRoutes() noexcept
{
    // Register the entire /api surface as two wildcard routes and dispatch by
    // path+method at request time. Registering each endpoint separately
    // allocates a FunctionRequestHandler per route (~52 KB total on this
    // firmware), which exhausts heap and aborts on boards without PSRAM.
    struct ApiRoute
    {
        const char *path;
        HTTPMethod method;
        void (WebPortal::*handler)();
    };

    static const ApiRoute kRoutes[] = {
        {"/api/audio/control", HTTP_POST, &WebPortal::handleApiAudioControl},
        {"/api/auth/change-password", HTTP_POST, &WebPortal::handleApiAuthChangePassword},
        {"/api/auth/login", HTTP_POST, &WebPortal::handleApiAuthLogin},
        {"/api/auth/logout", HTTP_POST, &WebPortal::handleApiAuthLogout},
        {"/api/auth/status", HTTP_GET, &WebPortal::handleApiAuthStatus},
        {"/api/automation/nl/match", HTTP_POST, &WebPortal::handleApiAutomationNLMatch},
        {"/api/automation/nl/patterns", HTTP_GET, &WebPortal::handleApiAutomationNLPatterns},
        {"/api/automations", HTTP_GET, &WebPortal::handleApiAutomations},
        {"/api/automations", HTTP_POST, &WebPortal::handleApiAutomationCreate},
        {"/api/automations/delete", HTTP_POST, &WebPortal::handleApiAutomationDelete},
        {"/api/automations/toggle", HTTP_POST, &WebPortal::handleApiAutomationToggle},
        {"/api/briefing/evening", HTTP_GET, &WebPortal::handleApiBriefingEvening},
        {"/api/briefing/morning", HTTP_GET, &WebPortal::handleApiBriefingMorning},
        {"/api/companion/devices", HTTP_GET, &WebPortal::handleApiCompanionDevices},
        {"/api/companion/devices", HTTP_POST, &WebPortal::handleApiCompanionDevices},
        {"/api/companion/messages", HTTP_GET, &WebPortal::handleApiCompanionMessages},
        {"/api/context", HTTP_GET, &WebPortal::handleApiContext},
        {"/api/context/history", HTTP_GET, &WebPortal::handleApiContextHistory},
        {"/api/crashes", HTTP_GET, &WebPortal::handleApiCrashes},
        {"/api/crashes/ack", HTTP_POST, &WebPortal::handleApiCrashAck},
        {"/api/crashes/clear", HTTP_POST, &WebPortal::handleApiCrashClear},
        {"/api/dashboard/recent", HTTP_GET, &WebPortal::handleApiDashboardRecent},
        {"/api/dashboard/summary", HTTP_GET, &WebPortal::handleApiDashboardSummary},
        {"/api/decisions", HTTP_GET, &WebPortal::handleApiDecisions},
        {"/api/decisions/explain", HTTP_GET, &WebPortal::handleApiDecisionExplain},
        {"/api/decisions/make", HTTP_POST, &WebPortal::handleApiDecisionMake},
        {"/api/decisions/rank", HTTP_GET, &WebPortal::handleApiDecisionRank},
        {"/api/developer", HTTP_GET, &WebPortal::handleApiDeveloper},
        {"/api/developer/export", HTTP_GET, &WebPortal::handleApiDeveloperExport},
        {"/api/diagnostics", HTTP_GET, &WebPortal::handleApiDiagnostics},
        {"/api/display/control", HTTP_GET, &WebPortal::handleApiDisplayControl},
        {"/api/display/control", HTTP_POST, &WebPortal::handleApiDisplayControl},
        {"/api/documents", HTTP_GET, &WebPortal::handleApiDocuments},
        {"/api/documents", HTTP_POST, &WebPortal::handleApiDocumentUpload},
        {"/api/documents/content", HTTP_GET, &WebPortal::handleApiDocumentContent},
        {"/api/documents/delete", HTTP_POST, &WebPortal::handleApiDocumentDelete},
        {"/api/documents/search", HTTP_GET, &WebPortal::handleApiDocumentSearch},
        {"/api/espnow/discovery", HTTP_POST, &WebPortal::handleApiEspNowDiscovery},
        {"/api/espnow/nodes", HTTP_GET, &WebPortal::handleApiEspNowNodes},
        {"/api/espnow/pair", HTTP_POST, &WebPortal::handleApiEspNowPair},
        {"/api/espnow/status", HTTP_GET, &WebPortal::handleApiEspNowStatus},
        {"/api/espnow/unpair", HTTP_POST, &WebPortal::handleApiEspNowUnpair},
        {"/api/errors", HTTP_GET, &WebPortal::handleApiErrors},
        {"/api/errors/ack", HTTP_POST, &WebPortal::handleApiErrorsAck},
        {"/api/errors/active", HTTP_GET, &WebPortal::handleApiErrorsActive},
        {"/api/errors/clear", HTTP_POST, &WebPortal::handleApiErrorsClear},
        {"/api/errors/count", HTTP_GET, &WebPortal::handleApiErrorsCount},
        {"/api/functions", HTTP_GET, &WebPortal::handleApiFunctions},
        {"/api/functions/execute", HTTP_POST, &WebPortal::handleApiFunctionExecute},
        {"/api/goals", HTTP_GET, &WebPortal::handleApiGoals},
        {"/api/goals", HTTP_POST, &WebPortal::handleApiGoalCreate},
        {"/api/goals/delete", HTTP_POST, &WebPortal::handleApiGoalDelete},
        {"/api/goals/update", HTTP_POST, &WebPortal::handleApiGoalUpdate},
        {"/api/graph/relationships", HTTP_GET, &WebPortal::handleApiGraphRelationships},
        {"/api/habits", HTTP_GET, &WebPortal::handleApiHabits},
        {"/api/habits", HTTP_POST, &WebPortal::handleApiHabitCreate},
        {"/api/habits/delete", HTTP_POST, &WebPortal::handleApiHabitDelete},
        {"/api/habits/toggle", HTTP_POST, &WebPortal::handleApiHabitToggle},
        {"/api/knowledge-graph", HTTP_GET, &WebPortal::handleApiKnowledgeGraph},
        {"/api/knowledge-graph/edge", HTTP_POST, &WebPortal::handleApiKnowledgeGraphEdge},
        {"/api/knowledge-graph/node", HTTP_POST, &WebPortal::handleApiKnowledgeGraphNode},
        {"/api/knowledge-graph/traverse", HTTP_GET, &WebPortal::handleApiKnowledgeGraphTraverse},
        {"/api/learning", HTTP_GET, &WebPortal::handleApiLearning},
        {"/api/learning/observe", HTTP_POST, &WebPortal::handleApiLearningObserve},
        {"/api/led/control", HTTP_GET, &WebPortal::handleApiLedControl},
        {"/api/led/control", HTTP_POST, &WebPortal::handleApiLedControl},
        {"/api/memories/maintenance", HTTP_POST, &WebPortal::handleApiMemoriesMaintenance},
        {"/api/memories/ranked", HTTP_GET, &WebPortal::handleApiMemoriesRanked},
        {"/api/memories/search", HTTP_GET, &WebPortal::handleApiMemoriesSearch},
        {"/api/memory/archive", HTTP_POST, &WebPortal::handleApiMemoryArchive},
        {"/api/memory/archived", HTTP_GET, &WebPortal::handleApiMemoryArchived},
        {"/api/memory/compare", HTTP_GET, &WebPortal::handleApiMemoryCompare},
        {"/api/memory/pin", HTTP_POST, &WebPortal::handleApiMemoryPin},
        {"/api/memory/pinned", HTTP_GET, &WebPortal::handleApiMemoryPinned},
        {"/api/memory/restore", HTTP_POST, &WebPortal::handleApiMemoryRestore},
        {"/api/memory/revisions", HTTP_GET, &WebPortal::handleApiMemoryRevisions},
        {"/api/mic/control", HTTP_POST, &WebPortal::handleApiMicControl},
        {"/api/mic/level", HTTP_GET, &WebPortal::handleApiMicLevel},
        {"/api/offline_ai/disable", HTTP_POST, &WebPortal::handleApiOfflineAIDisable},
        {"/api/offline_ai/enable", HTTP_POST, &WebPortal::handleApiOfflineAIEnable},
        {"/api/offline_ai/status", HTTP_GET, &WebPortal::handleApiOfflineAIStatus},
        {"/api/offline_ai/test", HTTP_POST, &WebPortal::handleApiOfflineAITest},
        {"/api/patterns", HTTP_GET, &WebPortal::handleApiPatterns},
        {"/api/performance", HTTP_GET, &WebPortal::handleApiPerformance},
        {"/api/personality", HTTP_GET, &WebPortal::handleApiPersonality},
        {"/api/personality/active", HTTP_GET, &WebPortal::handleApiPersonalityActive},
        {"/api/personality/active", HTTP_POST, &WebPortal::handleApiPersonalityActive},
        {"/api/planner", HTTP_GET, &WebPortal::handleApiPlanner},
        {"/api/planner/suggest", HTTP_GET, &WebPortal::handleApiPlannerSuggest},
        {"/api/planner/task", HTTP_POST, &WebPortal::handleApiPlannerTask},
        {"/api/plugins", HTTP_GET, &WebPortal::handleApiPlugins},
        {"/api/plugins/disable", HTTP_POST, &WebPortal::handleApiPluginDisable},
        {"/api/plugins/enable", HTTP_POST, &WebPortal::handleApiPluginEnable},
        {"/api/plugins/marketplace", HTTP_GET, &WebPortal::handleApiPluginsMarketplace},
        {"/api/plugins/marketplace/register", HTTP_POST, &WebPortal::handleApiPluginsMarketplaceRegister},
        {"/api/predictions", HTTP_GET, &WebPortal::handleApiPredictions},
        {"/api/predictions/run", HTTP_POST, &WebPortal::handleApiPredictionRun},
        {"/api/recommendations", HTTP_GET, &WebPortal::handleApiRecommendations},
        {"/api/recommendations/act", HTTP_POST, &WebPortal::handleApiRecommendAct},
        {"/api/recommendations/dismiss", HTTP_POST, &WebPortal::handleApiRecommendDismiss},
        {"/api/recommendations/generate", HTTP_POST, &WebPortal::handleApiRecommendGenerate},
        {"/api/reflections", HTTP_GET, &WebPortal::handleApiReflections},
        {"/api/reflections/run", HTTP_POST, &WebPortal::handleApiReflectionRun},
        {"/api/sd/diagnostics", HTTP_GET, &WebPortal::handleApiSdDiagnostics},
        {"/api/search/semantic", HTTP_GET, &WebPortal::handleApiSemanticSearch},
        {"/api/settings", HTTP_GET, &WebPortal::handleApiSettings},
        {"/api/settings", HTTP_POST, &WebPortal::handleApiSettings},
        {"/api/skills", HTTP_GET, &WebPortal::handleApiSkills},
        {"/api/skills", HTTP_POST, &WebPortal::handleApiSkillAdd},
        {"/api/skills/delete", HTTP_POST, &WebPortal::handleApiSkillDelete},
        {"/api/skills/disable", HTTP_POST, &WebPortal::handleApiSkillDisable},
        {"/api/skills/enable", HTTP_POST, &WebPortal::handleApiSkillEnable},
        {"/api/skills/studio/duplicate", HTTP_POST, &WebPortal::handleApiSkillsStudioDuplicate},
        {"/api/skills/studio/export", HTTP_GET, &WebPortal::handleApiSkillsStudioExport},
        {"/api/skills/studio/import", HTTP_POST, &WebPortal::handleApiSkillsStudioImport},
        {"/api/skills/studio/update", HTTP_POST, &WebPortal::handleApiSkillsStudioUpdate},
        {"/api/startup/settings", HTTP_GET, &WebPortal::handleApiStartupSettings},
        {"/api/startup/settings", HTTP_POST, &WebPortal::handleApiStartupSettings},
        {"/api/status", HTTP_GET, &WebPortal::handleApiStatus},
        {"/api/study/flashcards", HTTP_GET, &WebPortal::handleApiStudyFlashcards},
        {"/api/study/flashcards", HTTP_POST, &WebPortal::handleApiStudyFlashcards},
        {"/api/study/sessions", HTTP_GET, &WebPortal::handleApiStudySessions},
        {"/api/study/sessions", HTTP_POST, &WebPortal::handleApiStudySessions},
        {"/api/study/stats", HTTP_GET, &WebPortal::handleApiStudyStats},
        {"/api/study/subjects", HTTP_GET, &WebPortal::handleApiStudySubjects},
        {"/api/study/subjects", HTTP_POST, &WebPortal::handleApiStudySubjects},
        {"/api/summaries", HTTP_GET, &WebPortal::handleApiSummaries},
        {"/api/summaries/delete", HTTP_POST, &WebPortal::handleApiSummaryDelete},
        {"/api/summaries/today", HTTP_GET, &WebPortal::handleApiSummaryToday},
        {"/api/timeline", HTTP_GET, &WebPortal::handleApiTimeline},
        {"/api/timeline/search", HTTP_GET, &WebPortal::handleApiTimelineSearch},
        {"/api/uptime", HTTP_GET, &WebPortal::handleApiUptime},
        {"/api/vault", HTTP_GET, &WebPortal::handleApiVault},
        {"/api/vault", HTTP_POST, &WebPortal::handleApiVaultSet},
        {"/api/vault/backup", HTTP_POST, &WebPortal::handleApiVaultBackup},
        {"/api/vault/delete", HTTP_POST, &WebPortal::handleApiVaultDelete},
        {"/api/version", HTTP_GET, &WebPortal::handleApiVersion},
        {"/api/wake-word/calibrate", HTTP_POST, &WebPortal::handleApiWakeWordCalibrate},
        {"/api/wake-word/phrases", HTTP_GET, &WebPortal::handleApiWakeWordPhrases},
        {"/api/wake-word/phrases", HTTP_POST, &WebPortal::handleApiWakeWordAddPhrase},
        {"/api/wake-word/phrases/remove", HTTP_POST, &WebPortal::handleApiWakeWordRemovePhrase},
        {"/api/wake-word/settings", HTTP_GET, &WebPortal::handleApiWakeWordSettings},
        {"/api/wake-word/settings", HTTP_POST, &WebPortal::handleApiWakeWordSettings},
        {"/api/wake-word/stats/reset", HTTP_POST, &WebPortal::handleApiWakeWordResetStats},
        {"/api/wake-word/status", HTTP_GET, &WebPortal::handleApiWakeWordStatus},
        {"/api/wifi", HTTP_GET, &WebPortal::handleApiWifi},
        {"/api/wifi", HTTP_POST, &WebPortal::handleApiWifi},
        {"/api/wifi/forget", HTTP_POST, &WebPortal::handleApiWifiForget},
        {"/api/wifi/scan", HTTP_POST, &WebPortal::handleApiWifiScan},
        {"/api/workspaces", HTTP_GET, &WebPortal::handleApiWorkspaces},
        {"/api/workspaces", HTTP_POST, &WebPortal::handleApiWorkspaceCreate},
        {"/api/workspaces/activate", HTTP_POST, &WebPortal::handleApiWorkspaceActivate},
        {"/api/workspaces/delete", HTTP_POST, &WebPortal::handleApiWorkspaceDelete},
        {"/api/workspaces/member", HTTP_POST, &WebPortal::handleApiWorkspaceMember},
    };
    constexpr size_t kRouteCount = sizeof(kRoutes) / sizeof(kRoutes[0]);

    m_server.on(UriGlob("/api/*"), HTTP_ANY, [this]() {
        const String uri = m_server.uri();
        const HTTPMethod method = m_server.method();
        for (size_t i = 0; i < kRouteCount; ++i) {
            if (method == kRoutes[i].method && uri == kRoutes[i].path) {
                (this->*kRoutes[i].handler)();
                return;
            }
        }
        m_server.send(404, "text/plain", "Not Found");
    });

    Logger::info("WebPortal", "API dispatcher registered (%u routes, heap %u B)",
                 static_cast<unsigned>(kRouteCount), ESP.getFreeHeap());
    return true;
}

// ============================================================================
// System Control
// ============================================================================

void WebPortal::restartDevice() noexcept
{
    Logger::warning("WebPortal", "Device restart requested");
    sendSuccess("Device restarting...");

    xTaskCreate(restartTask, "restart_task", 4096, nullptr, 1, nullptr);
}

void WebPortal::factoryReset() noexcept
{
    Logger::warning("WebPortal", "Factory reset requested");
    sendSuccess("Factory reset initiated...");

    xTaskCreate(factoryResetTask, "factory_reset_task", 4096, nullptr, 1, nullptr);
}

// ============================================================================
// Status Functions
// ============================================================================

uint16_t WebPortal::getPort() const noexcept
{
    return m_port;
}

String WebPortal::getIPAddress() const noexcept
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return WiFi.localIP().toString();
    }
    return "";
}

String WebPortal::getHostname() const noexcept
{
    return WiFi.getHostname();
}

// ============================================================================
// Private Helpers - Static File Serving
// ============================================================================

void WebPortal::serveSPA() noexcept
{
    String content;
    if (storageManager.readFile("/index.html", content, StorageType::SPIFFS) == StorageStatus::SUCCESS)
    {
        m_server.send(200, "text/html; charset=utf-8", content);
    }
    else
    {
        m_server.send(200, "text/html; charset=utf-8",
            "<!DOCTYPE html><html><head><meta charset='utf-8'><title>AURA</title></head>"
            "<body style='background:#050505;color:#fff;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;margin:0'>"
            "<div style='text-align:center'><h1>AURA</h1><p style='color:#8A8A8A'>Loading...</p></div></body></html>");
    }
}

void WebPortal::serveStaticFile(const String& path) noexcept
{
    if (!storageManager.fileExists(path.c_str(), StorageType::SPIFFS))
    {
        m_server.send(404, "text/plain", "Not Found");
        return;
    }

    String content;
    const StorageStatus status = storageManager.readFile(
        path.c_str(), content, StorageType::SPIFFS);

    if (status != StorageStatus::SUCCESS)
    {
        m_server.send(500, "text/plain", "Internal Server Error");
        return;
    }

    String contentType = "text/plain";
    if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".css"))  contentType = "text/css";
    else if (path.endsWith(".js"))   contentType = "application/javascript";
    else if (path.endsWith(".json")) contentType = "application/json";
    else if (path.endsWith(".png"))  contentType = "image/png";
    else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
    else if (path.endsWith(".svg"))  contentType = "image/svg+xml";
    else if (path.endsWith(".ico"))  contentType = "image/x-icon";
    else if (path.endsWith(".woff2")) contentType = "font/woff2";
    else if (path.endsWith(".woff")) contentType = "font/woff";

    m_server.send(200, contentType, content);
}

// ============================================================================
// Private Handlers - Standard Routes
// ============================================================================

void WebPortal::handleRoot() noexcept
{
    m_requestCounter++;
    serveSPA();
}

void WebPortal::handleStatus() noexcept
{
    m_requestCounter++;
    serveSPA();
}

void WebPortal::handleWifi() noexcept
{
    m_requestCounter++;
    serveSPA();
}

void WebPortal::handleSaveWifi() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (!m_server.hasArg("ssid") || !m_server.hasArg("password"))
    {
        sendError("Missing SSID or password");
        return;
    }

    String ssid = m_server.arg("ssid");
    String password = m_server.arg("password");

    // Persist to NVS so the device reconnects after reboot, then connect.
    wifiManager.saveCredentials(ssid.c_str(), password.c_str());
    wifiManager.connect(ssid.c_str(), password.c_str());
    sendSuccess("Wi-Fi configuration saved. Device will reconnect...");
}

void WebPortal::handleSettings() noexcept
{
    m_requestCounter++;
    serveSPA();
}

void WebPortal::handleSaveSettings() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.hasArg("device_name"))
    {
        String deviceName = m_server.arg("device_name");
        Logger::info("WebPortal", "Device name updated");
    }

    sendSuccess("Settings saved successfully");
}

void WebPortal::handleRestart() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    restartDevice();
}

void WebPortal::handleFactoryReset() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    factoryReset();
}

void WebPortal::handleOTA() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET)
    {
        serveSPA();
    }
    else if (m_server.method() == HTTP_POST)
    {
        HTTPUpload& upload = m_server.upload();

        if (upload.status == UPLOAD_FILE_START)
        {
            Logger::info("WebPortal", "OTA upload started");
            m_otaInProgress = true;
            m_otaHashing = true;
            mbedtls_sha256_starts(&m_otaSha256, 0);

            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            {
                Logger::error("WebPortal", "OTA update begin failed");
                m_otaInProgress = false;
                m_otaHashing = false;
                sendError("OTA update initialization failed", 500);
                return;
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (m_otaInProgress)
            {
                if (m_otaHashing) {
                    mbedtls_sha256_update(&m_otaSha256, upload.buf, upload.currentSize);
                }
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                {
                    Logger::error("WebPortal", "OTA update write failed");
                    Update.abort();
                    m_otaInProgress = false;
                    m_otaHashing = false;
                    sendError("OTA write failed", 500);
                }
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            bool signatureOk = true;
            if (m_otaInProgress && m_otaHashing) {
                uint8_t hash[32];
                mbedtls_sha256_finish(&m_otaSha256, hash);
                m_otaHashing = false;
                const String sig = m_server.hasHeader("X-Signature")
                    ? m_server.header("X-Signature")
                    : (m_server.hasArg("signature") ? m_server.arg("signature") : "");
                if (!sig.isEmpty()) {
                    signatureOk = verifyWebOtaSignature(hash, sig);
                    if (!signatureOk) {
                        Logger::error("WebPortal", "Web OTA signature INVALID â€” aborting");
                        Update.abort();
                        m_otaInProgress = false;
                        securityManager.LogAudit(AuditEventType::OTA_STARTED, "webportal",
                                                 "Web OTA rejected: bad signature", false);
                        sendError("Firmware signature verification failed", 400);
                        return;
                    }
                } else {
                    Logger::warning("WebPortal",
                        "Web OTA uploaded WITHOUT a signature. Only signed firmware should "
                        "be installed in production (see OTA docs).");
                }
            }

            if (m_otaInProgress && signatureOk && Update.end(true))
            {
                Logger::info("WebPortal", "OTA update completed successfully");
                m_otaInProgress = false;
                sendSuccess("Firmware updated successfully. Device will restart...");

                xTaskCreate(otaRestartTask, "ota_restart_task", 4096, nullptr, 1, nullptr);
            }
            else if (m_otaInProgress)
            {
                Logger::error("WebPortal", "OTA update finalization failed");
                m_otaInProgress = false;
                sendError("OTA update failed", 500);
            }
        }
        else if (upload.status == UPLOAD_FILE_ABORTED)
        {
            Logger::warning("WebPortal", "OTA update aborted");
            Update.abort();
            m_otaInProgress = false;
            m_otaHashing = false;
            sendError("OTA upload aborted", 400);
        }
    }
}

bool WebPortal::verifyWebOtaSignature(const uint8_t hash[32], const String& sigHex) noexcept {
    // Fail closed if no key is embedded.
    if (kFirmwareSigningKeyLen == 0) {
        Logger::error("WebPortal", "No firmware signing key embedded");
        return false;
    }

    size_t hexLen = sigHex.length();
    if (hexLen == 0 || (hexLen % 2) != 0) return false;
    size_t sigLen = hexLen / 2;
    uint8_t* sigBuf = new (std::nothrow) uint8_t[sigLen];
    if (!sigBuf) return false;
    for (size_t i = 0; i < sigLen; ++i) {
        char hi = sigHex[i * 2];
        char lo = sigHex[i * 2 + 1];
        uint8_t b = 0;
        if (hi >= '0' && hi <= '9') b = (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') b = (hi - 'a' + 10) << 4;
        else if (hi >= 'A' && hi <= 'F') b = (hi - 'A' + 10) << 4;
        else { delete[] sigBuf; return false; }
        if (lo >= '0' && lo <= '9') b |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') b |= (lo - 'a' + 10);
        else if (lo >= 'A' && lo <= 'F') b |= (lo - 'A' + 10);
        else { delete[] sigBuf; return false; }
        sigBuf[i] = b;
    }

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ret = mbedtls_pk_parse_public_key(&pk, kFirmwareSigningKey, kFirmwareSigningKeyLen);
    if (ret != 0) {
        delete[] sigBuf;
        mbedtls_pk_free(&pk);
        return false;
    }
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sigBuf, sigLen);
    delete[] sigBuf;
    mbedtls_pk_free(&pk);
    return ret == 0;
}

void WebPortal::handleNotFound() noexcept
{
    m_requestCounter++;

    const String uri = m_server.uri();

    if (uri.startsWith("/css/") || uri.startsWith("/js/") ||
        uri.startsWith("/assets/") || uri == "/manifest.json" || uri == "/favicon.ico")
    {
        serveStaticFile(uri);
    }
    else
    {
        String content;
        if (storageManager.readFile("/index.html", content, StorageType::SPIFFS) == StorageStatus::SUCCESS)
        {
            m_server.send(404, "text/html; charset=utf-8", content);
        }
        else
        {
            m_server.send(404, "text/plain", "Not Found");
        }
    }
}

// ============================================================================
// Private Handlers - WebSocket
// ============================================================================

void WebPortal::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) noexcept {
    switch (type) {
        case WStype_DISCONNECTED:
            Logger::info("WebPortal", "WS[%u] disconnected", num);
            m_wsAuth[num] = false;
            if (eventBus.isInitialized()) {
                eventBus.publish(EventType::WS_CLIENT_DISCONNECTED, "WebPortal");
            }
            break;

        case WStype_CONNECTED:
            Logger::info("WebPortal", "WS[%u] connected (awaiting auth)", num);
            m_wsAuth[num] = false;
            // No telemetry is sent until the client authenticates with a valid
            // session token (see handleWebSocketAuthMessage). Sending telemetry
            // here would leak device state to any unauthenticated client.
            m_webSocket.sendTXT(num, "{\"type\":\"auth_required\"}");
            break;

        case WStype_TEXT:
        {
            String msg = String(reinterpret_cast<char*>(payload));
            if (msg == "ping") {
                m_webSocket.sendTXT(num, "pong");
                break;
            }
            if (msg == "pong") {
                break;
            }
            handleWebSocketAuthMessage(num, msg);
            break;
        }

        default:
            break;
    }
}

void WebPortal::handleWebSocketAuthMessage(uint8_t num, const String& msg) noexcept {
    if (m_wsAuth[num]) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) {
        m_webSocket.sendTXT(num, "{\"type\":\"error\",\"message\":\"invalid handshake\"}");
        m_webSocket.disconnect(num);
        return;
    }

    String type = doc["type"] | "";
    if (type != "auth") {
        m_webSocket.sendTXT(num, "{\"type\":\"error\",\"message\":\"auth required\"}");
        m_webSocket.disconnect(num);
        return;
    }

    String token = doc["token"] | "";

    // Reject empty/expired/mismatched tokens; close the connection immediately.
    // The token must be the active session token validated by SecurityManager.
    bool valid = !token.isEmpty()
        && !m_sessionToken.isEmpty()
        && m_sessionCreated != 0
        && (millis() - m_sessionCreated) < kSessionTimeoutMs
        && securityManager.CheckToken(token);

    if (!valid) {
        Logger::warning("WebPortal", "WS[%u] rejected: invalid session token", num);
        securityManager.LogAudit(AuditEventType::UNAUTHORIZED_ACCESS, "websocket",
                                 "WebSocket auth rejected", false);
        m_webSocket.sendTXT(num, "{\"type\":\"error\",\"message\":\"unauthorized\"}");
        m_webSocket.disconnect(num);
        return;
    }

    m_wsAuth[num] = true;
    Logger::info("WebPortal", "WS[%u] authenticated", num);

    // Send the current dashboard + module status snapshot on successful auth
    webSocketBroadcastDashboard();
    serviceStatusManager.markAllChanged();
    webSocketBroadcastModuleStatus();
}

void WebPortal::sendToAuthenticatedClients(String json) noexcept {
    if (!m_running) return;
    for (size_t i = 0; i < WS_MAX_CLIENTS; ++i) {
        if (m_wsAuth[i] && m_webSocket.clientIsConnected(static_cast<uint8_t>(i))) {
            m_webSocket.sendTXT(static_cast<uint8_t>(i), json);
        }
    }
}

void WebPortal::webSocketBroadcast(String json) noexcept {
    sendToAuthenticatedClients(json);
}

void WebPortal::broadcastAssistantResponse(const String& json) noexcept {
    if (!m_running) return;
    if (m_webSocket.connectedClients() == 0) return;  // drop safely if app disconnected
    webSocketBroadcast(json);
}

void WebPortal::webSocketBroadcastDashboard() noexcept {
    if (!m_running || m_webSocket.connectedClients() == 0) return;

    String json;
    json.reserve(2048);
    json = "{\"type\":\"dashboard\"";

    // Performance metrics
    if (performanceManager.isInitialized()) {
        json += ",\"performance\":" + performanceManager.getMetricsJson();
    }

    // Diagnostics
    if (diagnosticsManager.isInitialized()) {
        json += ",\"diagnostics\":" + diagnosticsManager.getResultsJson();
    }

    // Conversation state
    if (conversationManager.isInitialized()) {
        json += ",\"conversation\":{";
        json += "\"state\":" + String(static_cast<int>(conversationManager.getState()));
        json += ",\"stateString\":\"" + String(conversationStateToString(conversationManager.getState())) + "\"";
        json += ",\"busy\":" + String(conversationManager.isBusy() ? "true" : "false");
        json += ",\"wakeWord\":" + String(conversationManager.isWakeWordEnabled() ? "true" : "false");
        json += ",\"privacy\":" + String(conversationManager.isPrivacyMode() ? "true" : "false");
        json += ",\"muted\":" + String(conversationManager.isPrivacyMode() ? "true" : "false");
        json += ",\"setup\":" + String(conversationManager.isSetupMode() ? "true" : "false");
        json += ",\"listening\":" + String(conversationManager.getState() == ConversationState::LISTENING ? "true" : "false");
        json += ",\"speaking\":" + String(conversationManager.getState() == ConversationState::SPEAKING ? "true" : "false");
        json += ",\"processing\":" + String((conversationManager.getState() == ConversationState::TRANSCRIBING ||
                                             conversationManager.getState() == ConversationState::THINKING) ? "true" : "false");
        json += ",\"recording\":" + String(conversationManager.isMicActive() ? "true" : "false");
        json += ",\"mic_active\":" + String(conversationManager.isMicActive() ? "true" : "false");
        json += ",\"lastVoiceTimestamp\":" + String(conversationManager.getLastVoiceTimestampMs());
        json += ",\"wakeSource\":\"" + conversationManager.getLastWakeSource() + "\"";
        json += ",\"touch\":\"" + conversationManager.getLastTouchEvent() + "\"";
        if (audioManager.isInitialized()) {
            json += ",\"voiceActive\":" + String(audioManager.isVoiceActive() ? "true" : "false");
            json += ",\"micLevel\":" + String(static_cast<int>(audioManager.getAudioEnergy() * 100.0f));
        }
        json += "}";
    }

    // ESP-NOW status
    if (espNowManager.isInitialized()) {
        json += ",\"espnow\":" + espNowManager.getNodesJson();
    }

    // System info
    json += ",\"system\":{";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap());
    json += ",\"uptime\":" + String(millis() / 1000);
    json += ",\"wifiRSSI\":" + String(WiFi.RSSI());
    json += "}";

    // Disco Mode state (app always reflects whether it is active)
    json += ",\"disco\":{";
    json += "\"enabled\":" + String(ledRing.isDiscoEnabled() ? "true" : "false");
    json += ",\"active\":" + String(ledRing.isDiscoActive() ? "true" : "false");
    json += ",\"brightness\":" + String(static_cast<int>(settingsManager.getDiscoBrightness()));
    json += "}";

    json += "}";
    webSocketBroadcast(json);
}

void WebPortal::webSocketBroadcastModuleStatus() noexcept {
    if (!m_running || m_webSocket.connectedClients() == 0) return;
    if (!serviceStatusManager.hasPendingChanges()) return;

    String json = serviceStatusManager.takePendingChangesJson();
    webSocketBroadcast(json);
}

// ============================================================================
// Private Handlers - API Routes
// ============================================================================

// ============================================================================
// Plugin API
// ============================================================================

void WebPortal::handleApiPlugins() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String json;
    json.reserve(2048);
    json += "{\"plugins\":[";
    const auto& plugins = pluginManager.getAllPlugins();
    for (size_t i = 0; i < plugins.size(); ++i) {
        if (i > 0) json += ",";
        const auto& p = plugins[i];
        json += "{";
        json += "\"id\":\"" + p.id + "\",";
        json += "\"name\":\"" + p.name + "\",";
        json += "\"version\":\"" + p.version + "\",";
        json += "\"author\":\"" + p.author + "\",";
        json += "\"description\":\"" + p.description + "\",";
        json += "\"enabled\":" + String(p.enabled ? "true" : "false");
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiPluginEnable() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing plugin ID"); return; }
    sendSuccess(pluginManager.enablePlugin(id) ? "Plugin enabled" : "Plugin not found");
}

void WebPortal::handleApiPluginDisable() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing plugin ID"); return; }
    sendSuccess(pluginManager.disablePlugin(id) ? "Plugin disabled" : "Plugin not found");
}

// ============================================================================
// Skills API
// ============================================================================

void WebPortal::handleApiSkills() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String json;
    json.reserve(4096);
    json += "{\"skills\":[";
    const auto& skills = skillManager.getAllSkills();
    for (size_t i = 0; i < skills.size(); ++i) {
        if (i > 0) json += ",";
        const auto& s = skills[i];
        json += "{";
        json += "\"id\":\"" + s.id + "\",";
        json += "\"name\":\"" + s.name + "\",";
        json += "\"trigger\":\"" + s.voiceTrigger + "\",";
        json += "\"priority\":" + String(s.priority) + ",";
        json += "\"enabled\":" + String(s.enabled ? "true" : "false") + ",";
        json += "\"triggers\":" + String(s.triggerCount);
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiSkillAdd() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    if (!m_server.hasArg("plain")) {
        sendError("No JSON body", 400);
        return;
    }
    // Skills added via frontend - parse and create
    SkillEntry skill;
    skill.name = m_server.arg("name");
    skill.voiceTrigger = m_server.arg("trigger");
    if (skill.name.isEmpty()) { sendError("Missing skill name"); return; }
    sendSuccess(skillManager.addSkill(skill) ? "Skill added" : "Failed to add skill");
}

void WebPortal::handleApiSkillDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing skill ID"); return; }
    sendSuccess(skillManager.removeSkill(id) ? "Skill removed" : "Skill not found");
}

void WebPortal::handleApiSkillEnable() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing skill ID"); return; }
    sendSuccess(skillManager.enableSkill(id) ? "Skill enabled" : "Skill not found");
}

void WebPortal::handleApiSkillDisable() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing skill ID"); return; }
    sendSuccess(skillManager.disableSkill(id) ? "Skill disabled" : "Skill not found");
}

// ============================================================================
// Personality API
// ============================================================================

void WebPortal::handleApiPersonality() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String json;
    json.reserve(2048);
    json += "{\"profiles\":[";
    const auto& profiles = personalityManager.getAllProfiles();
    for (size_t i = 0; i < profiles.size(); ++i) {
        if (i > 0) json += ",";
        const auto& p = profiles[i];
        json += "{";
        json += "\"id\":\"" + p.id + "\",";
        json += "\"name\":\"" + p.name + "\",";
        json += "\"voice\":\"" + p.voice + "\",";
        json += "\"style\":\"" + p.responseStyle + "\",";
        json += "\"active\":" + String(p.id == personalityManager.getActiveProfileId() ? "true" : "false");
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiPersonalityActive() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET)
    {
        String json;
        json.reserve(256);
        const auto& active = personalityManager.getActiveProfile();
        json += "{\"id\":\"" + active.id + "\",";
        json += "\"name\":\"" + active.name + "\",";
        json += "\"voice\":\"" + active.voice + "\",";
        json += "\"style\":\"" + active.responseStyle + "\"}";
        sendJson(json);
    }
    else if (m_server.method() == HTTP_POST)
    {
        String id = m_server.arg("id");
        if (id.isEmpty()) { sendError("Missing profile ID"); return; }
        sendSuccess(personalityManager.activateProfile(id) ? "Profile activated" : "Profile not found");
    }
}

// ============================================================================
// Context API
// ============================================================================

void WebPortal::handleApiContext() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(contextManager.getContextJson());
}

void WebPortal::handleApiContextHistory() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json = "{\"history\":";
    json += contextManager.getContextHistory();
    json += "}";
    sendJson(json);
}

// ============================================================================
// Timeline API
// ============================================================================

void WebPortal::handleApiTimeline() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String range = m_server.arg("range");
    std::vector<TimelineEntry> entries;
    if (range == "yesterday") entries = timelineManager.getYesterday();
    else if (range == "week") entries = timelineManager.getThisWeek();
    else if (range == "month") entries = timelineManager.getThisMonth();
    else entries = timelineManager.getToday();
    String json; json.reserve(4096);
    json += "{\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + entries[i].id + "\",";
        json += "\"ts\":" + String(entries[i].timestamp) + ",";
        json += "\"cat\":\"" + entries[i].category + "\",";
        json += "\"summary\":\"" + entries[i].summary + "\",";
        json += "\"imp\":" + String(entries[i].importance) + "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiTimelineSearch() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String query = m_server.arg("q");
    if (query.isEmpty()) { sendError("Missing query"); return; }
    auto entries = timelineManager.search(query);
    String json; json.reserve(4096);
    json += "{\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + entries[i].id + "\",";
        json += "\"ts\":" + String(entries[i].timestamp) + ",";
        json += "\"cat\":\"" + entries[i].category + "\",";
        json += "\"summary\":\"" + entries[i].summary + "\",";
        json += "\"imp\":" + String(entries[i].importance) + "}";
    }
    json += "]}";
    sendJson(json);
}

// ============================================================================
// Briefing API
// ============================================================================

void WebPortal::handleApiBriefingMorning() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String content = briefingManager.generateMorning();
    String json = "{\"type\":\"morning\",\"content\":\"" + content + "\"}";
    sendJson(json);
}

void WebPortal::handleApiBriefingEvening() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String content = briefingManager.generateEvening();
    String json = "{\"type\":\"evening\",\"content\":\"" + content + "\"}";
    sendJson(json);
}

// ============================================================================
// Semantic Search API
// ============================================================================

void WebPortal::handleApiSemanticSearch() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String query = m_server.arg("q");
    if (query.isEmpty()) { sendError("Missing query"); return; }
    auto results = semanticSearchManager.search(query);
    String json; json.reserve(4096);
    json += "{\"results\":[";
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"source\":\"" + results[i].source + "\",";
        json += "\"id\":\"" + results[i].id + "\",";
        json += "\"title\":\"" + results[i].title + "\",";
        json += "\"snippet\":\"" + results[i].snippet + "\",";
        json += "\"relevance\":" + String(results[i].relevance, 2) + "}";
    }
    json += "]}";
    sendJson(json);
}

// ============================================================================
// Graph Relationships API
// ============================================================================

void WebPortal::handleApiGraphRelationships() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String type = m_server.arg("type");
    if (type.isEmpty()) {
        sendJson("{\"edges\":[]}");
        return;
    }
    auto edges = knowledgeGraphManager.getRelationshipsByType(type);
    String json; json.reserve(4096);
    json += "{\"edges\":[";
    for (size_t i = 0; i < edges.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + edges[i].id + "\",";
        json += "\"source\":\"" + edges[i].sourceId + "\",";
        json += "\"target\":\"" + edges[i].targetId + "\",";
        json += "\"rel\":\"" + edges[i].relationship + "\",";
        json += "\"strength\":" + String(edges[i].strength, 2) + ",";
        json += "\"bidir\":" + String(edges[i].bidirectional ? "true" : "false") + "}";
    }
    json += "]}";
    sendJson(json);
}

// ============================================================================
// Performance API
// ============================================================================

void WebPortal::handleApiPerformance() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(512);
    json += "{";
    json += "\"metrics\":";
    json += performanceManager.getMetricsJson();
    json += "}";
    sendJson(json);
}

// ============================================================================
// Diagnostics API
// ============================================================================

void WebPortal::handleApiDiagnostics() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    diagnosticsManager.runAllTests();
    sendJson(diagnosticsManager.getResultsJson());
}

// ============================================================================
// Crash Logs API
// ============================================================================

void WebPortal::handleApiCrashes() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String json;
    json.reserve(4096);
    json += "{\"crashes\":[";
    const auto& crashes = crashManager.getAllCrashes();
    for (size_t i = 0; i < crashes.size(); ++i) {
        if (i > 0) json += ",";
        const auto& c = crashes[i];
        json += "{";
        json += "\"id\":\"" + c.id + "\",";
        json += "\"timestamp\":" + String(c.timestamp) + ",";
        json += "\"exception\":\"" + c.exception + "\",";
        json += "\"module\":\"" + c.lastModule + "\",";
        json += "\"heap\":" + String(c.freeHeap) + ",";
        json += "\"reason\":\"" + c.resetReason + "\",";
        json += "\"acknowledged\":" + String(c.acknowledged ? "true" : "false");
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiCrashAck() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing crash ID"); return; }
    sendSuccess(crashManager.acknowledgeCrash(id) ? "Crash acknowledged" : "Crash not found");
}

void WebPortal::handleApiCrashClear() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    crashManager.clearCrashes();
    sendSuccess("All crash logs cleared");
}

// ============================================================================
// Diagnostic Events (AURA_ERROR) API
// ============================================================================

void WebPortal::handleApiErrors() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (!id.isEmpty()) {
        String ev = errorManager.getEventJsonById(id);
        if (ev.isEmpty()) { sendError("Event not found", 404); return; }
        sendJson("{\"event\":" + ev + "}");
        return;
    }
    sendJson(errorManager.buildJson(false));
}

void WebPortal::handleApiErrorsActive() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(errorManager.buildJson(true));
}

void WebPortal::handleApiErrorsCount() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(errorManager.buildSummaryJson());
}

void WebPortal::handleApiErrorsAck() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing event ID"); return; }
    sendSuccess(errorManager.acknowledge(id) ? "Event acknowledged" : "Event not found");
}

void WebPortal::handleApiErrorsClear() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    errorManager.clearAll();
    sendSuccess("All diagnostic events cleared");
}

// ============================================================================
// Daily Summary API
// ============================================================================

void WebPortal::handleApiSummaries() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String json;
    json.reserve(4096);
    json += "{\"summaries\":[";
    const auto& summaries = briefingManager.getAllSummaries();
    for (size_t i = 0; i < summaries.size(); ++i) {
        if (i > 0) json += ",";
        const auto& s = summaries[i];
        json += "{";
        json += "\"id\":\"" + s.id + "\",";
        json += "\"date\":\"" + s.date + "\",";
        json += "\"reminders\":" + String(s.reminderCount) + ",";
        json += "\"memories\":" + String(s.memoryCount) + ",";
        json += "\"favorite\":" + String(s.favorite ? "true" : "false");
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiSummaryToday() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    DailySummary today = briefingManager.getTodaySummary();
    if (today.id.isEmpty()) {
        briefingManager.generateTodaySummary();
        today = briefingManager.getTodaySummary();
    }

    String json;
    json.reserve(2048);
    json += "{";
    json += "\"id\":\"" + today.id + "\",";
    json += "\"date\":\"" + today.date + "\",";
    json += "\"content\":\"" + today.content + "\",";
    json += "\"reminders\":" + String(today.reminderCount) + ",";
    json += "\"memories\":" + String(today.memoryCount) + ",";
    json += "\"conversations\":" + String(today.conversationCount);
    json += "}";
    sendJson(json);
}

void WebPortal::handleApiSummaryDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing summary ID"); return; }
    sendSuccess(briefingManager.deleteSummary(id) ? "Summary deleted" : "Summary not found");
}

// ============================================================================
// Memory Extensions API
// ============================================================================

void WebPortal::handleApiMemoriesRanked() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    auto ranked = memoryManager.getRankedMemories(MEMORY_RANK_TOP_N);
    String json;
    json.reserve(2048);
    json += "{\"ranked\":[";
    for (size_t i = 0; i < ranked.size(); ++i) {
        if (i > 0) json += ",";
        const auto& m = ranked[i];
        json += "{";
        json += "\"id\":\"" + m.id + "\",";
        json += "\"key\":\"" + m.key + "\",";
        json += "\"value\":\"" + m.value + "\",";
        json += "\"score\":" + String(m.computeRelevanceScore(), 1) + ",";
        json += "\"favorite\":" + String(m.favorite ? "true" : "false");
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiMemoriesSearch() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String query = m_server.arg("q");
    if (query.isEmpty()) { sendError("Missing search query"); return; }

    auto results = memoryManager.semanticSearch(query, 10);
    String json;
    json.reserve(2048);
    json += "{\"results\":[";
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) json += ",";
        const auto& m = results[i];
        json += "{";
        json += "\"id\":\"" + m.id + "\",";
        json += "\"key\":\"" + m.key + "\",";
        json += "\"value\":\"" + m.value + "\",";
        json += "\"category\":" + String(static_cast<int>(m.category));
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiMemoriesMaintenance() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    size_t cleaned = memoryManager.runMaintenance();
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"cleaned\":%u}", cleaned);
    sendJson(buf);
}

// ============================================================================
// V2.0 Feature API Handlers
// ============================================================================

// ============================================================================
// Knowledge Graph API
// ============================================================================

void WebPortal::handleApiKnowledgeGraph() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(4096);
    json += "{\"nodes\":[";
    const auto& nodes = knowledgeGraphManager.getAllNodes();
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) json += ",";
        const auto& n = nodes[i];
        String typeStr;
        switch (n.type) {
            case NodeType::USER: typeStr = "user"; break; case NodeType::PROJECT: typeStr = "project"; break;
            case NodeType::DEVICE: typeStr = "device"; break; case NodeType::PERSON: typeStr = "person"; break;
            case NodeType::SUBJECT: typeStr = "subject"; break; case NodeType::GOAL: typeStr = "goal"; break;
            case NodeType::PREFERENCE: typeStr = "preference"; break; case NodeType::SKILL: typeStr = "skill"; break;
            default: typeStr = "custom"; break;
        }
        json += "{\"id\":\"" + n.id + "\",\"name\":\"" + n.name + "\",\"type\":\"" + typeStr + "\",\"value\":\"" + n.value + "\",\"tags\":\"" + n.tags + "\",\"importance\":" + String(n.importance) + "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiKnowledgeGraphNode() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String name = m_server.arg("name");
    if (name.isEmpty()) { sendError("Missing node name"); return; }
    String typeStr = m_server.arg("type");
    NodeType nt = NodeType::CUSTOM;
    if (typeStr == "user") nt = NodeType::USER; else if (typeStr == "project") nt = NodeType::PROJECT;
    else if (typeStr == "device") nt = NodeType::DEVICE; else if (typeStr == "person") nt = NodeType::PERSON;
    else if (typeStr == "subject") nt = NodeType::SUBJECT; else if (typeStr == "goal") nt = NodeType::GOAL;
    else if (typeStr == "preference") nt = NodeType::PREFERENCE; else if (typeStr == "skill") nt = NodeType::SKILL;
    uint8_t importance = static_cast<uint8_t>(m_server.arg("importance").toInt());
    String id = knowledgeGraphManager.createNode(nt, name, m_server.arg("value"), m_server.arg("tags"), importance);
    sendSuccess(id.isEmpty() ? "Failed to create node" : ("Node created: " + id));
}

void WebPortal::handleApiKnowledgeGraphEdge() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String source = m_server.arg("source");
    String target = m_server.arg("target");
    String relation = m_server.arg("relation");
    float strength = m_server.arg("strength").toFloat();
    if (strength < 0.01f) strength = 1.0f;
    if (source.isEmpty() || target.isEmpty()) { sendError("Missing source or target"); return; }
    String id = knowledgeGraphManager.createEdge(source, target, relation, strength, false);
    sendSuccess(id.isEmpty() ? "Failed to create edge" : ("Edge created: " + id));
}

void WebPortal::handleApiKnowledgeGraphTraverse() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String startId = m_server.arg("start");
    if (startId.isEmpty()) { sendError("Missing start node ID"); return; }
    uint8_t depth = static_cast<uint8_t>(m_server.arg("depth").toInt());
    if (depth < 1 || depth > 3) depth = 2;
    auto result = knowledgeGraphManager.traverse(startId, "", depth);
    String json;
    json.reserve(2048);
    json += "{\"path\":[";
    for (size_t i = 0; i < result.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + result[i].id + "\",\"name\":\"" + result[i].name + "\",\"type\":" + String(static_cast<int>(result[i].type)) + "}";
    }
    json += "]}";
    sendJson(json);
}

// ============================================================================
// Goals API
// ============================================================================

void WebPortal::handleApiGoals() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(4096);
    json += "{\"goals\":[";
    const auto& goals = goalManager.getAllGoals();
    for (size_t i = 0; i < goals.size(); ++i) {
        if (i > 0) json += ",";
        const auto& g = goals[i];
        String typeStr;
        switch (g.type) {
            case GoalType::DAILY: typeStr = "daily"; break; case GoalType::WEEKLY: typeStr = "weekly"; break;
            case GoalType::LONG_TERM: typeStr = "long_term"; break; default: typeStr = "daily"; break;
        }
        json += "{\"id\":\"" + g.id + "\",\"title\":\"" + g.title + "\",\"description\":\"" + g.description + "\"";
        json += ",\"type\":\"" + typeStr + "\",\"progress\":" + String(g.progress) + ",\"priority\":" + String(g.priority);
        json += ",\"completed\":" + String(g.completed ? "true" : "false") + ",\"deadline\":" + String(g.deadline) + "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiGoalCreate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String title = m_server.arg("title");
    if (title.isEmpty()) { sendError("Missing goal title"); return; }
    String typeStr = m_server.arg("type");
    GoalType gt = GoalType::DAILY;
    if (typeStr == "weekly") gt = GoalType::WEEKLY; else if (typeStr == "long_term") gt = GoalType::LONG_TERM;
    uint8_t prio = static_cast<uint8_t>(m_server.arg("priority").toInt());
    unsigned long deadline = static_cast<unsigned long>(m_server.arg("deadline").toInt());
    String id = goalManager.createGoal(title, gt, m_server.arg("description"), prio, deadline);
    sendSuccess(id.isEmpty() ? "Failed to create goal" : ("Goal created: " + id));
}

void WebPortal::handleApiGoalDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing goal ID"); return; }
    sendSuccess(goalManager.deleteGoal(id) ? "Goal deleted" : "Goal not found");
}

void WebPortal::handleApiGoalUpdate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing goal ID"); return; }
    uint8_t progress = static_cast<uint8_t>(m_server.arg("progress").toInt());
    uint8_t priority = static_cast<uint8_t>(m_server.arg("priority").toInt());
    bool completed = m_server.arg("complete") == "true";
    bool ok = false;
    if (completed) { ok = goalManager.completeGoal(id); }
    else { ok = goalManager.updateGoal(id, m_server.arg("title"), m_server.arg("description"), priority, progress); }
    sendSuccess(ok ? "Goal updated" : "Goal not found");
}

// ============================================================================
// Habits API
// ============================================================================

void WebPortal::handleApiHabits() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(4096);
    json += "{\"habits\":[";
    const auto& habits = habitManager.getAllHabits();
    for (size_t i = 0; i < habits.size(); ++i) {
        if (i > 0) json += ",";
        const auto& h = habits[i];
        String schedStr;
        switch (h.schedule) {
            case HabitSchedule::DAILY: schedStr = "daily"; break; case HabitSchedule::WEEKLY: schedStr = "weekly"; break;
            case HabitSchedule::MONTHLY: schedStr = "monthly"; break; case HabitSchedule::CUSTOM: schedStr = "custom"; break;
            default: schedStr = "daily"; break;
        }
        json += "{\"id\":\"" + h.id + "\",\"name\":\"" + h.name + "\",\"description\":\"" + h.description + "\"";
        json += ",\"schedule\":\"" + schedStr + "\",\"streak\":" + String(h.streak) + ",\"successRate\":" + String(h.successRate, 1);
        json += ",\"totalCompletions\":" + String(h.totalCompletions) + ",\"longestStreak\":" + String(h.longestStreak) + "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiHabitCreate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String name = m_server.arg("name");
    if (name.isEmpty()) { sendError("Missing habit name"); return; }
    String schedStr = m_server.arg("schedule");
    HabitSchedule hs = HabitSchedule::DAILY;
    if (schedStr == "weekly") hs = HabitSchedule::WEEKLY; else if (schedStr == "monthly") hs = HabitSchedule::MONTHLY; else if (schedStr == "custom") hs = HabitSchedule::CUSTOM;
    bool reminder = m_server.arg("reminder") == "true";
    String id = habitManager.createHabit(name, hs, m_server.arg("description"), reminder);
    sendSuccess(id.isEmpty() ? "Failed to create habit" : ("Habit created: " + id));
}

void WebPortal::handleApiHabitDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing habit ID"); return; }
    sendSuccess(habitManager.deleteHabit(id) ? "Habit deleted" : "Habit not found");
}

void WebPortal::handleApiHabitToggle() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing habit ID"); return; }
    sendSuccess(habitManager.completeHabit(id) ? "Habit completed" : "Habit not found");
}

// ============================================================================
// Planner API
// ============================================================================

void WebPortal::handleApiPlanner() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(4096);
    json += "{\"tasks\":[";
    const auto& tasks = plannerManager.getAllTasks();
    for (size_t i = 0; i < tasks.size(); ++i) {
        if (i > 0) json += ",";
        const auto& t = tasks[i];
        json += "{\"id\":\"" + t.id + "\",\"title\":\"" + t.title + "\",\"description\":\"" + t.description + "\"";
        json += ",\"priority\":" + String(t.priority) + ",\"completed\":" + String(t.completed ? "true" : "false");
        json += ",\"scheduledTime\":" + String(t.scheduledTime) + ",\"goalId\":\"" + t.goalId + "\"}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiPlannerTask() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String title = m_server.arg("title");
    if (title.isEmpty()) { sendError("Missing task title"); return; }
    String goalId = m_server.arg("goalId");
    uint8_t priority = static_cast<uint8_t>(m_server.arg("priority").toInt());
    unsigned long deadline = static_cast<unsigned long>(m_server.arg("deadline").toInt());
    String id = plannerManager.addTask(goalId, title, m_server.arg("description"), priority, 0, deadline);
    sendSuccess(id.isEmpty() ? "Failed to create task" : ("Task created: " + id));
}

void WebPortal::handleApiPlannerSuggest() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String suggestion = plannerManager.suggestNextAction();
    String json = "{\"suggestion\":\"" + suggestion + "\"}";
    sendJson(json);
}

// ============================================================================
// Automations API
// ============================================================================

void WebPortal::handleApiAutomations() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(4096);
    json += "{\"scripts\":[";
    const auto& scripts = automationManager.getAllScripts();
    for (size_t i = 0; i < scripts.size(); ++i) {
        if (i > 0) json += ",";
        const auto& s = scripts[i];
        json += "{\"id\":\"" + s.id + "\",\"name\":\"" + s.name + "\"";
        json += ",\"enabled\":" + String(s.enabled ? "true" : "false");
        json += ",\"conditions\":" + String(s.conditions.size());
        json += ",\"actions\":" + String(s.actions.size());
        json += "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiAutomationCreate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String name = m_server.arg("name");
    if (name.isEmpty()) { sendError("Missing script name"); return; }
    String id = automationManager.createScript(name);
    sendSuccess(id.isEmpty() ? "Failed to create script" : ("Script created: " + id));
}

void WebPortal::handleApiAutomationDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing script ID"); return; }
    sendSuccess(automationManager.deleteScript(id) ? "Script deleted" : "Script not found");
}

void WebPortal::handleApiAutomationToggle() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing script ID"); return; }
    bool enable = m_server.arg("enable") == "true";
    sendSuccess((enable ? automationManager.enableScript(id) : automationManager.disableScript(id)) ? "Script toggled" : "Script not found");
}

// ============================================================================
// Reflections API
// ============================================================================

void WebPortal::handleApiReflections() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    ReflectionRecord latest = reflectionManager.getLatestReflection();
    String json;
    json.reserve(512);
    json += "{\"hasReflection\":" + String(latest.id.isEmpty() ? "false" : "true");
    if (!latest.id.isEmpty()) {
        json += ",\"id\":\"" + latest.id + "\",\"date\":\"" + latest.date + "\"";
        json += ",\"timestamp\":" + String(latest.timestamp);
        json += ",\"memoriesExtracted\":" + String(latest.memoriesExtracted);
        json += ",\"graphLinksAdded\":" + String(latest.graphLinksAdded);
        json += ",\"productivityScore\":" + String(latest.productivityScore, 1);
        json += ",\"summary\":\"" + latest.summary + "\"";
    }
    json += "}";
    sendJson(json);
}

void WebPortal::handleApiReflectionRun() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    ReflectionRecord rec = reflectionManager.runReflection();
    String json;
    json.reserve(512);
    json += "{\"success\":true,\"message\":\"Reflection cycle completed\"";
    json += ",\"memoriesExtracted\":" + String(rec.memoriesExtracted);
    json += ",\"graphLinksAdded\":" + String(rec.graphLinksAdded);
    json += ",\"productivityScore\":" + String(rec.productivityScore, 1);
    json += "}";
    sendJson(json);
}

// ============================================================================
// Functions API
// ============================================================================

void WebPortal::handleApiFunctions() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(functionRouter.getFunctionDeclarations());
}

void WebPortal::handleApiFunctionExecute() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    if (!m_server.hasArg("plain")) { sendError("No function call body"); return; }
    String body = m_server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { sendError("Invalid JSON", 400); return; }
    String funcName = doc["name"] | "";
    String argsJson;
    if (doc["args"].is<JsonObject>()) {
        JsonDocument argsDoc;
        argsDoc.set(doc["args"].as<JsonObject>());
        serializeJson(argsDoc, argsJson);
    } else {
        argsJson = "{}";
    }
    FuncResult result = functionRouter.execute(funcName, argsJson);
    String json;
    json.reserve(1024);
    json += "{\"success\":" + String(result.success ? "true" : "false");
    json += ",\"message\":\"" + escapeJson(result.message) + "\"";
    if (result.data.length() > 0) json += ",\"data\":" + result.data;
    json += "}";
    sendJson(json);
}

// ============================================================================
// Private Handlers - API Routes (existing)
// ============================================================================

void WebPortal::handleApiStatus() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    // Full status payload from ServiceStatusManager (keeps legacy keys
    // running/uptime/heap_free/wifi_connected/requests for compatibility).
    String json = serviceStatusManager.getStatusJson(m_requestCounter);

    sendJson(json);
}

void WebPortal::handleApiUptime() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    sendJson(uptimeMonitor.toJson());
}

void WebPortal::handleApiWifi() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET)
    {
        String ssid = WiFi.SSID();
        String ip = WiFi.localIP().toString();
        String gateway = WiFi.gatewayIP().toString();

        char json[MAX_JSON_BUFFER];
        snprintf(json, sizeof(json),
            R"({"connected":%s,"ssid":"%s","ip":"%s","gateway":"%s","signal":%d})",
            WiFi.status() == WL_CONNECTED ? "true" : "false",
            ssid.c_str(),
            ip.c_str(),
            gateway.c_str(),
            WiFi.RSSI()
        );

        sendJson(json);
    }
    else if (m_server.method() == HTTP_POST)
    {
        if (!m_server.hasArg("plain"))
        {
            sendError("No JSON body provided", 400);
            return;
        }

        String body = m_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            sendError("Invalid JSON", 400);
            return;
        }

        String ssid = doc["ssid"] | "";
        String password = doc["password"] | "";
        if (ssid.isEmpty()) {
            sendError("Missing SSID", 400);
            return;
        }

        // Persist to NVS so the device reconnects automatically after reboot,
        // then initiate the connection (connect() also persists).
        wifiManager.saveCredentials(ssid.c_str(), password.c_str());
        wifiManager.connect(ssid.c_str(), password.c_str());
        Logger::info("WebPortal", "Wi-Fi credentials saved via API");
        sendSuccess("Wi-Fi credentials saved. Device will reconnect...");
    }
}

// ============================================================================
// V2 Companion Device Control handlers
// ============================================================================

namespace {

const char* conversationStateToString(const ConversationState state) noexcept
{
    switch (state) {
        case ConversationState::IDLE:         return "idle";
        case ConversationState::LISTENING:    return "listening";
        case ConversationState::TRANSCRIBING: return "transcribing";
        case ConversationState::THINKING:     return "thinking";
        case ConversationState::SPEAKING:     return "speaking";
        case ConversationState::PAUSED:       return "paused";
        case ConversationState::COMPLETED:    return "completed";
        case ConversationState::ERROR:        return "error";
        default:                              return "unknown";
    }
}

const char* moodToName(const AuraMood mood) noexcept
{
    switch (mood) {
        case AuraMood::IDLE:            return "idle";
        case AuraMood::LISTENING:       return "listening";
        case AuraMood::THINKING:        return "thinking";
        case AuraMood::PROCESSING:      return "processing";
        case AuraMood::SPEAKING:        return "speaking";
        case AuraMood::HAPPY:           return "happy";
        case AuraMood::SUCCESS:         return "success";
        case AuraMood::REMINDER:        return "reminder";
        case AuraMood::WARNING:         return "warning";
        case AuraMood::ERROR:           return "error";
        case AuraMood::CRITICAL:        return "critical";
        case AuraMood::OTA:             return "ota";
        case AuraMood::OFFLINE:         return "offline";
        case AuraMood::SLEEP:           return "sleep";
        case AuraMood::WAKE:            return "wake";
        case AuraMood::WIFI_CONNECTING: return "wifi_connecting";
        case AuraMood::WIFI_CONNECTED:  return "wifi_connected";
        default:                        return "idle";
    }
}

AuraMood nameToMood(const char* name) noexcept
{
    if (name == nullptr) return AuraMood::IDLE;
    struct MoodEntry { const char* name; AuraMood mood; };
    static const MoodEntry entries[] = {
        {"idle", AuraMood::IDLE},
        {"listening", AuraMood::LISTENING},
        {"thinking", AuraMood::THINKING},
        {"processing", AuraMood::PROCESSING},
        {"speaking", AuraMood::SPEAKING},
        {"happy", AuraMood::HAPPY},
        {"success", AuraMood::SUCCESS},
        {"reminder", AuraMood::REMINDER},
        {"warning", AuraMood::WARNING},
        {"error", AuraMood::ERROR},
        {"critical", AuraMood::CRITICAL},
        {"ota", AuraMood::OTA},
        {"offline", AuraMood::OFFLINE},
        {"sleep", AuraMood::SLEEP},
        {"wake", AuraMood::WAKE},
        {"wifi_connecting", AuraMood::WIFI_CONNECTING},
        {"wifi_connected", AuraMood::WIFI_CONNECTED},
    };
    for (const MoodEntry& entry : entries) {
        if (strcmp(entry.name, name) == 0) return entry.mood;
    }
    return AuraMood::IDLE;
}

String escapeJsonString(const String& value) noexcept
{
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

} // namespace

void WebPortal::handleApiWifiScan() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    const int16_t count = wifiManager.scanNetworks();
    if (count < 0) {
        sendError("Wi-Fi scan failed", 500);
        return;
    }

    String json = "{\"networks\":[";
    uint16_t shown = 0;
    const uint16_t maxNetworks = 32;
    for (int16_t i = 0; i < count && shown < maxNetworks; ++i) {
        char ssid[64] = {0};
        int32_t rssi = 0;
        uint8_t channel = 0;
        bool isOpen = false;
        if (!wifiManager.getNetworkInfo(static_cast<uint16_t>(i), ssid, sizeof(ssid), rssi, channel, isOpen)) {
            continue;
        }
        if (shown > 0) json += ',';
        json += "{\"ssid\":\"";
        json += escapeJsonString(ssid);
        json += "\",\"rssi\":";
        json += String(rssi);
        json += ",\"channel\":";
        json += String(channel);
        json += ",\"open\":";
        json += isOpen ? "true" : "false";
        json += '}';
        ++shown;
    }
    json += "]}";

    sendJson(json);
}

void WebPortal::handleApiWifiForget() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    wifiManager.clearCredentials();
    Logger::info("WebPortal", "Saved Wi-Fi credentials cleared via API");
    sendSuccess("Saved Wi-Fi credentials cleared. Device will disconnect from known networks.");
}

void WebPortal::handleApiDisplayControl() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET) {
        const bool poweredOn = displayManager.getState() != DisplayState::SLEEP;
        char json[MAX_JSON_BUFFER];
        snprintf(json, sizeof(json),
            R"({"on":%s,"brightness":%u,"timeout":%u,"night_mode":%s})",
            poweredOn ? "true" : "false",
            settingsManager.getScreenBrightness(),
            settingsManager.getScreenTimeout(),
            settingsManager.getNightModeEnabled() ? "true" : "false");
        sendJson(json);
        return;
    }

    if (!m_server.hasArg("plain")) {
        sendError("No JSON body provided", 400);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, m_server.arg("plain"))) {
        sendError("Invalid JSON", 400);
        return;
    }

    if (doc["power"].is<bool>()) {
        if (doc["power"].as<bool>()) {
            displayManager.displayOn();
        } else {
            displayManager.displayOff();
        }
    }
    if (doc["brightness"].is<int>()) {
        const uint8_t brightness = static_cast<uint8_t>(constrain(doc["brightness"].as<int>(), 0, 255));
        displayManager.setBrightness(brightness);
        settingsManager.setScreenBrightness(brightness);
    }
    if (doc["invert"].is<bool>()) {
        displayManager.setInverted(doc["invert"].as<bool>());
    }
    if (doc["rotation"].is<int>()) {
        displayManager.setRotation(static_cast<uint8_t>(constrain(doc["rotation"].as<int>(), 0, 3)));
    }
    if (doc["timeout"].is<int>()) {
        settingsManager.setScreenTimeout(static_cast<uint16_t>(constrain(doc["timeout"].as<int>(), 0, 3600)));
    }
    if (doc["text"].is<const char*>()) {
        const char* text = doc["text"].as<const char*>();
        if (text != nullptr && text[0] != '\0') {
            displayManager.showNotification("AURA", text, 5000);
        }
    }
    settingsManager.save();
    sendSuccess("Display updated");
}

void WebPortal::handleApiLedControl() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET) {
        const CRGB color = ledRing.getThemeColor();
        const bool manual = ledRing.isManualControl();
        const AuraMood mood = manual ? ledRing.getManualMood() : ledRing.getMood();
        char json[MAX_JSON_BUFFER];
        snprintf(json, sizeof(json),
            R"({"enabled":%s,"brightness":%u,"mood":"%s","manual":%s,"color":{"r":%u,"g":%u,"b":%u},"disco":%s,"discoBrightness":%u,"discoActive":%s})",
            ledRing.isEnabled() ? "true" : "false",
            settingsManager.getLedBrightness(),
            moodToName(mood),
            manual ? "true" : "false",
            color.r, color.g, color.b,
            ledRing.isDiscoEnabled() ? "true" : "false",
            settingsManager.getDiscoBrightness(),
            ledRing.isDiscoActive() ? "true" : "false");
        sendJson(json);
        return;
    }

    if (!m_server.hasArg("plain")) {
        sendError("No JSON body provided", 400);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, m_server.arg("plain"))) {
        sendError("Invalid JSON", 400);
        return;
    }

    // Manual device-control commands open (or refresh) a temporary manual
    // session: the ring obeys the user's selection until the session is
    // disabled or auto-expires, then automatic idle behaviour (LED off)
    // resumes.
    bool anyCommand = false;
    const bool manualRequested = doc["manual"].is<bool>()
        ? doc["manual"].as<bool>() : true;

    if (doc["enabled"].is<bool>()) {
        anyCommand = true;
        const bool enabled = doc["enabled"].as<bool>();
        if (enabled) {
            ledRing.turnOn();
            if (manualRequested) ledRing.beginManualControl();
        } else {
            ledRing.endManualControl();
            ledRing.turnOff();
        }
        settingsManager.setLedEnabled(enabled);
    }
    if (doc["brightness"].is<int>()) {
        anyCommand = true;
        const uint8_t brightness = static_cast<uint8_t>(constrain(doc["brightness"].as<int>(), 0, 255));
        ledRing.setBrightness(brightness);
        if (manualRequested) ledRing.beginManualControl();
        settingsManager.setLedBrightness(brightness);
    }
    if (doc["mood"].is<const char*>()) {
        anyCommand = true;
        const char* moodName = doc["mood"].as<const char*>();
        if (moodName != nullptr && moodName[0] != '\0') {
            // Manual animation changes drive the ring directly (not the whole
            // face+aura system) so a test never desyncs the OLED face.
            ledRing.setManualMood(nameToMood(moodName));
            if (manualRequested) ledRing.beginManualControl();
        }
    }
    if (doc["r"].is<int>() && doc["g"].is<int>() && doc["b"].is<int>()) {
        anyCommand = true;
        const CRGB color(
            static_cast<uint8_t>(constrain(doc["r"].as<int>(), 0, 255)),
            static_cast<uint8_t>(constrain(doc["g"].as<int>(), 0, 255)),
            static_cast<uint8_t>(constrain(doc["b"].as<int>(), 0, 255)));
        ledRing.setThemeColor(color);
        if (manualRequested) ledRing.beginManualControl();
    }
    // Disco Mode - Companion App only. Turning it on ends any manual test
    // session so Disco owns the ring; turning it off restores the live mood.
    if (doc["disco"].is<bool>()) {
        anyCommand = true;
        const bool disco = doc["disco"].as<bool>();
        ledRing.setDiscoEnabled(disco);
        ledRing.endManualControl();
        Logger::info("WebPortal", "Disco Mode %s (app)", disco ? "enabled" : "disabled");
    }
    if (doc["discoBrightness"].is<int>()) {
        anyCommand = true;
        const uint8_t dbr = static_cast<uint8_t>(
            constrain(doc["discoBrightness"].as<int>(), 10, 100));
        ledRing.setDiscoBrightness(dbr);
        settingsManager.setDiscoBrightness(dbr);
    }
    if (anyCommand) {
        settingsManager.save();
    }
    sendSuccess("LED ring updated");
}

void WebPortal::handleApiAudioControl() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (!m_server.hasArg("plain")) {
        sendError("No JSON body provided", 400);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, m_server.arg("plain"))) {
        sendError("Invalid JSON", 400);
        return;
    }

    if (doc["volume"].is<int>()) {
        const uint8_t volume = static_cast<uint8_t>(constrain(doc["volume"].as<int>(), 0, 100));
        audioManager.setVolume(volume);
        settingsManager.setVolume(volume);
    }
    if (doc["mute"].is<bool>()) {
        const bool mute = doc["mute"].as<bool>();
        if (mute) {
            audioManager.mute();
        } else {
            audioManager.unmute();
        }
    }
    if (doc["output_speaker"].is<bool>()) {
        settingsManager.setOutputToSpeaker(doc["output_speaker"].as<bool>());
    }
    if (doc["test"].is<bool>() && doc["test"].as<bool>()) {
        audioManager.playEarcon(1);
    }
    settingsManager.save();
    sendSuccess("Audio updated");
}

void WebPortal::handleApiMicControl() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (!m_server.hasArg("plain")) {
        sendError("No JSON body provided", 400);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, m_server.arg("plain"))) {
        sendError("Invalid JSON", 400);
        return;
    }

    if (doc["calibrate"].is<bool>() && doc["calibrate"].as<bool>()) {
        audioManager.calibrateNoiseFloor();
        sendSuccess("Noise floor recalibrated");
        return;
    }

    // Privacy mode: blocks the microphone/listening until re-enabled. This is
    // the physical "soft mute" that the touch sensor long-press used to expose;
    // it stays reachable from the Companion App / REST.
    if (doc["privacy"].is<bool>()) {
        conversationManager.setPrivacyMode(doc["privacy"].as<bool>());
        sendSuccess(conversationManager.isPrivacyMode()
                        ? "Microphone privacy enabled"
                        : "Microphone privacy disabled");
        return;
    }

    if (doc["gain"].is<int>()) {
        const uint8_t gain = static_cast<uint8_t>(constrain(doc["gain"].as<int>(), 0, 100));
        audioManager.setMicrophoneGain(gain);
        settingsManager.setMicrophoneGain(gain);
    }
    settingsManager.save();
    sendSuccess("Microphone updated");
}

void WebPortal::handleApiMicLevel() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    float energy = 0.0f;
    float peak = 0.0f;
    audioManager.sampleMicLevel(energy, peak);

    char json[MAX_JSON_BUFFER];
    snprintf(json, sizeof(json),
        R"({"energy":%.3f,"peak":%.3f,"noise_floor":%u,"noise_threshold":%u,"recording":%s,"privacy":%s,"voice_active":%s,"vad_enabled":%s})",
        static_cast<double>(energy),
        static_cast<double>(peak),
        audioManager.getNoiseFloor(),
        audioManager.getNoiseThreshold(),
        audioManager.isRecording() ? "true" : "false",
        conversationManager.isPrivacyMode() ? "true" : "false",
        audioManager.isVoiceActive() ? "true" : "false",
        audioManager.isVoiceActivityMonitoringEnabled() ? "true" : "false");
    sendJson(json);
}

void WebPortal::handleApiSdDiagnostics() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    const bool initialized = storageManager.isInitialized();
    const bool mounted = storageManager.isSDMounted();
    const uint64_t cardSize = storageManager.getSDCardSize();
    const uint32_t spiHz = storageManager.getSDSpiFrequencyHz();

    size_t totalBytes = 0, usedBytes = 0, freeBytes = 0;
    if (mounted) {
        storageManager.getStatistics(StorageType::SD_CARD, totalBytes, usedBytes, freeBytes);
    }

    float readKBs = 0.0f, writeKBs = 0.0f;
    if (mounted) {
        float readBps = 0.0f, writeBps = 0.0f;
        storageManager.runSDSpeedTest(readBps, writeBps);
        readKBs = readBps / 1024.0f;
        writeKBs = writeBps / 1024.0f;
    }

    String json;
    json.reserve(512);
    json += "{\"success\":true,";
    json += "\"initialized\":" + String(initialized ? "true" : "false") + ",";
    json += "\"card_detected\":" + String(mounted ? "true" : "false") + ",";
    json += "\"card_type\":\"" + String(storageManager.getSDCardTypeName()) + "\",";
    json += "\"capacity_bytes\":" + String(static_cast<unsigned long long>(cardSize)) + ",";
    json += "\"capacity_mb\":" + String(static_cast<unsigned long long>(cardSize / (1024ULL * 1024ULL))) + ",";
    json += "\"filesystem\":\"" + String(mounted ? "FAT32" : "none") + "\",";
    json += "\"spi_frequency_hz\":" + String(spiHz) + ",";
    json += "\"spi_frequency_mhz\":" + String(static_cast<float>(spiHz) / 1000000.0f, 1) + ",";
    json += "\"cs_pin\":" + String(SD_CS_PIN) + ",";
    json += "\"last_error\":\"" + escapeJson(String(storageManager.getSDLastError())) + "\",";
    json += "\"free_space\":" + String(static_cast<unsigned long long>(freeBytes)) + ",";
    json += "\"used_space\":" + String(static_cast<unsigned long long>(usedBytes)) + ",";
    json += "\"total_space\":" + String(static_cast<unsigned long long>(totalBytes)) + ",";
    json += "\"read_speed_kbs\":" + String(readKBs, 1) + ",";
    json += "\"write_speed_kbs\":" + String(writeKBs, 1);
    json += "}";
    sendJson(json);
}

void WebPortal::handleApiSettings() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET)
    {
        String apiKeyStatus = geminiClient.hasApiKey() ? geminiClient.getMaskedApiKey() : "";
        char json[MAX_JSON_BUFFER];
        snprintf(json, sizeof(json),
            R"({"device_name":"AURA","version":"1.0.0","api_key":"%s","has_key":%s,"build_date":"%s","build_time":"%s","output_oled":%s,"output_speaker":%s,"output_companion":%s})",
            apiKeyStatus.isEmpty() ? "" : apiKeyStatus.c_str(),
            geminiClient.hasApiKey() ? "true" : "false",
            __DATE__,
            __TIME__,
            settingsManager.isInitialized() && settingsManager.getOutputToOled() ? "true" : "false",
            settingsManager.isInitialized() && settingsManager.getOutputToSpeaker() ? "true" : "false",
            settingsManager.isInitialized() && settingsManager.getOutputToCompanion() ? "true" : "false"
        );

        sendJson(json);
    }
    else if (m_server.method() == HTTP_POST)
    {
        if (!m_server.hasArg("plain"))
        {
            sendError("No JSON body provided", 400);
            return;
        }

        String body = m_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            sendError("Invalid JSON", 400);
            return;
        }

        String apiKey = doc["api_key"] | "";
        if (!apiKey.isEmpty()) {
            geminiClient.setApiKey(apiKey);
            Logger::info("WebPortal", "API key updated via settings");
        }

        String deviceName = doc["device_name"] | "";
        if (!deviceName.isEmpty()) {
            Logger::info("WebPortal", "Device name updated: %s", deviceName.c_str());
        }

        // Output routing toggles (OLED / Speaker / Companion)
        bool settingsChanged = false;
        if (doc["output_oled"].is<bool>()) {
            settingsManager.setOutputToOled(doc["output_oled"].as<bool>());
            settingsChanged = true;
        }
        if (doc["output_speaker"].is<bool>()) {
            settingsManager.setOutputToSpeaker(doc["output_speaker"].as<bool>());
            settingsChanged = true;
        }
        if (doc["output_companion"].is<bool>()) {
            settingsManager.setOutputToCompanion(doc["output_companion"].as<bool>());
            settingsChanged = true;
        }
        if (settingsChanged) {
            settingsManager.save();
            Logger::info("WebPortal", "Output routing settings updated");
        }

        Logger::info("WebPortal", "Settings API update request received");
        sendSuccess("Settings updated");
    }
}

// ============================================================================
// Auth Methods
// ============================================================================

bool WebPortal::isAuthenticated() noexcept {
    if (m_sessionToken.isEmpty() || m_sessionCreated == 0) {
        return false;
    }
    if (millis() - m_sessionCreated >= kSessionTimeoutMs) {
        m_sessionToken.clear();
        m_sessionCreated = 0;
        for (size_t i = 0; i < WS_MAX_CLIENTS; ++i) m_wsAuth[i] = false;
        return false;
    }
    if (!m_server.hasHeader("X-Auth-Token")) {
        return false;
    }
    const String& token = m_server.header("X-Auth-Token");
    if (token.length() != m_sessionToken.length()) {
        return false;
    }
    // Constant-time comparison
    uint8_t diff = 0;
    for (size_t i = 0; i < token.length(); ++i) {
        diff |= static_cast<uint8_t>(token[i]) ^ static_cast<uint8_t>(m_sessionToken[i]);
    }
    return diff == 0;
}

bool WebPortal::isAuthenticatedOrReject() noexcept {
    if (isAuthenticated()) {
        return true;
    }
    Logger::warning("WebPortal", "Auth rejected: hasHeader=%d hasSession=%d uri=%s",
        m_server.hasHeader("X-Auth-Token") ? 1 : 0,
        !m_sessionToken.isEmpty() ? 1 : 0,
        m_server.uri().c_str());
    sendError("Unauthorized", 401);
    return false;
}

String WebPortal::generateSessionToken() noexcept {
    String token;
    token.reserve(32);
    for (int i = 0; i < 16; ++i) {
        uint8_t r = (uint8_t)esp_random();
        token += "0123456789abcdef"[r & 0x0F];
        token += "0123456789abcdef"[(r >> 4) & 0x0F];
    }
    return token;
}

bool WebPortal::secureEquals(const String& a, const String& b) noexcept {
    if (a.length() != b.length()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.length(); ++i) {
        diff |= static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i]);
    }
    return diff == 0;
}

String WebPortal::generateRandomPassword() noexcept {
    String pwd;
    pwd.reserve(16);
    for (int i = 0; i < 16; ++i) {
        uint8_t r = static_cast<uint8_t>(esp_random());
        pwd += "0123456789abcdef"[r & 0x0F];
        pwd += "0123456789abcdef"[(r >> 4) & 0x0F];
    }
    return pwd;
}

bool WebPortal::loadAuthCredentials() noexcept {
    m_authPrefs.begin(kAuthNamespace, true);
    uint32_t storedVersion = m_authPrefs.getUInt("version", 0);
    m_authUsername = m_authPrefs.getString("username", kDefaultUsername);
    m_authPassword = m_authPrefs.getString("password", "");
    m_authMustChange = m_authPrefs.getBool("must_change", false);
    m_authPrefs.end();

    // FIRST BOOT: there is no configured password and no prior credential
    // record. In production a strong random admin password is generated,
    // persisted in NVS, and printed to the Serial monitor (no known default).
    // In development builds the well-known development credentials are used
    // for local testing only.
    if (m_authPassword.isEmpty() && storedVersion == 0) {
#if AURA_DEVELOPMENT_MODE
        m_authPassword = AURA_DEV_WEB_PASSWORD;
        m_authMustChange = false;
        saveAuthCredentials(m_authUsername, m_authPassword);

        Serial.println();
        Serial.println("======================================================");
        Serial.println("  AURA DEVELOPMENT-MODE ADMIN CREDENTIALS");
        Serial.print("    Username: "); Serial.println(m_authUsername);
        Serial.print("    Password: "); Serial.println(m_authPassword);
        Serial.println("  (AURA_DEVELOPMENT_MODE is ON - do NOT ship this build!)");
        Serial.println("======================================================");
        Serial.println();
        Logger::warning("WebPortal", "Dev-mode admin credentials applied (for local testing only)");
        return true;
#else
        m_authPassword = generateRandomPassword();
        m_authMustChange = true;
        saveAuthCredentials(m_authUsername, m_authPassword);

        // Production: never dump the first-boot password to the serial log.
        Logger::warning("WebPortal", "First-boot admin credentials generated and stored in NVS");
        return true;
#endif
    }

    // Version migration: NEVER reset user credentials to defaults. Only persist
    // the stored credentials again under the new layout (e.g. adding the
    // must_change flag), so the user-chosen password is preserved.
    if (storedVersion != kAuthCredVersion) {
        Logger::info("WebPortal", "Auth NVS layout migrated (v%u -> v%u)",
            static_cast<unsigned>(storedVersion), static_cast<unsigned>(kAuthCredVersion));
        saveAuthCredentials(m_authUsername, m_authPassword);
    }

    // Safety net for exotic states: never boot with an empty password.
    if (m_authPassword.isEmpty()) {
#if AURA_DEVELOPMENT_MODE
        m_authPassword = AURA_DEV_WEB_PASSWORD;
        m_authMustChange = false;
#else
        m_authPassword = generateRandomPassword();
        m_authMustChange = true;
#endif
                saveAuthCredentials(m_authUsername, m_authPassword);
#if AURA_DEVELOPMENT_MODE
        Serial.print("AURA admin password: "); Serial.println(m_authPassword);
#else
        // Production: refresh the stored password without dumping it to serial.
        Logger::warning("WebPortal", "Regenerated empty admin password (stored in NVS)");
#endif
    }

    return true;
}

bool WebPortal::saveAuthCredentials(const String& username, const String& password) noexcept {
    m_authPrefs.begin(kAuthNamespace, false);
    bool ok = true;
    ok = ok && m_authPrefs.putString("username", username);
    ok = ok && m_authPrefs.putString("password", password);
    ok = ok && m_authPrefs.putUInt("version", kAuthCredVersion);
    ok = ok && m_authPrefs.putBool("must_change", m_authMustChange);
    m_authPrefs.end();
    if (ok) {
        m_authUsername = username;
        m_authPassword = password;
    }
    return ok;
}

void WebPortal::resetLoginTracker(const String& ip) noexcept {
    m_loginTrackers.erase(ip);
}

bool WebPortal::isLoginLockedOut(const String& ip) noexcept {
    auto it = m_loginTrackers.find(ip);
    if (it == m_loginTrackers.end()) return false;
    return millis() < it->second.lockoutUntil;
}

void WebPortal::recordFailedLogin(const String& ip) noexcept {
    LoginTracker& tracker = m_loginTrackers[ip];
    tracker.attempts++;
    if (tracker.attempts >= kMaxLoginAttempts) {
        tracker.lockoutUntil = millis() + kLoginLockoutDurationMs;
        tracker.attempts = 0;
        Logger::warning("WebPortal", "Login locked out for %lu ms from %s",
            kLoginLockoutDurationMs, ip.c_str());
    }

    // Bound memory usage: drop stale trackers once the map grows too large.
    if (m_loginTrackers.size() > kMaxTrackedIps) {
        for (auto it = m_loginTrackers.begin(); it != m_loginTrackers.end();) {
            if (it->second.lockoutUntil == 0 || millis() > it->second.lockoutUntil) {
                it = m_loginTrackers.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void WebPortal::handleApiAuthLogin() noexcept {
    m_requestCounter++;

    String clientIp = m_server.client().remoteIP().toString();

    // Per-IP rate limiting check (prevents a single attacker locking out every
    // other client and bounds brute-force attempts per source address).
    if (isLoginLockedOut(clientIp)) {
        unsigned long remaining = (m_loginTrackers[clientIp].lockoutUntil - millis()) / 1000UL;
        Logger::warning("WebPortal", "Login locked out for %lu more seconds from %s",
            remaining, clientIp.c_str());
        sendError("Too many attempts. Try again later.", 429);
        return;
    }

    if (!m_server.hasArg("plain")) {
        sendError("No JSON body", 400);
        return;
    }

    String body = m_server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        sendError("Invalid JSON", 400);
        return;
    }

    String username = doc["username"] | "";
    String password = doc["password"] | "";

    if (username.isEmpty() || password.isEmpty()) {
        sendError("Missing username or password", 400);
        return;
    }

    if (username != m_authUsername || !secureEquals(password, m_authPassword)) {
        recordFailedLogin(clientIp);
        Logger::warning("WebPortal", "Failed login attempt from %s", clientIp.c_str());
        securityManager.LogAudit(AuditEventType::LOGIN, clientIp,
                                 "Login failed", false);
        sendError("Invalid credentials", 401);
        return;
    }

    // Successful login â€” reset per-IP rate limiter
    resetLoginTracker(clientIp);

    m_sessionToken = generateSessionToken();
    m_sessionCreated = millis();
    securityManager.Authenticate(m_sessionToken);

    String json;
    json.reserve(192);
    json += "{\"success\":true,\"token\":\"";
    json += m_sessionToken;
    json += "\",\"expiresIn\":" + String(kSessionTimeoutMs / 1000UL);
    json += ",\"must_change\":" + String(m_authMustChange ? "true" : "false");
    json += "}";
    sendJson(json, 200);

    Logger::info("WebPortal", "User logged in from %s, session token generated",
        clientIp.c_str());
}

void WebPortal::handleApiAuthLogout() noexcept {
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    m_sessionToken.clear();
    m_sessionCreated = 0;
    for (size_t i = 0; i < WS_MAX_CLIENTS; ++i) m_wsAuth[i] = false;
    securityManager.Deauthenticate();
    sendSuccess("Logged out");
    Logger::info("WebPortal", "User logged out");
}

void WebPortal::handleApiAuthStatus() noexcept {
    m_requestCounter++;
    bool authenticated = isAuthenticated();
    String json;
    json.reserve(192);
    json += "{\"authenticated\":" + String(authenticated ? "true" : "false");
    json += ",\"hasSession\":" + String(!m_sessionToken.isEmpty() ? "true" : "false");
    json += ",\"must_change\":" + String(m_authMustChange ? "true" : "false");
    if (authenticated) {
        unsigned long remaining = (kSessionTimeoutMs - (millis() - m_sessionCreated)) / 1000UL;
        json += ",\"expiresIn\":" + String(remaining);
    }
    json += "}";
    sendJson(json, 200);
}

void WebPortal::handleApiAuthChangePassword() noexcept {
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (!m_server.hasArg("plain")) {
        sendError("No JSON body", 400);
        return;
    }

    String body = m_server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        sendError("Invalid JSON", 400);
        return;
    }

    String currentPassword = doc["current_password"] | "";
    String newPassword = doc["new_password"] | "";

    if (!secureEquals(currentPassword, m_authPassword)) {
        securityManager.LogAudit(AuditEventType::PERMISSION_CHANGE, "webportal",
                                 "Password change rejected (wrong current password)", false);
        sendError("Current password is incorrect", 401);
        return;
    }

    if (newPassword.length() < 8) {
        sendError("New password must be at least 8 characters", 400);
        return;
    }

    m_authMustChange = false;
    saveAuthCredentials(m_authUsername, newPassword);
    securityManager.LogAudit(AuditEventType::PERMISSION_CHANGE, "webportal",
                             "Admin password changed", true);
    sendSuccess("Password updated");
    Logger::info("WebPortal", "Admin password changed");
}

// ============================================================================
// Startup Greeting API
// ============================================================================

void WebPortal::handleApiStartupSettings() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET)
    {
        const auto& s = startupGreetingManager.getSettings();
        String json;
        json.reserve(512);
        json += "{";
        json += "\"enabled\":" + String(s.enabled ? "true" : "false") + ",";
        json += "\"speakGreeting\":" + String(s.speakGreeting ? "true" : "false") + ",";
        json += "\"showMark\":" + String(s.showMark ? "true" : "false") + ",";
        json += "\"showCodename\":" + String(s.showCodename ? "true" : "false") + ",";
        json += "\"showSemVer\":" + String(s.showSemVer ? "true" : "false") + ",";
        json += "\"showBuildDate\":" + String(s.showBuildDate ? "true" : "false") + ",";
        json += "\"showAge\":" + String(s.showAge ? "true" : "false") + ",";
        json += "\"showPersonality\":" + String(s.showPersonality ? "true" : "false") + ",";
        json += "\"showWifi\":" + String(s.showWifi ? "true" : "false") + ",";
        json += "\"showStorage\":" + String(s.showStorage ? "true" : "false") + ",";
        json += "\"displayDurationSec\":" + String(s.displayDurationSec);
        json += "}";
        sendJson(json);
    }
    else if (m_server.method() == HTTP_POST)
    {
        StartupGreetingSettings s = startupGreetingManager.getSettings();
        const String enabled = m_server.arg("enabled");
        if (!enabled.isEmpty()) s.enabled = (enabled == "true");
        const String speak = m_server.arg("speakGreeting");
        if (!speak.isEmpty()) s.speakGreeting = (speak == "true");
        const String mark = m_server.arg("showMark");
        if (!mark.isEmpty()) s.showMark = (mark == "true");
        const String code = m_server.arg("showCodename");
        if (!code.isEmpty()) s.showCodename = (code == "true");
        const String semver = m_server.arg("showSemVer");
        if (!semver.isEmpty()) s.showSemVer = (semver == "true");
        const String bdate = m_server.arg("showBuildDate");
        if (!bdate.isEmpty()) s.showBuildDate = (bdate == "true");
        const String age = m_server.arg("showAge");
        if (!age.isEmpty()) s.showAge = (age == "true");
        const String pers = m_server.arg("showPersonality");
        if (!pers.isEmpty()) s.showPersonality = (pers == "true");
        const String wifi = m_server.arg("showWifi");
        if (!wifi.isEmpty()) s.showWifi = (wifi == "true");
        const String stor = m_server.arg("showStorage");
        if (!stor.isEmpty()) s.showStorage = (stor == "true");
        const String dur = m_server.arg("displayDurationSec");
        if (!dur.isEmpty()) s.displayDurationSec = static_cast<uint8_t>(dur.toInt());
        startupGreetingManager.updateSettings(s);
        sendSuccess("Startup greeting settings saved");
    }
}

// ============================================================================
// Version API
// ============================================================================

void WebPortal::handleApiVersion() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    char buildBuf[24];
    {
        static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        const char* src = aura::version::kBuildDate;
        char monStr[4] = {};
        int day = 0, year = 0;
        if (sscanf(src, "%3s %d %d", monStr, &day, &year) == 3) {
            int mon = 0;
            for (int i = 0; i < 12; ++i) {
                if (strcmp(monStr, months[i]) == 0) { mon = i + 1; break; }
            }
            if (mon > 0) {
                snprintf(buildBuf, sizeof(buildBuf), "%04d.%02d.%02d", year, mon, day);
            } else {
                snprintf(buildBuf, sizeof(buildBuf), "%s", src);
            }
        } else {
            snprintf(buildBuf, sizeof(buildBuf), "%s", src);
        }
    }

    char json[512];
    snprintf(json, sizeof(json),
        R"({"mark":"%s","codename":"%s","version":"%s","build":"%s","channel":"%s"})",
        AURA_MARK_ROMAN,
        aura::version::kCodename,
        AURA_SEMVER,
        buildBuf,
        aura::version::kChannel
    );
    sendJson(json);
}

// ============================================================================
// Offline AI API Handlers
// ============================================================================

void WebPortal::handleApiOfflineAIStatus() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(tinyAIManager.getStatusJSON());
}

void WebPortal::handleApiOfflineAIEnable() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    tinyAIManager.setEnabled(true);
    sendJson(R"({"success":true,"message":"Offline AI enabled"})");
}

void WebPortal::handleApiOfflineAIDisable() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    tinyAIManager.setEnabled(false);
    sendJson(R"({"success":true,"message":"Offline AI disabled"})");
}

void WebPortal::handleApiOfflineAITest() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String testResponse;
    String testInput = m_server.arg("text");
    if (testInput.isEmpty()) {
        testInput = "Hello, what can you do?";
    }

    bool ok = tinyAIManager.process(testInput, testResponse);

    char json[512];
    if (ok) {
        snprintf(json, sizeof(json),
            R"({"success":true,"input":"%s","response":"%s"})",
            testInput.c_str(), testResponse.c_str());
    } else {
        snprintf(json, sizeof(json),
            R"({"success":false,"input":"%s","error":"No matching response"})",
            testInput.c_str());
    }
    sendJson(json);
}

// ============================================================================
// Private Helpers - JSON
// ============================================================================

void WebPortal::sendJson(const String& json, const int code) noexcept
{
    m_server.send(code, "application/json; charset=utf-8", json);
}

void WebPortal::sendSuccess(const String& message, const String& data) noexcept
{
    String json;
    json.reserve(HTML_BUFFER_JSON);
    json += "{\"success\":true,\"message\":\"";
    json += escapeJson(message);
    json += "\"";

    if (data.length() > 0)
    {
        json += ",\"data\":";
        json += data;
    }

    json += "}";
    sendJson(json, 200);
}

void WebPortal::sendError(const String& message, const int code) noexcept
{
    String json;
    json.reserve(HTML_BUFFER_JSON);
    json += "{\"success\":false,\"error\":\"";
    json += escapeJson(message);
    json += "\"}";
    sendJson(json, code);
}

// ============================================================================
// V2.1 Intelligence Layer API Handlers
// ============================================================================

// ============================================================================
// Decision API
// ============================================================================

void WebPortal::handleApiDecisions() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String type = m_server.arg("type");
    sendJson(decisionManager.getDecisionsJson(type));
}

void WebPortal::handleApiDecisionMake() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String question = m_server.arg("question");
    if (question.isEmpty()) { sendError("Missing question"); return; }
    String type = m_server.arg("type");
    if (type.isEmpty()) type = "general";
    
    // Parse option IDs from comma-separated arg
    std::vector<String> optionIds;
    String optsStr = m_server.arg("options");
    int pos = 0;
    while (pos < (int)optsStr.length()) {
        int comma = optsStr.indexOf(',', pos);
        if (comma < 0) { optionIds.push_back(optsStr.substring(pos)); break; }
        optionIds.push_back(optsStr.substring(pos, comma));
        pos = comma + 1;
    }
    
    if (optionIds.empty()) {
        // Create default options
        String opt1 = decisionManager.addOption("Option A", "First option", 0.5f, 0.3f, 0.2f);
        String opt2 = decisionManager.addOption("Option B", "Second option", 0.7f, 0.5f, 0.6f);
        optionIds.push_back(opt1);
        optionIds.push_back(opt2);
    }
    
    String decisionId = decisionManager.makeDecision(question, optionIds, type);
    if (decisionId.isEmpty()) { sendError("Failed to make decision"); return; }
    
    String json;
    json.reserve(512);
    json += "{\"success\":true,\"decisionId\":\"" + decisionId + "\"";
    json += ",\"explanation\":\"" + decisionManager.explainDecision(decisionId) + "\"}";
    sendJson(json);
}

void WebPortal::handleApiDecisionExplain() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing decision ID"); return; }
    String explanation = decisionManager.explainDecision(id);
    String json = "{\"explanation\":\"" + explanation + "\"}";
    sendJson(json);
}

void WebPortal::handleApiDecisionRank() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String ids = m_server.arg("ids");
    std::vector<String> optionIds;
    int pos = 0;
    while (pos < (int)ids.length()) {
        int comma = ids.indexOf(',', pos);
        if (comma < 0) { optionIds.push_back(ids.substring(pos)); break; }
        optionIds.push_back(ids.substring(pos, comma));
        pos = comma + 1;
    }
    auto ranked = decisionManager.rankByPriority(optionIds);
    String json; json.reserve(1024);
    json += "{\"ranked\":[";
    for (size_t i = 0; i < ranked.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + ranked[i].id + "\",\"name\":\"" + ranked[i].name + "\",\"score\":" + String(ranked[i].priorityScore, 2) + "}";
    }
    json += "]}";
    sendJson(json);
}

// ============================================================================
// Learning API
// ============================================================================

void WebPortal::handleApiLearning() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(learningManager.getPatternsJson());
}

void WebPortal::handleApiLearningObserve() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String data = m_server.arg("data");
    if (data.isEmpty()) { sendError("Missing observation data"); return; }
    String category = m_server.arg("category");
    learningManager.observe(data, category);
    sendSuccess("Observation recorded");
}

void WebPortal::handleApiPatterns() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(learningManager.getPatternsJson());
}

// ============================================================================
// Recommendations API
// ============================================================================

void WebPortal::handleApiRecommendations() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    bool activeOnly = m_server.arg("active") != "false";
    sendJson(executiveAssistant.getRecommendationsJson(activeOnly));
}

void WebPortal::handleApiRecommendDismiss() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing recommendation ID"); return; }
    executiveAssistant.dismissRecommendation(id);
    sendSuccess("Recommendation dismissed");
}

void WebPortal::handleApiRecommendAct() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing recommendation ID"); return; }
    executiveAssistant.markRecommendationActed(id);
    sendSuccess("Recommendation marked acted");
}

void WebPortal::handleApiRecommendGenerate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    executiveAssistant.generateAllRecommendations();
    sendSuccess("Recommendations generated");
}

// ============================================================================
// Predictions API
// ============================================================================

void WebPortal::handleApiPredictions() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    float minProb = m_server.arg("min_probability").toFloat();
    sendJson(predictionManager.getPredictionsJson(minProb));
}

void WebPortal::handleApiPredictionRun() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    predictionManager.runAllPredictions();
    sendSuccess("Predictions generated");
}

// ============================================================================
// Documents API
// ============================================================================

void WebPortal::handleApiDocuments() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String filter = m_server.arg("ext");
    sendJson(documentManager.getDocumentsJson(filter));
}

void WebPortal::handleApiDocumentUpload() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String filename = m_server.arg("filename");
    String content = m_server.arg("content");
    if (filename.isEmpty() || content.isEmpty()) { sendError("Missing filename or content"); return; }
    String title = m_server.arg("title");
    String description = m_server.arg("description");
    String tags = m_server.arg("tags");
    if (documentManager.storeDocument(filename, content, title, description, tags)) {
        sendSuccess("Document stored: " + filename);
    } else {
        sendError("Failed to store document");
    }
}

void WebPortal::handleApiDocumentDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing document ID"); return; }
    sendSuccess(documentManager.deleteDocument(id) ? "Document deleted" : "Document not found");
}

void WebPortal::handleApiDocumentContent() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing document ID"); return; }
    String content;
    if (documentManager.getDocumentContent(id, content)) {
        String json = "{\"content\":\"" + content + "\"}";
        sendJson(json);
    } else {
        sendError("Document not found");
    }
}

void WebPortal::handleApiDocumentSearch() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String query = m_server.arg("q");
    if (query.isEmpty()) { sendError("Missing search query"); return; }
    auto results = documentManager.search(query);
    String json; json.reserve(4096);
    json += "{\"results\":[";
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + results[i].id + "\",\"name\":\"" + results[i].filename + "\",\"title\":\"" + results[i].title + "\"}";
    }
    json += "]}";
    sendJson(json);
}

// ============================================================================
// Workspaces API
// ============================================================================

void WebPortal::handleApiWorkspaces() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(workspaceManager.getWorkspacesJson());
}

void WebPortal::handleApiWorkspaceCreate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String name = m_server.arg("name");
    if (name.isEmpty()) { sendError("Missing workspace name"); return; }
    String desc = m_server.arg("description");
    String id = workspaceManager.createWorkspace(name, desc);
    sendSuccess(id.isEmpty() ? "Failed to create workspace" : ("Workspace created: " + id));
}

void WebPortal::handleApiWorkspaceDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing workspace ID"); return; }
    sendSuccess(workspaceManager.deleteWorkspace(id) ? "Workspace deleted" : "Workspace not found");
}

void WebPortal::handleApiWorkspaceActivate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing workspace ID"); return; }
    sendSuccess(workspaceManager.activateWorkspace(id) ? "Workspace activated" : "Workspace not found");
}

void WebPortal::handleApiWorkspaceMember() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String wsId = m_server.arg("workspace_id");
    String entityType = m_server.arg("entity_type");
    String entityId = m_server.arg("entity_id");
    String action = m_server.arg("action");
    if (wsId.isEmpty() || entityType.isEmpty() || entityId.isEmpty()) {
        sendError("Missing parameters"); return;
    }
    bool ok = false;
    if (action == "remove") {
        ok = workspaceManager.removeMember(wsId, entityId);
    } else {
        ok = workspaceManager.addMember(wsId, entityType, entityId);
    }
    sendSuccess(ok ? "Member updated" : "Operation failed");
}

// ============================================================================
// Vault API
// ============================================================================

void WebPortal::handleApiVault() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(vaultManager.getVaultJson());
}

void WebPortal::handleApiVaultSet() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String key = m_server.arg("key");
    String value = m_server.arg("value");
    String category = m_server.arg("category");
    if (key.isEmpty() || value.isEmpty()) { sendError("Missing key or value"); return; }
    sendSuccess(vaultManager.setSecret(key, value, category) ? "Secret stored" : "Failed to store secret");
}

void WebPortal::handleApiVaultDelete() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String key = m_server.arg("key");
    if (key.isEmpty()) { sendError("Missing key"); return; }
    sendSuccess(vaultManager.deleteSecret(key) ? "Secret deleted" : "Secret not found");
}

void WebPortal::handleApiVaultBackup() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendSuccess(vaultManager.exportBackup() ? "Backup exported" : "Backup failed");
}

// ============================================================================
// Developer API
// ============================================================================

void WebPortal::handleApiDeveloper() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(2048);
    json += "{\"developer\":";
    json += performanceManager.getDeveloperMetricsJson();
    json += ",\"diagnostics\":";
    diagnosticsManager.runAllTests();
    json += diagnosticsManager.getResultsJson();
    json += ",\"system\":{";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    json += "}}";
    sendJson(json);
}

void WebPortal::handleApiDeveloperExport() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String diag = performanceManager.exportDiagnostics();
    // Add system info
    diag += "\nSystem Uptime: " + String(millis() / 1000) + " seconds\n";
    diag += "WiFi Connected: " + String(WiFi.status() == WL_CONNECTED ? "Yes" : "No") + "\n";
    
    String json;
    json.reserve(2048);
    json += "{";
    json += "\"success\":true,";
    json += "\"diagnostics\":\"" + diag + "\"";
    json += "}";
    sendJson(json);
}

// ============================================================================
// V3.0 API Handlers
// ============================================================================

void WebPortal::handleApiDashboardSummary() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    size_t memCount = memoryManager.isInitialized() ? memoryManager.memoryCount() : 0;
    size_t wsCount = 0;
    if (workspaceManager.isInitialized()) {
        for (auto& w : workspaceManager.getAllWorkspaces()) {
            if (w.active) wsCount++;
        }
    }
    size_t recCount = executiveAssistant.isInitialized() ? executiveAssistant.getActiveRecommendations().size() : 0;
    std::vector<Reminder> reminders;
    size_t remCount = reminderManager.isInitialized() ? reminderManager.getReminders(reminders) : 0;
    size_t convCount = conversationManager.isInitialized() ? conversationManager.getHistory().size() : 0;
    size_t studyMin = studyManager.isInitialized() ? studyManager.totalStudyMinutes() : 0;
    size_t pairCount = companionManager.isInitialized() ? companionManager.pairedDeviceCount() : 0;

    String json;
    json.reserve(512);
    json += "{";
    json += "\"memoryCount\":" + String(memCount) + ",";
    json += "\"activeWorkspaces\":" + String(wsCount) + ",";
    json += "\"pendingRecommendations\":" + String(recCount) + ",";
    json += "\"upcomingReminders\":" + String(remCount) + ",";
    json += "\"recentConversations\":" + String(convCount) + ",";
    json += "\"studyMinutes\":" + String(studyMin) + ",";
    json += "\"pairedDevices\":" + String(pairCount) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    sendJson(json);
}

void WebPortal::handleApiDashboardRecent() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson("{\"entries\":[]}");
}

// Memory Importance Engine

void WebPortal::handleApiMemoryPin() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing memory ID"); return; }
    sendSuccess(memoryManager.setPin(id, true) ? "Memory pinned" : "Memory not found");
}

void WebPortal::handleApiMemoryArchive() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing memory ID"); return; }
    sendSuccess(memoryManager.setArchive(id, true) ? "Memory archived" : "Memory not found");
}

void WebPortal::handleApiMemoryPinned() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    auto pinned = memoryManager.getPinned();
    String json; json.reserve(2048);
    json += "{\"pinned\":[";
    for (size_t i = 0; i < pinned.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + pinned[i].id + "\",\"key\":\"" + pinned[i].key + "\",\"value\":\"" + pinned[i].value + "\"}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiMemoryArchived() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    auto archived = memoryManager.getArchived();
    String json; json.reserve(2048);
    json += "{\"archived\":[";
    for (size_t i = 0; i < archived.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + archived[i].id + "\",\"key\":\"" + archived[i].key + "\",\"value\":\"" + archived[i].value + "\"}";
    }
    json += "]}";
    sendJson(json);
}

// Versioned Memory

void WebPortal::handleApiMemoryRevisions() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing memory ID"); return; }
    auto revisions = memoryManager.getRevisions(id);
    String json; json.reserve(1024);
    json += "{\"revisions\":[";
    for (size_t i = 0; i < revisions.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + revisions[i].id + "\",\"ts\":" + String(revisions[i].timestamp) + ",\"previousValue\":\"" + revisions[i].previousValue + "\"}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiMemoryRestore() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String revisionId = m_server.arg("revisionId");
    if (revisionId.isEmpty()) { sendError("Missing revision ID"); return; }
    sendSuccess(memoryManager.restoreRevision(revisionId) ? "Memory restored" : "Restore failed");
}

void WebPortal::handleApiMemoryCompare() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id1 = m_server.arg("id1");
    String id2 = m_server.arg("id2");
    if (id1.isEmpty() || id2.isEmpty()) { sendError("Missing memory IDs"); return; }
    auto diff = memoryManager.compareRevisions(id1, id2);
    String json = "{\"diff\":\"" + diff + "\"}";
    sendJson(json);
}

// Skill Studio

void WebPortal::handleApiSkillsStudioUpdate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing skill ID"); return; }
    SkillEntry updates;
    String name = m_server.arg("name");
    if (!name.isEmpty()) updates.name = name;
    String trigger = m_server.arg("trigger");
    if (!trigger.isEmpty()) updates.voiceTrigger = trigger;
    sendSuccess(skillManager.updateSkill(id, updates) ? "Skill updated" : "Skill not found");
}

void WebPortal::handleApiSkillsStudioDuplicate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing skill ID"); return; }
    sendSuccess(skillManager.duplicateSkill(id) ? "Skill duplicated" : "Skill not found");
}

void WebPortal::handleApiSkillsStudioExport() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    if (id.isEmpty()) { sendError("Missing skill ID"); return; }
    String exported = skillManager.exportSkill(id);
    String json = "{\"skill\":" + exported + "}";
    sendJson(json);
}

void WebPortal::handleApiSkillsStudioImport() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    if (!m_server.hasArg("plain")) { sendError("No skill data"); return; }
    String data = m_server.arg("plain");
    sendSuccess(skillManager.importSkill(data) ? "Skill imported" : "Failed to import skill");
}

// NL Automation

void WebPortal::handleApiAutomationNLPatterns() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    auto patterns = automationManager.getAllNLPatterns();
    String json; json.reserve(2048);
    json += "{\"patterns\":[";
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + patterns[i].id + "\",\"pattern\":\"" + patterns[i].pattern + "\"";
        json += ",\"actionType\":\"" + patterns[i].actionType + "\",\"priority\":" + String(patterns[i].priority) + "}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiAutomationNLMatch() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String text = m_server.arg("text");
    if (text.isEmpty()) { sendError("Missing text"); return; }
    NLPattern result = automationManager.matchNL(text);
    String json;
    json.reserve(256);
    json += "{\"matched\":" + String(result.id.isEmpty() ? "false" : "true");
    if (!result.id.isEmpty()) {
        json += ",\"pattern\":\"" + result.pattern + "\",\"actionType\":\"" + result.actionType + "\"";
    }
    json += "}";
    sendJson(json);
}

// Study Manager

void WebPortal::handleApiStudySubjects() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET) {
        auto subjects = studyManager.getAllSubjects();
        String json; json.reserve(2048);
        json += "{\"subjects\":[";
        for (size_t i = 0; i < subjects.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"id\":\"" + subjects[i].id + "\",\"name\":\"" + subjects[i].name + "\"";
            json += ",\"mastery\":" + String(subjects[i].masteryLevel);
            json += ",\"minutes\":" + String(subjects[i].totalMinutes) + "}";
        }
        json += "]}";
        sendJson(json);
    } else if (m_server.method() == HTTP_POST) {
        String name = m_server.arg("name");
        if (name.isEmpty()) { sendError("Missing subject name"); return; }
        StudySubject subject;
        subject.name = name;
        subject.description = m_server.arg("description");
        subject.tags = m_server.arg("tags");
        String id = studyManager.addSubject(subject);
        sendSuccess(id.isEmpty() ? "Failed to create subject" : ("Subject created: " + id));
    }
}

void WebPortal::handleApiStudySessions() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET) {
        String subjectId = m_server.arg("subjectId");
        auto sessions = studyManager.getSessions(subjectId);
        String json; json.reserve(4096);
        json += "{\"sessions\":[";
        for (size_t i = 0; i < sessions.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"id\":\"" + sessions[i].id + "\",\"subjectId\":\"" + sessions[i].subjectId + "\"";
            json += ",\"duration\":" + String(sessions[i].durationMinutes);
            json += ",\"performance\":" + String(sessions[i].performance) + "}";
        }
        json += "]}";
        sendJson(json);
    } else if (m_server.method() == HTTP_POST) {
        String subjectId = m_server.arg("subjectId");
        if (subjectId.isEmpty()) { sendError("Missing subject ID"); return; }
        String id = studyManager.startSession(subjectId);
        if (id.isEmpty()) { sendError("Failed to start session"); return; }
        unsigned long duration = static_cast<unsigned long>(m_server.arg("duration").toInt());
        uint8_t performance = static_cast<uint8_t>(m_server.arg("performance").toInt());
        String notes = m_server.arg("notes");
        String topics = m_server.arg("topics");
        studyManager.endSession(id, performance, notes, topics);
        sendSuccess("Session recorded: " + id);
    }
}

void WebPortal::handleApiStudyFlashcards() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET) {
        String subjectId = m_server.arg("subjectId");
        auto cards = studyManager.getAllFlashCards(subjectId);
        String json; json.reserve(4096);
        json += "{\"flashcards\":[";
        for (size_t i = 0; i < cards.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"id\":\"" + cards[i].id + "\",\"subjectId\":\"" + cards[i].subjectId + "\"";
            json += ",\"question\":\"" + cards[i].question + "\",\"answer\":\"" + cards[i].answer + "\"}";
        }
        json += "]}";
        sendJson(json);
    } else if (m_server.method() == HTTP_POST) {
        String subjectId = m_server.arg("subjectId");
        String question = m_server.arg("question");
        String answer = m_server.arg("answer");
        if (subjectId.isEmpty() || question.isEmpty() || answer.isEmpty()) {
            sendError("Missing subjectId, question, or answer"); return;
        }
        String id = studyManager.addFlashCard(subjectId, question, answer);
        sendSuccess(id.isEmpty() ? "Failed to add flashcard" : ("Flashcard added: " + id));
    }
}

void WebPortal::handleApiStudyStats() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String json;
    json.reserve(512);
    json += "{";
    json += "\"totalSubjects\":" + String(studyManager.isInitialized() ? studyManager.getAllSubjects().size() : 0) + ",";
    json += "\"totalMinutes\":" + String(studyManager.totalStudyMinutes()) + ",";
    json += "\"dueSubjects\":" + String(studyManager.isInitialized() ? studyManager.getDueSubjects().size() : 0);
    json += "}";
    sendJson(json);
}

// Companion

void WebPortal::handleApiCompanionDevices() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_GET) {
        auto devices = companionManager.getAllDevices();
        String json; json.reserve(2048);
        json += "{\"devices\":[";
        for (size_t i = 0; i < devices.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"id\":\"" + devices[i].id + "\",\"name\":\"" + devices[i].name + "\"";
            json += ",\"type\":\"" + devices[i].deviceType + "\",\"status\":\"" + devices[i].status + "\"}";
        }
        json += "]}";
        sendJson(json);
    } else if (m_server.method() == HTTP_POST) {
        String name = m_server.arg("name");
        String type = m_server.arg("type");
        String ip = m_server.arg("ip");
        if (name.isEmpty() || type.isEmpty()) { sendError("Missing name or type"); return; }
        uint16_t port = static_cast<uint16_t>(m_server.arg("port").toInt());
        if (port == 0) port = 8080;
        sendSuccess(companionManager.pairDevice(name, type, ip, port) ? "Device paired" : "Failed to pair device");
    }
}

void WebPortal::handleApiCompanionMessages() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    String deviceId = m_server.arg("deviceId");
    auto messages = companionManager.getMessageHistory(deviceId);
    String json; json.reserve(4096);
    json += "{\"messages\":[";
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + messages[i].id + "\",\"type\":\"" + messages[i].type + "\"";
        json += ",\"payload\":\"" + messages[i].payload + "\",\"timestamp\":" + String(messages[i].timestamp);
        json += ",\"delivered\":" + String(messages[i].delivered ? "true" : "false") + "}";
    }
    json += "]}";
    sendJson(json);
}

// Plugin Marketplace

void WebPortal::handleApiPluginsMarketplace() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json;
    json.reserve(2048);
    json += "{\"plugins\":[";
    const auto& plugins = pluginManager.getAllPlugins();
    for (size_t i = 0; i < plugins.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + plugins[i].id + "\",\"name\":\"" + plugins[i].name + "\",\"version\":\"" + plugins[i].version + "\"}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiPluginsMarketplaceRegister() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String id = m_server.arg("id");
    String name = m_server.arg("name");
    String version = m_server.arg("version");
    String author = m_server.arg("author");
    String description = m_server.arg("description");
    if (id.isEmpty() || name.isEmpty()) { sendError("Missing plugin id or name"); return; }
    PluginMetadata plugin;
    plugin.id = id;
    plugin.name = name;
    plugin.version = version;
    plugin.author = author;
    plugin.description = description;
    sendSuccess(pluginManager.registerPlugin(plugin) ? "Plugin registered" : "Failed to register plugin");
}

// ============================================================================
// V3.1 - Wake Word API Handlers
// ============================================================================

void WebPortal::handleApiWakeWordStatus() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(conversationManager.getWakeWordStatsJson());
}

void WebPortal::handleApiWakeWordSettings() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;

    if (m_server.method() == HTTP_POST) {
        if (m_server.hasArg("enabled")) {
            conversationManager.setWakeWordEnabled(m_server.arg("enabled") == "true");
        }
        if (m_server.hasArg("sensitivity")) {
            float sens = m_server.arg("sensitivity").toFloat();
            conversationManager.setWakeWordSensitivity(sens);
        }
        if (m_server.hasArg("cooldown")) {
            unsigned long cd = static_cast<unsigned long>(m_server.arg("cooldown").toInt());
            conversationManager.setWakeWordCooldown(cd);
        }
        if (m_server.hasArg("noiseThreshold")) {
            uint16_t nt = static_cast<uint16_t>(m_server.arg("noiseThreshold").toInt());
            audioManager.setNoiseThreshold(nt);
        }
        sendSuccess("Wake word settings updated");
        return;
    }

    String json = "{";
    json += "\"enabled\":" + String(conversationManager.isWakeWordEnabled() ? "true" : "false");
    json += ",\"sensitivity\":" + String(conversationManager.getWakeWordSensitivity(), 2);
    json += ",\"cooldownMs\":" + String(conversationManager.getWakeWordCooldown());
    json += ",\"noiseThreshold\":" + String(audioManager.getNoiseThreshold());
    json += ",\"noiseFloor\":" + String(audioManager.getNoiseFloor());
    json += "}";
    sendJson(json);
}

void WebPortal::handleApiWakeWordPhrases() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json = "{\"phrases\":[";
    const auto& phrases = conversationManager.getWakeWordPhrases();
    for (size_t i = 0; i < phrases.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"index\":" + String(i) + ",\"phrase\":\"" + phrases[i] + "\"}";
    }
    json += "]}";
    sendJson(json);
}

void WebPortal::handleApiWakeWordAddPhrase() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String phrase = m_server.arg("phrase");
    if (phrase.isEmpty()) { sendError("Missing phrase"); return; }
    sendSuccess(conversationManager.addWakeWordPhrase(phrase) ? "Phrase added" : "Failed to add phrase");
}

void WebPortal::handleApiWakeWordRemovePhrase() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    size_t index = static_cast<size_t>(m_server.arg("index").toInt());
    conversationManager.removeWakeWordPhrase(index);
    sendSuccess("Phrase removed");
}

void WebPortal::handleApiWakeWordResetStats() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    conversationManager.resetWakeWordStats();
    sendSuccess("Wake word stats reset");
}

void WebPortal::handleApiWakeWordCalibrate() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    if (audioManager.calibrateNoiseFloor()) {
        sendSuccess("Noise floor calibrated");
    } else {
        sendError("Calibration failed (is mic recording?)");
    }
}

// ============================================================================
// V3.1 - ESP-NOW API Handlers
// ============================================================================

void WebPortal::handleApiEspNowStatus() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String json = "{";
    json += "\"initialized\":" + String(espNowManager.isInitialized() ? "true" : "false");
    json += ",\"discovering\":" + String(espNowManager.isDiscovering() ? "true" : "false");
    json += ",\"nodeCount\":" + String(espNowManager.nodeCount());
    json += "}";
    sendJson(json);
}

void WebPortal::handleApiEspNowNodes() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    sendJson(espNowManager.getNodesJson());
}

void WebPortal::handleApiEspNowPair() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String macStr = m_server.arg("mac");
    if (macStr.isEmpty()) { sendError("Missing MAC address"); return; }
    uint8_t mac[6];
    if (sscanf(macStr.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        sendError("Invalid MAC format"); return;
    }
    int type = m_server.arg("type").toInt();
    EspNowNodeType nodeType = (type >= 0 && type <= 4) ? static_cast<EspNowNodeType>(type) : EspNowNodeType::UNKNOWN;
    sendSuccess(espNowManager.pairNode(mac, nodeType) ? "Node paired" : "Failed to pair node");
}

void WebPortal::handleApiEspNowUnpair() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String macStr = m_server.arg("mac");
    if (macStr.isEmpty()) { sendError("Missing MAC address"); return; }
    uint8_t mac[6];
    if (sscanf(macStr.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        sendError("Invalid MAC format"); return;
    }
    espNowManager.unpairNode(mac);
    sendSuccess("Node unpaired");
}

void WebPortal::handleApiEspNowDiscovery() noexcept
{
    m_requestCounter++;
    if (!isAuthenticatedOrReject()) return;
    String action = m_server.arg("action");
    if (action == "start") {
        sendSuccess(espNowManager.startDiscovery() ? "Discovery started" : "Failed to start discovery");
    } else if (action == "stop") {
        espNowManager.stopDiscovery();
        sendSuccess("Discovery stopped");
    } else {
        sendError("Invalid action (use 'start' or 'stop')");
    }
}

// ============================================================================
// Private Task Functions
// ============================================================================

/**
 * @brief FreeRTOS task for device restart.
 *
 * Scheduled restart with delay to allow HTTP response to complete.
 */
static void restartTask(void* pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
    ESP.restart();
    vTaskDelete(nullptr);
}

/**
 * @brief FreeRTOS task for factory reset.
 *
 * Clears Wi-Fi credentials and restarts device.
 */
static void factoryResetTask(void* pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(FACTORY_RESET_DELAY_MS));
    wifiManager.clearCredentials();
    Logger::info("WebPortal", "Wi-Fi credentials cleared");

    // Clear web-portal admin credentials so the next boot generates a fresh
    // random admin password (printed to Serial). Never reuse old credentials.
    {
        Preferences authPrefs;
        authPrefs.begin("auraauth", false);
        authPrefs.clear();
        authPrefs.end();
        Logger::info("WebPortal", "Admin credentials cleared");
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
    vTaskDelete(nullptr);
}

/**
 * @brief FreeRTOS task for OTA restart.
 *
 * Scheduled restart after OTA firmware update completion.
 */
static void otaRestartTask(void* pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(OTA_RESTART_DELAY_MS));
    ESP.restart();
    vTaskDelete(nullptr);
}
