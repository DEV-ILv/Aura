#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "secrets.h"
#include "version.h"

// ===========================================================================
// BUILD MODE - development vs production credentials
// ===========================================================================
// AURA_DEVELOPMENT_MODE selects how credentials are provisioned:
//
//   0 (default / production):
//       - a strong random admin password is generated on first boot, stored in
//         NVS, and printed to the Serial monitor (never hardcoded).
//       - the setup AP password is derived from the device MAC when unset.
//
//   1 (development - LOCAL TESTING ONLY):
//       - well-known development credentials are used for convenience:
//             Web Portal login : Devil / Devil
//             Setup AP (AURA_Setup) : DevilDevil
//       - NEVER ship or publish a development build.
//
// The git-ignored secrets.h (included above) may override this default, so a
// local developer build can enable the mode without touching committed files.
// ===========================================================================
#ifndef AURA_DEVELOPMENT_MODE
#define AURA_DEVELOPMENT_MODE 0
#endif

#if AURA_DEVELOPMENT_MODE
// Well-known DEVELOPMENT credentials - compiled in ONLY for dev builds.
#define AURA_DEV_WEB_USERNAME "Devil"
#define AURA_DEV_WEB_PASSWORD "Devil"
#define AURA_DEV_AP_PASSWORD  "DevilDevil"
#endif

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
// OLED DISPLAY (SH1106)
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
//
// Interaction model (exactly three gestures, final interaction spec):
//   SINGLE TAP   -> microphone ON, listening
//   DOUBLE TAP   -> microphone OFF, cancel voice interaction, IDLE
//   5s HOLD      -> AURA SETUP mode (highest gesture priority)
//
// Priority: 5s hold > double tap > single tap. A hold must never
// accidentally fire a tap, and vice-versa. All timings are non-blocking
// (millis()-based); see ConversationManager::processTouch().
//
// Recommended reference values:
//   Debounce        50-100 ms
//   Double-tap      350-500 ms
//   Setup hold      5000 ms

#define TOUCH_PIN              13

// Digital debounce across the raw touch line before state is considered.
#define TOUCH_DEBOUNCE_MS      80

// Shortest press considered a real tap (shorter = electrical bounce).
#define TAP_MIN_MS             50UL

// Longest press still treated as a tap (longer = hold, no tap action).
#define TAP_MAX_MS             400UL

// Max delay between the two releases of a double-tap.
#define DOUBLE_TAP_WINDOW_MS   400UL

// Continuous-hold duration that enters AURA SETUP mode.
#define SETUP_HOLD_MS          5000UL

// Touch polling cadence (resulted in the main conversation loop).
#define TOUCH_POLL_INTERVAL_MS 20UL

// Touch diagnostics (DEVELOPMENT ONLY): rate-limited touch telemetry lines
// (raw/debounced state, transition count, press/release timestamps, gestures).
// Set to 0 for production builds — when 0 the diagnostics compile out entirely,
// so there is zero runtime cost and zero Serial output from this feature.
#define TOUCH_DIAGNOSTICS_ENABLED 1
#define TOUCH_DIAG_INTERVAL_MS    5000UL

// Minimum gap after a *finalized* touch gesture before a new press is accepted.
// Suppresses rapid re-triggering from a hand hovering/jittering at the edge of
// the sensing range. Deliberately SHORTER than DOUBLE_TAP_WINDOW_MS and a
// pending double-tap second tap is always allowed through, so the double-tap
// timing is never altered.
#define TOUCH_GESTURE_GAP_MS      250UL

//======================================================
// HARDWARE PROFILE - current prototype (MK-II)
//======================================================
// Declares which optional output peripherals are physically connected.
// 1 = module present on this unit (initialized normally).
// 0 = module absent  (marked OFFLINE, initialization skipped, boot continues).
// Input peripherals (Display, Microphone, SD, Touch) are detected/handled
// independently and are not listed here.

#define AURA_HW_LED_RING_PRESENT  1   // WS2812 LED ring   : connected (GPIO4, 16 LEDs)
#define AURA_HW_SPEAKER_PRESENT   0   // MAX98357 amp+spkr : NOT connected

//======================================================
// MICRO SD (SPI)
//======================================================

#define SD_CS_PIN              5
#define SD_MOSI_PIN            23
#define SD_MISO_PIN            19
#define SD_SCK_PIN             18

// SD SPI clock. 4 MHz is well within the SD SPI spec, tolerant of long
// dupont-wire runs and marginal cards, and materially improves reliability
// over the 25 MHz default. Can be raised if the card/installation is solid.
#define SD_SPI_FREQUENCY_HZ    4000000UL

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
// HEADLESS DEVELOPMENT MODE
//======================================================
// HEADLESS_MODE_AUTO   : enable headless automatically when no display is
//                        detected during boot (missing OLED -> headless).
// HEADLESS_MODE_FORCE  : force headless even when all hardware is present.
//                        Useful for bench testing with only ESP32 + USB.
// When headless, optional hardware modules (Display, LED Ring, Microphone,
// Speaker, Touch, sensors, SD) are disabled with a warning; boot NEVER aborts
// for a missing peripheral. All headless-capable features (Wi-Fi, Web Portal,
// REST, WebSocket, Local AI, Gemini, Memory, Planner, etc.) remain active.

