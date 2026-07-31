#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "secrets.h"
#include "version.h"

//======================================================
// AURA AI Desktop Assistant
// Hardware Configuration
// Target : ESP32-WROOM-32 (38 Pin)
// Version: see version.h (single source of truth)
//======================================================

//======================================================
// PROJECT INFORMATION
//======================================================

#define AURA_NAME              "AURA"
#define AURA_VERSION           aura::version::kSemVer

namespace aura {
namespace identity {

constexpr const char* kProjectName  = "AURA AI Desktop Assistant";
constexpr const char* kVersion      = aura::version::kSemVer;
constexpr const char* kCodename     = aura::version::kCodename;
constexpr const char* kBuildType    = aura::version::kChannel;
constexpr const char* kHardwareRev  = "MK-II";
constexpr const char* kAuthor       = "Devil";
constexpr const char* kPlatform     = "ESP32-WROOM-32";
constexpr const char* kBuildDate    = aura::version::kBuildDate;
constexpr const char* kBuildTime    = aura::version::kBuildTime;
constexpr const char* kCompiler     = __VERSION__;

} // namespace identity
} // namespace aura

//======================================================
// OLED DISPLAY (SSD1306)
//======================================================

#define OLED_SDA_PIN           21
#define OLED_SCL_PIN           22

#define OLED_WIDTH             128
#define OLED_HEIGHT            64
#define OLED_ADDRESS           0x3C
#define OLED_RESET             -1

//======================================================
// INMP441 MICROPHONE (I2S)
//======================================================

#define MIC_BCLK_PIN           26
#define MIC_WS_PIN             25
#define MIC_DATA_PIN           34

#define AUDIO_SAMPLE_RATE      16000
#define AUDIO_BITS             16

//======================================================
// MAX98357 SPEAKER (I2S)
//======================================================

#define SPK_BCLK_PIN           27
#define SPK_LRC_PIN            14
#define SPK_DATA_PIN           12

//======================================================
// WS2812 LED RING
//======================================================

#define LED_RING_PIN           4
#define LED_COUNT              16
#define LED_BRIGHTNESS         80

//======================================================
// TOUCH SENSOR
//======================================================

#define TOUCH_PIN              13
#define TOUCH_DEBOUNCE         50

//======================================================
// MICRO SD (SPI)
//======================================================

#define SD_CS_PIN              5
#define SD_MOSI_PIN            23
#define SD_MISO_PIN            19
#define SD_SCK_PIN             18

//======================================================
// WIFI SETUP PORTAL
//======================================================

// AP credentials defined in secrets.h (namespace Secrets::AP_SSID / AP_PASSWORD)
// Use Secrets::AP_SSID and Secrets::AP_PASSWORD directly.

#define WIFI_TIMEOUT           30000

//======================================================
// WEB SERVER
//======================================================

#define WEB_PORT               80

//======================================================
// GOOGLE AI
//======================================================

#define GEMINI_MODEL               "gemini-2.5-flash"

#define GEMINI_URL \
"https://generativelanguage.googleapis.com/v1beta/models/" GEMINI_MODEL ":generateContent"

#define GEMINI_TEMPERATURE          0.7
#define GEMINI_TOP_P                0.95
#define GEMINI_TOP_K                40
#define GEMINI_MAX_TOKENS           2048
#define GEMINI_TIMEOUT_MS           60000UL
#define GEMINI_CONNECT_TIMEOUT_MS   10000UL
#define GEMINI_RETRY_MAX            3
#define GEMINI_RETRY_BASE_DELAY_MS  1000UL
#define GEMINI_RETRY_MAX_DELAY_MS   30000UL
#define GEMINI_CONVERSATION_MAX_TOKENS 4096
#define GEMINI_CACHE_SIZE           32
#define GEMINI_BUFFER_SIZE          4096

#define GOOGLE_STT_URL \
"https://speech.googleapis.com/v1/speech:recognize"

#define GOOGLE_TTS_URL \
"https://texttospeech.googleapis.com/v1/text:synthesize"

