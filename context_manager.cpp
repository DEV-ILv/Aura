#include "context_manager.h"
#include "memory_manager.h"
#include "reminder_manager.h"
#include "personality_manager.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "json_helpers.h"
#include "event_bus.h"

ContextManager contextManager;

ContextManager::ContextManager() noexcept
    : m_initialized(false), m_lastRefresh(0)
    , m_assistantContext(AssistantContext::GENERAL), m_contextSwitchTime(0)
    , m_contextSwitchCount(0), m_lastContextSwitchDate(0) {
    m_history.reserve(kMaxHistory);
}

ContextManager::~ContextManager() noexcept {}

bool ContextManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }
    refresh();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized");
    return true;
}

void ContextManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    if (now - m_lastRefresh >= kRefreshIntervalMs) {
        m_lastRefresh = now;
        refresh();
    }
}

String ContextManager::getContextJson() const noexcept {
    String json;
    json.reserve(1024);
    json += "{";
    json += "\"timestamp\":" + String(m_context.timestamp) + ",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"wifi_connected\":" + String(m_context.wifiConnected ? "true" : "false") + ",";
    json += "\"wifi_rssi\":" + String(m_context.wifiRSSI) + ",";
    json += "\"ip\":\"" + escapeJson(m_context.localIP) + "\",";
    json += "\"free_heap\":" + String(m_context.freeHeap) + ",";
    json += "\"memories\":" + String(m_context.memoryCount) + ",";
    json += "\"reminders\":" + String(m_context.reminderCount) + ",";
    json += "\"conversations\":" + String(m_context.conversationCount) + ",";
    json += "\"system_state\":\"" + escapeJson(m_context.systemState) + "\",";
    json += "\"personality\":\"" + escapeJson(m_context.activePersonality) + "\",";
    json += "\"sd_mounted\":" + String(m_context.sdMounted ? "true" : "false") + ",";
    json += "\"activity\":\"" + escapeJson(m_context.currentActivity) + "\",";
    json += "\"project\":\"" + escapeJson(m_context.currentProject) + "\",";
    json += "\"subject\":\"" + escapeJson(m_context.currentSubject) + "\",";
    json += "\"location\":\"" + escapeJson(m_context.currentLocation) + "\",";
    json += "\"topic\":\"" + escapeJson(m_context.currentTopic) + "\",";
    json += "\"mood\":\"" + escapeJson(m_context.currentMood) + "\",";
    json += "\"focus\":\"" + escapeJson(m_context.currentFocusState) + "\",";
    json += "\"task\":\"" + escapeJson(m_context.currentTask) + "\"";
    json += "}";
    return json;
}

String ContextManager::getUserContextJson() const noexcept {
    String json;
    json.reserve(512);
    json += "{";
    json += "\"activity\":\"" + escapeJson(m_context.currentActivity) + "\",";
    json += "\"project\":\"" + escapeJson(m_context.currentProject) + "\",";
    json += "\"subject\":\"" + escapeJson(m_context.currentSubject) + "\",";
    json += "\"location\":\"" + escapeJson(m_context.currentLocation) + "\",";
    json += "\"topic\":\"" + escapeJson(m_context.currentTopic) + "\",";
    json += "\"mood\":\"" + escapeJson(m_context.currentMood) + "\",";
    json += "\"session\":\"" + escapeJson(m_context.currentWorkSession) + "\",";
    json += "\"device\":\"" + escapeJson(m_context.currentDevice) + "\",";
    json += "\"focus\":\"" + escapeJson(m_context.currentFocusState) + "\",";
    json += "\"task\":\"" + escapeJson(m_context.currentTask) + "\"";
    json += "}";
    return json;
}

String ContextManager::getContextAsJson() const noexcept {
    return getContextJson();
}

