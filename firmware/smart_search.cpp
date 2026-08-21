#include "smart_search.h"
#include <algorithm>
#include "memory_manager.h"
#include "reminder_manager.h"
#include "timeline_manager.h"

SmartSearch smartSearch;

SmartSearch::SmartSearch() noexcept
    : m_initialized(false), m_lastSearchTime(0) {}

SmartSearch::~SmartSearch() noexcept = default;

bool SmartSearch::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    LOG_INFO(kLogCategory, "SmartSearch initialized");
    return true;
}

void SmartSearch::update() noexcept {
}

std::vector<SearchResult> SmartSearch::search(const SearchQuery& query) noexcept {
    if (!m_initialized) return {};

    if (eventBus.isInitialized()) {
        eventBus.publish(EventType::SEARCH_STARTED, "SmartSearch",
                         "{\"query\":\"" + query.text + "\"}");
    }

    std::vector<SearchResult> allResults;

    if (query.domain == SearchDomain::ALL || query.domain == SearchDomain::MEMORY) {
        auto results = searchMemory(query);
        allResults.insert(allResults.end(), results.begin(), results.end());
    }
    if (query.domain == SearchDomain::ALL || query.domain == SearchDomain::CONVERSATIONS) {
        auto results = searchConversations(query);
        allResults.insert(allResults.end(), results.begin(), results.end());
    }
    if (query.domain == SearchDomain::ALL || query.domain == SearchDomain::REMINDERS) {
        auto results = searchReminders(query);
        allResults.insert(allResults.end(), results.begin(), results.end());
    }
    if (query.domain == SearchDomain::ALL || query.domain == SearchDomain::KNOWLEDGE_BASE) {
        auto results = searchKnowledgeBase(query);
        allResults.insert(allResults.end(), results.begin(), results.end());
    }
    if (query.domain == SearchDomain::ALL || query.domain == SearchDomain::TIMELINE) {
        auto results = searchTimeline(query);
        allResults.insert(allResults.end(), results.begin(), results.end());
    }

    std::sort(allResults.begin(), allResults.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.relevance > b.relevance;
    });

    if (allResults.size() > query.maxResults) {
        allResults.resize(query.maxResults);
    }

    m_cache = allResults;
    m_lastQuery = query.text;
    m_lastSearchTime = millis();

    if (eventBus.isInitialized()) {
        char count[8];
        snprintf(count, sizeof(count), "%u", static_cast<unsigned int>(allResults.size()));
        eventBus.publish(EventType::SEARCH_COMPLETED, "SmartSearch",
                         "{\"count\":" + String(count) + "}");
    }

    return allResults;
}

std::vector<SearchResult> SmartSearch::quickSearch(const String& text, SearchDomain domain) noexcept {
    SearchQuery q;
    q.text = text;
    q.domain = domain;
    q.maxResults = 5;
    return search(q);
}

void SmartSearch::clearCache() noexcept {
    m_cache.clear();
    m_lastQuery = "";
}

bool SmartSearch::isInitialized() const noexcept {
    return m_initialized;
}

std::vector<SearchResult> SmartSearch::searchMemory(const SearchQuery& query) noexcept {
    std::vector<SearchResult> results;
    if (!memoryManager.isInitialized()) return results;

    SearchCriteria criteria;
    criteria.maxResults = kMaxResultsPerDomain;
    criteria.semanticSearch = query.semanticSearch;

    auto entries = memoryManager.search(criteria);
    String lowerQuery = query.text;
    lowerQuery.toLowerCase();

    for (const auto& entry : entries) {
        String lowerKey = entry.key;
        String lowerValue = entry.value;
        lowerKey.toLowerCase();
        lowerValue.toLowerCase();

        float relevance = 0.0f;
        if (query.partialMatch) {
            if (lowerKey.indexOf(lowerQuery) >= 0) relevance = 0.8f;
            else if (lowerValue.indexOf(lowerQuery) >= 0) relevance = 0.6f;
        }
        if (lowerKey == lowerQuery) relevance = 1.0f;

        if (relevance > 0.0f) {
            // Boost by importance, confidence, and recency
            relevance += static_cast<float>(entry.importance) * 0.002f;
            relevance *= entry.confidence;
            unsigned long ageHours = (millis() - entry.lastAccessed) / 3600000UL;
            if (ageHours < 24) relevance += 0.1f;
            if (entry.favorite) relevance += 0.15f;
            if (entry.pinned) relevance += 0.2f;

            SearchResult r;
            r.id = entry.id;
            r.title = entry.key;
            r.snippet = entry.value.length() > 80 ? entry.value.substring(0, 80) + "..." : entry.value;
            r.domain = "memory";
            r.relevance = relevance;
            r.timestamp = entry.timestamp;
            r.sourceManager = "MemoryManager";
            results.push_back(r);
        }
    }

    return results;
}