//======================================================
// AUDIO FOLDER
//======================================================

#define CONFIG_FILE            "/config.json"
#define WIFI_FILE              "/wifi.json"
#define REMINDER_FILE          "/reminders.json"
#define HISTORY_FILE           "/history.json"

#define AUDIO_FOLDER           "/audio"
#define AUDIO_MANIFEST_PATH    "/audio/manifest.json"
#define AUDIO_THEMES_PATH      "/audio/themes"
#define AUDIO_CACHE_SIZE       8
#define AUDIO_CACHE_PATH       "/cache/audio"
#define AUDIO_MAX_ASSET_SIZE   262144
#define CACHE_FOLDER           "/cache"
#define LOG_FOLDER             "/logs"

//======================================================
// LED COLORS
//======================================================

#define LED_BOOT               0xFFFFFF
#define LED_READY              0x00FF00
#define LED_LISTENING          0x0000FF
#define LED_PROCESSING         0xFFFF00
#define LED_SPEAKING           0x8000FF
#define LED_MUTED              0xFF0000
#define LED_ERROR              0xFF3300

//======================================================
// LOG LEVEL
//======================================================

enum LogLevel
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
};

#define CURRENT_LOG_LEVEL LOG_INFO

//======================================================
// PLUGIN SYSTEM
//======================================================

#define PLUGINS_PATH             "/plugins"
#define PLUGIN_METADATA_FILE     "plugin.json"
#define PLUGIN_MAX_COUNT         32
#define PLUGIN_DESC_MAX_LEN      256

//======================================================
// SKILL ENGINE
//======================================================

#define SKILLS_PATH              "/skills"
#define SKILL_ACTIONS_MAX        8
#define SKILL_CONDITIONS_MAX     4
#define SKILL_MAX_COUNT          64

//======================================================
// PERSONALITY PROFILES
//======================================================

#define PERSONALITIES_PATH       "/personalities"
#define PERSONALITY_PROMPT_MAX   2048
#define PERSONALITY_MAX_COUNT    16

//======================================================
// MEMORY EXTENSIONS
//======================================================

#define CONVERSATIONS_PATH       "/conversations"
#define MAX_CONVERSATION_HISTORY 50
#define MEMORY_RANK_TOP_N        10
#define MEMORY_CLEANUP_INTERVAL_MS 3600000UL

//======================================================
// BRIEFING / DAILY SUMMARY
//======================================================

#define SUMMARY_MAX_LENGTH       2048

//======================================================
// PERFORMANCE MONITORING
//======================================================

#define PERF_SAMPLE_INTERVAL_MS  5000UL
#define PERF_HISTORY_SIZE        60

//======================================================
// CRASH REPORTER
//======================================================

#define CRASH_LOG_PATH           "/crashes"
#define CRASH_LOG_MAX            10
#define BOOT_LOOP_THRESHOLD      3
#define CRASH_COUNTER_NVS_NAMESPACE "auracrash"
#define CRASH_COUNTER_KEY        "crash_cnt"

//======================================================
// DIAGNOSTICS
//======================================================

#define DIAG_MAX_COMPONENTS      16

//======================================================
// SAFE MODE
//======================================================

#define SAFE_MODE_TOUCH_HOLD_MS  3000UL
#define SAFE_MODE_TOUCH_THRESHOLD 40

//======================================================
// KNOWLEDGE GRAPH
//======================================================

#define KNOWLEDGE_GRAPH_PATH     "/knowledge_graph.json"
#define KG_MAX_NODES             256
#define KG_MAX_EDGES             512
#define KG_AUTO_LINK_THRESHOLD   0.3f

//======================================================
// GOAL MANAGER
//======================================================

#define GOALS_PATH               "/goals.json"
#define GOAL_MAX_COUNT           64
#define GOAL_MAX_MILESTONES      16

//======================================================
// HABIT MANAGER
//======================================================