String ContextManager::getContextNaturalLanguage() const noexcept {
    String preamble;
    preamble.reserve(512);

    // Time and uptime
    unsigned long uptimeSec = millis() / 1000;
    unsigned long hours = uptimeSec / 3600;
    unsigned long mins = (uptimeSec % 3600) / 60;
    preamble += "The system has been running for " + String(hours) + "h " + String(mins) + "m. ";

    // WiFi status
    if (m_context.wifiConnected) {
        preamble += "WiFi is connected (signal: " + String(m_context.wifiRSSI) + " dBm). ";
    } else {
        preamble += "WiFi is not connected. ";
    }

    // Personality
    if (!m_context.activePersonality.isEmpty()) {
        preamble += "Active personality: " + m_context.activePersonality + ". ";
    }

    // User activity context
    if (!m_context.currentActivity.isEmpty()) {
        preamble += "The user is " + m_context.currentActivity + ". ";
    }
    if (!m_context.currentProject.isEmpty()) {
        preamble += "Active project: " + m_context.currentProject + ". ";
    }
    if (!m_context.currentTopic.isEmpty()) {
        preamble += "Current topic: " + m_context.currentTopic + ". ";
    }
    if (!m_context.currentLocation.isEmpty()) {
        preamble += "Location: " + m_context.currentLocation + ". ";
    }
    if (!m_context.currentMood.isEmpty()) {
        preamble += "The user's mood seems " + m_context.currentMood + ". ";
    }
    if (!m_context.currentTask.isEmpty()) {
        preamble += "Current task: " + m_context.currentTask + ". ";
    }
    if (!m_context.currentFocusState.isEmpty()) {
        preamble += "Focus state: " + m_context.currentFocusState + ". ";
    }

    // System state
    preamble += "Free heap: " + String(m_context.freeHeap) + " bytes. ";
    preamble += "Stored memories: " + String(m_context.memoryCount) + ". ";
    preamble += "Conversations today: " + String(m_context.conversationCount) + ". ";

    if (!m_context.lastConversation.isEmpty()) {
        preamble += "Last user said: \"" + m_context.lastConversation + "\". ";
    }

    return preamble;
}

const SystemContext& ContextManager::getContext() const noexcept {
    return m_context;
}

void ContextManager::refresh() noexcept {
    m_context.timestamp = millis();
    m_context.wifiConnected = (WiFi.status() == WL_CONNECTED);
    m_context.wifiRSSI = WiFi.RSSI();
    m_context.localIP = m_context.wifiConnected ? WiFi.localIP().toString() : "";
    m_context.freeHeap = ESP.getFreeHeap();
    m_context.totalHeap = ESP.getHeapSize();
    m_context.flashUsage = ESP.getFlashChipSize() - ESP.getFreeSketchSpace();
    m_context.cpuFreqMHz = static_cast<float>(ESP.getCpuFreqMHz());
    m_context.memoryCount = memoryManager.isInitialized() ? memoryManager.memoryCount() : 0;
    m_context.reminderCount = 0;
    // conversationCount and lastConversation are managed externally by ConversationManager
    m_context.systemState = "READY";
    m_context.activePersonality = personalityManager.isInitialized()
        ? personalityManager.getActiveProfileId() : "jarvis";
    m_context.sdMounted = storageManager.isHealthy();
    m_context.contextEntryCount = m_history.size();
    inferActivity();
}

void ContextManager::setActivity(const String& activity) noexcept {
    m_context.currentActivity = activity;
}

void ContextManager::setProject(const String& project) noexcept {
    if (m_context.currentProject != project && !project.isEmpty()) {
        ContextSnapshot snap;
        snap.timestamp = millis();
        snap.activity = m_context.currentActivity;
        snap.project = m_context.currentProject;
        snap.subject = m_context.currentSubject;
        snap.topic = m_context.currentTopic;
        snap.mood = m_context.currentMood;
        snap.task = m_context.currentTask;
        if (m_history.size() >= kMaxHistory) m_history.erase(m_history.begin());
        m_history.push_back(snap);
    }
    m_context.currentProject = project;
}

void ContextManager::setSubject(const String& subject) noexcept {
    m_context.currentSubject = subject;
}

void ContextManager::setLocation(const String& location) noexcept {
    m_context.currentLocation = location;
}