#define HEADLESS_MODE_AUTO          true
#define HEADLESS_MODE_FORCE         false

//======================================================
// GOOGLE AI
//======================================================

#define GEMINI_MODEL               "gemini-3.5-flash-lite"

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

//======================================================
// VOICE PROVIDER SELECTION - Speech-to-Text / Text-to-Speech
//======================================================
// AURA supports interchangeable speech providers. The former Google Cloud
// STT/TTS implementation has been removed; Sarvam AI (sarvam_stt.cpp /
// sarvam_tts.cpp) is now the ACTIVE provider but is not implemented yet, so it
// degrades gracefully until the API integration lands. Selecting a provider
// that is not yet implemented resolves to a "Not Implemented" module.
//======================================================

enum class SpeechProvider : uint8_t {
    SARVAM,     // Sarvam AI Speech-to-Text (active placeholder, not implemented)
    DEEPGRAM,   // Deepgram (planned, not implemented)
    LOCAL       // Local/on-device recognition (planned, not implemented)
};

enum class TTSProvider : uint8_t {
    SARVAM,     // Sarvam AI Text-to-Speech (active placeholder, not implemented)
    ELEVENLABS, // ElevenLabs (planned, not implemented)
    PIPER       // Piper local TTS (planned, not implemented)
};

#define DEFAULT_SPEECH_PROVIDER SpeechProvider::SARVAM
#define DEFAULT_TTS_PROVIDER    TTSProvider::SARVAM

// Sarvam AI base URL - the SarvamClient connects here over HTTPS (TLS 1.2).
#define SARVAM_BASE_URL \
"https://api.sarvam.ai"

// Sarvam AI API request paths (documented REST endpoints).
#define SARVAM_STT_PATH          "/v1/speech-to-text/transcribe"
#define SARVAM_TTS_PATH          "/v1/text-to-speech"

// Sarvam AI STT/TTS models and defaults.
#define SARVAM_STT_MODEL         "saarika:v2"
#define SARVAM_TTS_MODEL         "bulbul:v1"

// Language code sent to the Sarvam API (override at runtime via setLanguage).
// Sarvam supports Indian languages; "en-IN" (Indian English) and "hi-IN"
// (Hindi) map to the spoken language AURA transcribes and speaks by default.
#define SARVAM_LANGUAGE          "en-IN"

// Default TTS speaker (Sarvam provides a set of named voices: meera, pavithra,
// bhoomi, arjun, ...). Overridable at runtime via setVoice().
#define SARVAM_VOICE             "meera"

// Sample rate for transcription uploads (PCM captured at 16 kHz mono).
#define SARVAM_SAMPLE_RATE       16000U
// Sample rate requested from the TTS API for synthesized audio.
#define SARVAM_TTS_SAMPLE_RATE   16000U
#define SARVAM_CHANNELS          1
#define SARVAM_BITS_PER_SAMPLE   16

// Request timeouts and retry budget for Sarvam AI calls.
#define SARVAM_TIMEOUT_MS        15000UL
#define SARVAM_RETRY_MAX         3
#define SARVAM_RETRY_BASE_DELAY_MS 500UL
#define SARVAM_RETRY_MAX_DELAY_MS  8000UL

// Capture limits (heap budget on a 4 MB ESP32).
#define SARVAM_MAX_AUDIO_BYTES   176000UL   // ~5.5 s of 16 kHz 16-bit PCM
#define SARVAM_MAX_TTS_BASE64    245760UL   // cap holding raw base64 before decode
#define SARVAM_TTS_STREAM_CHUNK  512UL      // bytes decoded+played per loop tick

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
// ERROR REPORTER (structured diagnostic events)
//======================================================

#define ERROR_LOG_MAX            200
#define ERROR_EVENT_PATH         "/aura_errors.json"
#define ERROR_REPORT_THROTTLE_MS 30000UL
#define ERROR_WS_PUSH_SEVERITY   2   // push WS on new events >= this level (2=ERROR, 3=CRITICAL)

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
// LOCAL AI ENGINE V2 (Offline Intelligence Upgrade)
//======================================================

#define LOCAL_AI_HISTORY_TURNS        6      // Turns of conversation context retained
#define LOCAL_AI_CACHE_SIZE           8      // Response cache slots (questions+answers)
#define LOCAL_AI_RETRIEVAL_MEMORIES   3      // Top-N memories injected per turn
#define LOCAL_AI_RETRIEVAL_GRAPH      2      // Top-N knowledge-graph nodes injected
#define LOCAL_AI_MAX_FOLLOWUP_LEN     96     // Max length of generated follow-up question
#define LOCAL_AI_MAX_DATA_ITEMS       5      // Max list items quoted in a response
#define LOCAL_AI_SELF_TEST_ON_BOOT    true   // Run engine self-test during initialization
#define LOCAL_AI_VARIATION_POOL       4      // Minimum variants per phrase pool

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