std::vector<SearchResult> SmartSearch::searchConversations(const SearchQuery& query) noexcept {
    std::vector<SearchResult> results;
    if (!memoryManager.isInitialized()) return results;

    auto convs = memoryManager.searchConversations(query.text);
    for (const auto& conv : convs) {
        if (query.startDate > 0 && conv.timestamp < query.startDate) continue;
        if (query.endDate > 0 && conv.timestamp > query.endDate) continue;

        SearchResult r;
        r.id = conv.id;
        r.title = conv.title;
        r.snippet = conv.summary.length() > 80 ? conv.summary.substring(0, 80) + "..." : conv.summary;
        r.domain = "conversation";
        r.relevance = 0.7f;
        r.timestamp = conv.timestamp;
        r.sourceManager = "MemoryManager";
        results.push_back(r);
    }

    return results;
}

std::vector<SearchResult> SmartSearch::searchReminders(const SearchQuery& query) noexcept {
    std::vector<SearchResult> results;
    if (!reminderManager.isInitialized()) return results;

    std::vector<Reminder> reminders;
    reminderManager.getReminders(reminders);

    String lowerQuery = query.text;
    lowerQuery.toLowerCase();

    for (const auto& rem : reminders) {
        String lowerTitle = rem.title;
        String lowerMsg = rem.message;
        lowerTitle.toLowerCase();
        lowerMsg.toLowerCase();

        float relevance = 0.0f;
        if (query.partialMatch) {
            if (lowerTitle.indexOf(lowerQuery) >= 0) relevance = 0.9f;
            else if (lowerMsg.indexOf(lowerQuery) >= 0) relevance = 0.5f;
        }

        if (relevance > 0.0f) {
            if (query.startDate > 0 && rem.createdTime < query.startDate) continue;
            if (query.endDate > 0 && rem.createdTime > query.endDate) continue;

            SearchResult r;
            r.id = String((unsigned long)rem.id);
            r.title = rem.title;
            r.snippet = rem.message.length() > 80 ? rem.message.substring(0, 80) + "..." : rem.message;
            r.domain = "reminder";
            r.relevance = relevance;
            r.timestamp = rem.createdTime;
            r.sourceManager = "ReminderManager";
            results.push_back(r);
        }
    }

    return results;
}

std::vector<SearchResult> SmartSearch::searchKnowledgeBase(const SearchQuery& query) noexcept {
    std::vector<SearchResult> results;
    if (!memoryManager.isInitialized()) return results;

    SearchCriteria criteria;
    criteria.maxResults = kMaxResultsPerDomain;
    criteria.semanticSearch = query.semanticSearch;

    auto entries = memoryManager.search(criteria);
    String lowerQuery = query.text;
    lowerQuery.toLowerCase();

    for (const auto& entry : entries) {
        String lowerKey = entry.key;
        String lowerValue = entry.value;
        lowerKey.toLowerCase();
        lowerValue.toLowerCase();

        float relevance = 0.0f;
        if (query.partialMatch) {
            if (lowerKey.indexOf(lowerQuery) >= 0) relevance = 0.85f;
            else if (lowerValue.indexOf(lowerQuery) >= 0) relevance = 0.65f;
        }

        if (relevance > 0.0f && entry.persistent) {
            SearchResult r;
            r.id = entry.id;
            r.title = entry.key;
            r.snippet = entry.value.length() > 80 ? entry.value.substring(0, 80) + "..." : entry.value;
            r.domain = "knowledge";
            r.relevance = relevance;
            r.timestamp = entry.timestamp;
            r.sourceManager = "MemoryManager";
            results.push_back(r);
        }
    }

    return results;
}

std::vector<SearchResult> SmartSearch::searchTimeline(const SearchQuery& query) noexcept {
    std::vector<SearchResult> results;
    if (!timelineManager.isInitialized()) return results;

    auto entries = timelineManager.search(query.text);
    for (const auto& entry : entries) {
        if (query.startDate > 0 && entry.timestamp < query.startDate) continue;
        if (query.endDate > 0 && entry.timestamp > query.endDate) continue;

        float relevance = 0.6f;
        unsigned long ageHours = (millis() - entry.timestamp) / 3600000UL;
        if (ageHours < 24) relevance += 0.15f;

        SearchResult r;
        r.id = entry.id;
        r.title = entry.category;
        r.snippet = entry.summary.length() > 80 ? entry.summary.substring(0, 80) + "..." : entry.summary;
        r.domain = "timeline";
        r.relevance = relevance;
        r.timestamp = entry.timestamp;
        r.sourceManager = "TimelineManager";
        results.push_back(r);
    }

    return results;
}