#define HABITS_PATH              "/habits.json"
#define HABIT_MAX_COUNT          32
#define HABIT_HISTORY_DAYS       90

//======================================================
// PLANNER
//======================================================

#define PLANNER_PATH             "/plans.json"
#define PLANNER_MAX_TASKS        128

//======================================================
// REFLECTION
//======================================================

#define REFLECTION_PATH          "/reflections.json"
#define REFLECTION_MAX_HISTORY   30

//======================================================
// AUTOMATION
//======================================================

#define AUTOMATIONS_PATH         "/automations.json"
#define AUTO_MAX_SCRIPTS         32
#define AUTO_CONDITIONS_MAX      8
#define AUTO_ACTIONS_MAX         8

//======================================================
// TINY AI (Offline Engine)
//======================================================

#define TINYAI_ENABLED_DEFAULT   true
#define TINYAI_MAX_RESPONSE_LEN  512

//======================================================
// STARTUP GREETING
//======================================================

#define STARTUP_GREETING_DISPLAY_MS  4000UL
#define STARTUP_GREETING_NVS_NAMESPACE "aurastartup"
#define STARTUP_GREETING_LINES_MAX   9

//======================================================
// TIMELINE MANAGER
//======================================================

#define TIMELINE_PATH               "/timeline"
#define TIMELINE_MAX_ENTRIES        512
#define TIMELINE_BATCH_SIZE         50
#define TIMELINE_FILE_VERSION       1

//======================================================
// BRIEFING MANAGER
//======================================================

#define BRIEFING_PATH               "/briefings"
#define BRIEFING_MAX_LENGTH         2048
#define BRIEFING_FILE_VERSION       1

//======================================================
// SEMANTIC SEARCH
//======================================================

#define SEMANTIC_MAX_RESULTS        20
#define SEMANTIC_CACHE_SIZE         32
#define SEMANTIC_MIN_CONFIDENCE     0.25f

//======================================================
// RELATIONSHIP ENGINE
//======================================================

#define RELATIONSHIP_PATH           "/knowledge_graph.json"
#define REL_STRENGTH_DEFAULT        1.0f
#define REL_AUTO_LINK_MIN_STRENGTH  0.3f
#define REL_HISTORY_MAX             50

//======================================================
// DECISION MANAGER
//======================================================

#define DECISIONS_PATH              "/decisions"
#define DECISION_MAX_OPTIONS        8
#define DECISION_MAX_HISTORY        50
#define DECISION_CONFIDENCE_MIN     0.0f
#define DECISION_CONFIDENCE_MAX     1.0f

//======================================================
// LEARNING MANAGER
//======================================================

#define LEARNING_PATH               "/learning"
#define LEARNING_MAX_PATTERNS       64
#define LEARNING_MAX_OBSERVATIONS   256
#define LEARNING_MIN_CONFIDENCE     0.3f
#define LEARNING_OBSERVE_INTERVAL_MS 300000UL

//======================================================
// RECOMMENDATIONS (moved to ExecutiveAssistant)
//======================================================

#define RECOMMENDATIONS_PATH        "/recommendations"

//======================================================
// PREDICTION MANAGER
//======================================================

#define PREDICTIONS_PATH            "/predictions"
#define PREDICTION_MAX_HISTORY      50
#define PREDICTION_DEFAULT_CONFIDENCE 0.5f
#define PREDICTION_INTERVAL_MS      600000UL

//======================================================
// DOCUMENT MANAGER
//======================================================

#define DOCUMENTS_PATH              "/documents"
#define DOCUMENT_MAX_COUNT          64
#define DOCUMENT_MAX_SIZE           1048576UL
#define DOCUMENT_SUPPORTED_EXTS     ".txt,.md,.json,.csv"

//======================================================
// WORKSPACE MANAGER
//======================================================

#define WORKSPACES_PATH             "/workspaces"
#define WORKSPACE_MAX_COUNT         16
#define WORKSPACE_MAX_MEMBERS       64

//======================================================
// VAULT MANAGER
//======================================================