void ContextManager::setConversationTopic(const String& topic) noexcept {
    if (m_context.currentTopic != topic && !topic.isEmpty()) {
        if (m_history.size() >= kMaxHistory) m_history.erase(m_history.begin());
        ContextSnapshot snap;
        snap.timestamp = millis();
        snap.topic = m_context.currentTopic;
        snap.activity = m_context.currentActivity;
        m_history.push_back(snap);
    }
    m_context.currentTopic = topic;
}

void ContextManager::setMood(const String& mood) noexcept {
    m_context.currentMood = mood;
}

void ContextManager::setWorkSession(const String& session) noexcept {
    m_context.currentWorkSession = session;
}

void ContextManager::setDevice(const String& device) noexcept {
    m_context.currentDevice = device;
}

void ContextManager::setFocusState(const String& state) noexcept {
    m_context.currentFocusState = state;
}

void ContextManager::setConversationCount(size_t count) noexcept {
    m_context.conversationCount = count;
}

void ContextManager::setLastConversation(const String& summary) noexcept {
    m_context.lastConversation = summary;
}

void ContextManager::setTask(const String& task) noexcept {
    if (m_context.currentTask != task && !task.isEmpty()) {
        if (m_history.size() >= kMaxHistory) m_history.erase(m_history.begin());
        ContextSnapshot snap;
        snap.timestamp = millis();
        snap.task = m_context.currentTask;
        snap.activity = m_context.currentActivity;
        m_history.push_back(snap);
    }
    m_context.currentTask = task;
}

void ContextManager::setAssistantContext(AssistantContext ctx) noexcept {
    if (m_assistantContext == ctx) return;
    AssistantContext previous = m_assistantContext;
    m_assistantContext = ctx;
    m_contextSwitchTime = millis();
    unsigned long now = millis() / 86400000UL;
    if (now != m_lastContextSwitchDate) {
        m_contextSwitchCount = 0;
        m_lastContextSwitchDate = now;
    }
    m_contextSwitchCount++;
    if (eventBus.isInitialized()) {
        String data = "{\"from\":" + String(static_cast<uint8_t>(previous))
            + ",\"to\":" + String(static_cast<uint8_t>(ctx))
            + ",\"count\":" + String(m_contextSwitchCount) + "}";
        eventBus.publish(EventType::CONTEXT_SWITCHED, "ContextManager", data);
    }
    LOG_INFO(kLogCategory, "Context switched to %s (switch #%u today)", getAssistantContextName().c_str(), m_contextSwitchCount);
}

AssistantContext ContextManager::getAssistantContext() const noexcept {
    return m_assistantContext;
}

String ContextManager::getAssistantContextName() const noexcept {
    switch (m_assistantContext) {
        case AssistantContext::GENERAL:     return "General";
        case AssistantContext::STUDY:       return "Study";
        case AssistantContext::PROJECTS:    return "Projects";
        case AssistantContext::CODING:      return "Coding";
        case AssistantContext::RESEARCH:    return "Research";
        case AssistantContext::MAINTENANCE: return "Maintenance";
        case AssistantContext::MEETING:     return "Meeting";
        default:                            return "Unknown";
    }
}

bool ContextManager::switchToContext(AssistantContext ctx) noexcept {
    setAssistantContext(ctx);
    return true;
}

AssistantContext ContextManager::suggestContext() noexcept {
    if (!m_context.currentSubject.isEmpty()) return AssistantContext::STUDY;
    if (!m_context.currentProject.isEmpty()) return AssistantContext::PROJECTS;
    if (!m_context.currentActivity.isEmpty()) {
        String act = m_context.currentActivity;
        act.toLowerCase();
        if (act.indexOf("code") >= 0 || act.indexOf("program") >= 0 || act.indexOf("debug") >= 0) return AssistantContext::CODING;
        if (act.indexOf("research") >= 0 || act.indexOf("learn") >= 0 || act.indexOf("read") >= 0) return AssistantContext::RESEARCH;
        if (act.indexOf("update") >= 0 || act.indexOf("clean") >= 0 || act.indexOf("fix") >= 0 || act.indexOf("maintain") >= 0) return AssistantContext::MAINTENANCE;
        if (act.indexOf("meeting") >= 0 || act.indexOf("call") >= 0 || act.indexOf("conference") >= 0 || act.indexOf("sync") >= 0) return AssistantContext::MEETING;
    }
    if (!m_context.currentTask.isEmpty()) {
        String task = m_context.currentTask;
        task.toLowerCase();
        if (task.indexOf("meeting") >= 0 || task.indexOf("call") >= 0) return AssistantContext::MEETING;
    }
    if (!m_context.currentTopic.isEmpty()) {
        String topic = m_context.currentTopic;
        topic.toLowerCase();
        if (topic.indexOf("meeting") >= 0 || topic.indexOf("standup") >= 0 || topic.indexOf("sync") >= 0) return AssistantContext::MEETING;
    }
    return AssistantContext::GENERAL;
}

