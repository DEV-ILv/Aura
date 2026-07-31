#include "semantic_search_manager.h"
#include <algorithm>
#include "memory_manager.h"
#include "knowledge_graph_manager.h"
#include "goal_manager.h"
#include "habit_manager.h"
#include "planner_manager.h"
#include "reminder_manager.h"
#include "timeline_manager.h"
#include "context_manager.h"

SemanticSearchManager semanticSearchManager;

SemanticSearchManager::SemanticSearchManager() noexcept
    : m_initialized(false) {
    m_cache.reserve(kMaxCache);
}

SemanticSearchManager::~SemanticSearchManager() noexcept {}

bool SemanticSearchManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized");
    return true;
}

void SemanticSearchManager::update() noexcept {
    if (!m_initialized) return;
    unsigned long now = millis();
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (now - it->timestamp > 60000) it = m_cache.erase(it);
        else ++it;
    }
}

std::vector<SemanticResult> SemanticSearchManager::search(const String& query) noexcept {
    std::vector<SemanticResult> results;
    if (!m_initialized || query.isEmpty()) return results;

    String normalized = normalize(query);

    if (checkCache(normalized, results)) return results;

    results.reserve(kMaxResults);

    String terms = normalized;
    String lower = normalized;

    if (memoryManager.isInitialized()) {
        auto memories = memoryManager.search(normalized, true);
        for (const auto& m : memories) {
            SemanticResult r;
            r.source = "memory";
            r.id = m.id;
            r.title = m.key;
            r.snippet = m.value;
            r.relevance = computeRelevance(normalized, m.key + " " + m.value);
            r.timestamp = m.timestamp;
            results.push_back(r);
        }
    }

    if (knowledgeGraphManager.isInitialized()) {
        auto nodes = knowledgeGraphManager.searchNodes(normalized);
        for (const auto& n : nodes) {
            SemanticResult r;
            r.source = "knowledge_graph";
            r.id = n.id;
            r.title = n.name;
            r.snippet = n.value;
            r.relevance = computeRelevance(normalized, n.name + " " + n.value + " " + n.tags);
            r.timestamp = n.updatedAt;
            results.push_back(r);
        }
    }

    if (goalManager.isInitialized()) {
        auto goals = goalManager.getActiveGoals();
        for (const auto& g : goals) {
            float rel = computeRelevance(normalized, g.title + " " + g.description);
            if (rel >= kMinConfidence) {
                SemanticResult r;
                r.source = "goal";
                r.id = g.id;
                r.title = g.title;
                r.snippet = g.description;
                r.relevance = rel;
                results.push_back(r);
            }
        }
    }

    if (habitManager.isInitialized()) {
        auto habits = habitManager.getAllHabits();
        for (const auto& h : habits) {
            float rel = computeRelevance(normalized, h.name + " " + h.description);
            if (rel >= kMinConfidence) {
                SemanticResult r;
                r.source = "habit";
                r.id = h.id;
                r.title = h.name;
                r.snippet = h.description;
                r.relevance = rel;
                results.push_back(r);
            }
        }
    }

    if (plannerManager.isInitialized()) {
        auto tasks = plannerManager.getUpcomingTasks();
        for (const auto& t : tasks) {
            float rel = computeRelevance(normalized, t.title);
            if (rel >= kMinConfidence) {
                SemanticResult r;
                r.source = "planner";
                r.id = t.id;
                r.title = t.title;
                r.snippet = "";
                r.relevance = rel;
                r.timestamp = t.deadline;
                results.push_back(r);
            }
        }
    }

    if (reminderManager.isInitialized()) {
        std::vector<Reminder> reminders;
        reminderManager.getReminders(reminders);
        for (const auto& r : reminders) {
            float rel = computeRelevance(normalized, r.title + " " + r.message);
            if (rel >= kMinConfidence) {
                SemanticResult sr;
                sr.source = "reminder";
                sr.id = String(r.id);
                sr.title = r.title;
                sr.snippet = r.message;
                sr.relevance = rel;
                sr.timestamp = static_cast<unsigned long>(r.triggerTime);
                results.push_back(sr);
            }
        }
    }

    if (timelineManager.isInitialized()) {
        auto tl = timelineManager.search(normalized);
        for (const auto& t : tl) {
            SemanticResult r;
            r.source = "timeline";
            r.id = t.id;
            r.title = t.category;
            r.snippet = t.summary;
            r.relevance = t.importance * 0.25f;
            r.timestamp = t.timestamp;
            results.push_back(r);
        }
    }

    std::sort(results.begin(), results.end(), [](const SemanticResult& a, const SemanticResult& b) {
        return a.relevance > b.relevance;
    });

    if (results.size() > kMaxResults) results.resize(kMaxResults);

    addCache(normalized, results);
    return results;
}

bool SemanticSearchManager::isInitialized() const noexcept { return m_initialized; }
size_t SemanticSearchManager::getCacheSize() const noexcept { return m_cache.size(); }
void SemanticSearchManager::clearCache() noexcept { m_cache.clear(); }

String SemanticSearchManager::normalize(const String& text) const noexcept {
    String result;
    result.reserve(text.length());
    for (size_t i = 0; i < text.length(); ++i) {
        char c = static_cast<char>(tolower(static_cast<unsigned char>(text[i])));
        if (c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c == ' ') result += c;
    }
    result.trim();
    while (result.indexOf("  ") >= 0) result.replace("  ", " ");
    return result;
}

bool SemanticSearchManager::keywordMatch(const String& text, const String& keyword) const noexcept {
    return text.indexOf(keyword) >= 0;
}

float SemanticSearchManager::computeRelevance(const String& query, const String& target) const noexcept {
    if (query.isEmpty() || target.isEmpty()) return 0.0f;
    String nq = normalize(query);
    String nt = normalize(target);
    std::vector<String> qTerms;
    int s = 0;
    while (s < (int)nq.length()) {
        int c = nq.indexOf(' ', s);
        if (c < 0) { if (nq.substring(s).length() > 1) qTerms.push_back(nq.substring(s)); break; }
        if (c > s) { String t = nq.substring(s, c); if (t.length() > 1) qTerms.push_back(t); }
        s = c + 1;
    }
    if (qTerms.empty()) return 0.0f;
    size_t matches = 0;
    for (const auto& t : qTerms) {
        if (keywordMatch(nt, t)) matches++;
    }
    if (keywordMatch(nt, nq)) return 1.0f;
    return static_cast<float>(matches) / static_cast<float>(qTerms.size());
}

std::vector<String> SemanticSearchManager::extractTerms(const String& text) const noexcept {
    std::vector<String> terms;
    int s = 0;
    while (s < (int)text.length()) {
        int c = text.indexOf(' ', s);
        if (c < 0) { terms.push_back(text.substring(s)); break; }
        if (c > s) terms.push_back(text.substring(s, c));
        s = c + 1;
    }
    return terms;
}

bool SemanticSearchManager::checkCache(const String& normalized, std::vector<SemanticResult>& results) noexcept {
    for (const auto& entry : m_cache) {
        if (entry.query == normalized) {
            results = entry.results;
            return true;
        }
    }
    return false;
}

void SemanticSearchManager::addCache(const String& normalized, const std::vector<SemanticResult>& results) noexcept {
    while (m_cache.size() >= kMaxCache) m_cache.erase(m_cache.begin());
    SearchCacheEntry entry;
    entry.query = normalized;
    entry.timestamp = millis();
    entry.results = results;
    m_cache.push_back(entry);
}