#define VAULT_PATH                  "/vault"
#define VAULT_MAX_ENTRIES           32
#define VAULT_ENCRYPTION_KEY_SIZE   32
#define VAULT_BACKUP_PATH           "/vault_backup"

//======================================================
// DEVELOPER (PerformanceManager Extension)
//======================================================

#define DEV_API_LATENCY_HISTORY     20
#define DEV_MAX_TASK_NAME_LEN       16

//======================================================
// V3.0 - EVENT BUS
//======================================================

#define EVENT_BUS_MAX_PENDING      64
#define EVENT_BUS_MAX_HANDLERS     32
#define EVENT_BUS_PUBLISH_INTERVAL_MS 50

//======================================================
// V3.0 - SKILL STUDIO
//======================================================

#define SKILL_STUDIO_MAX_SKILLS    100
#define SKILL_EXECUTION_LOG_SIZE   200

//======================================================
// V3.0 - STUDY MANAGER
//======================================================

#define STUDY_SUBJECTS_MAX         32
#define STUDY_SESSIONS_MAX         512
#define STUDY_FLASHCARDS_MAX       256

//======================================================
// V3.0 - WORKSPACE PROJECT INTELLIGENCE
//======================================================

#define WORKSPACE_PROJECT_STATUS_ACTIVE     "active"
#define WORKSPACE_PROJECT_STATUS_PAUSED     "paused"
#define WORKSPACE_PROJECT_STATUS_COMPLETED  "completed"
#define WORKSPACE_PROJECT_STATUS_ARCHIVED   "archived"

//======================================================
// V3.0 - COMPANION MANAGER
//======================================================

#define COMPANION_MAX_DEVICES       8
#define COMPANION_MAX_MESSAGES      256
#define COMPANION_MAX_RETRIES       3
#define COMPANION_RETRY_INTERVAL_MS 30000

//======================================================
// V3.0 - NL AUTOMATION
//======================================================

#define NL_PATTERNS_MAX             50

//======================================================
// V3.0 - PLUGIN MARKETPLACE
//======================================================

#define PLUGIN_MARKETPLACE_MAX      32
#define PLUGIN_MARKETPLACE_PATH     "/plugin_marketplace.json"

//======================================================
// V3.0 - WAKE WORD DETECTION
//======================================================

#define WAKE_WORD_PHRASES_MAX       8
#define WAKE_WORD_PHRASE_MAX_LEN    32
#define WAKE_WORD_SENSITIVITY_MIN   0.0f
#define WAKE_WORD_SENSITIVITY_MAX   1.0f
#define WAKE_WORD_SENSITIVITY_DEFAULT 0.5f
#define WAKE_WORD_COOLDOWN_MS       3000UL
#define WAKE_WORD_COOLDOWN_DEFAULT  2000UL
#define WAKE_WORD_NOISE_THRESHOLD_DEFAULT 300
#define WAKE_WORD_STATS_WINDOW      100
#define WAKE_WORD_DETECT_BUF_SIZE   8192

//======================================================
// V3.0 - ESP-NOW NETWORK
//======================================================

#define ESPNOW_CHANNEL              1
#define ESPNOW_MAX_PEERS            20
#define ESPNOW_SEND_INTERVAL_MS     100
#define ESPNOW_HEARTBEAT_INTERVAL_MS 10000UL
#define ESPNOW_PAIR_TIMEOUT_MS      60000UL
#define ESPNOW_ENCRYPT_KEY_SIZE     16
#define ESPNOW_BROADCAST_FLAG       0xFF
#define ESPNOW_MAX_NODES            10
#define ESPNOW_OTA_CHUNK_SIZE       1024

//======================================================
// V3.0 - WEBSOCKET DASHBOARD
//======================================================

#define WS_PORT                     81
#define WS_MAX_CLIENTS              4
#define WS_PUBLISH_INTERVAL_MS      500UL
#define WS_PING_INTERVAL_MS         30000UL
#define WS_PAYLOAD_MAX              2048

#endif