String ContextManager::getContextPreamble() const noexcept {
    String preamble = getContextNaturalLanguage();
    preamble += " Current assistant context: " + getAssistantContextName() + ". ";
    return preamble;
}

String ContextManager::getContextMemories() const noexcept {
    String ctxName = getAssistantContextName();
    if (ctxName.isEmpty() || ctxName == "General") return "";

    String result;
    auto memories = memoryManager.getByContext(ctxName);
    for (const auto& mem : memories) {
        if (mem.archived) continue;
        result += "- " + mem.key + ": " + mem.value;
        if (!mem.summary.isEmpty()) result += " (" + mem.summary + ")";
        result += "\n";
    }
    return result;
}

uint8_t ContextManager::getContextSwitchCount() const noexcept {
    return m_contextSwitchCount;
}

void ContextManager::clearExpired(unsigned long timeoutMs) noexcept {
    unsigned long now = millis();
    if (now - m_context.timestamp > timeoutMs) {
        m_context.currentActivity = "";
        m_context.currentFocusState = "idle";
    }
}

void ContextManager::saveContextHistory() noexcept {
    String json;
    json.reserve(2048);
    json += "{\"history\":[";
    for (size_t i = 0; i < m_history.size(); ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ts\":" + String(m_history[i].timestamp) + ",";
        json += "\"activity\":\"" + escapeJson(m_history[i].activity) + "\",";
        json += "\"project\":\"" + escapeJson(m_history[i].project) + "\",";
        json += "\"subject\":\"" + escapeJson(m_history[i].subject) + "\",";
        json += "\"topic\":\"" + escapeJson(m_history[i].topic) + "\",";
        json += "\"mood\":\"" + escapeJson(m_history[i].mood) + "\",";
        json += "\"task\":\"" + escapeJson(m_history[i].task) + "\"";
        json += "}";
    }
    json += "]}";
    storageManager.writeFile("/context/history.json", json, StorageType::SPIFFS);
}

String ContextManager::getContextHistory() const noexcept {
    String json;
    json.reserve(1024);
    json += "[";
    for (size_t i = 0; i < m_history.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"ts\":" + String(m_history[i].timestamp) + ",";
        json += "\"activity\":\"" + escapeJson(m_history[i].activity) + "\",";
        json += "\"project\":\"" + escapeJson(m_history[i].project) + "\",";
        json += "\"topic\":\"" + escapeJson(m_history[i].topic) + "\"}";
    }
    json += "]";
    return json;
}

void ContextManager::inferActivity() noexcept {
    if (!m_context.currentActivity.isEmpty()) return;
    String lastConv = m_context.lastConversation;
    if (lastConv.length() > 3) {
        String lower; lower.reserve(lastConv.length());
        for (size_t i = 0; i < lastConv.length(); ++i) lower += static_cast<char>(tolower(lastConv[i]));
        if (lower.indexOf("remind") >= 0) m_context.currentActivity = "reviewing reminders";
        else if (lower.indexOf("goal") >= 0) m_context.currentActivity = "reviewing goals";
        else if (lower.indexOf("habit") >= 0) m_context.currentActivity = "tracking habits";
        else if (lower.indexOf("memory") >= 0 || lower.indexOf("remember") >= 0) m_context.currentActivity = "recalling memories";
        else m_context.currentActivity = "conversation";
    }
}

bool ContextManager::isInitialized() const noexcept {
    return m_initialized;
}
