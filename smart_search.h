#ifndef AURA_SMART_SEARCH_H
#define AURA_SMART_SEARCH_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "event_bus.h"
#include "memory_manager.h"

enum class SearchDomain : uint8_t {
    MEMORY,
    CONVERSATIONS,
    REMINDERS,
    NOTIFICATIONS,
    NOTES,
    PROJECTS,
    KNOWLEDGE_BASE,
    SD_DOCUMENTS,
    DEVICE_LOGS,
    SETTINGS,
    TIMELINE,
    ALL
};

struct SearchResult {
    String id;
    String title;
    String snippet;
    String domain;
    float relevance;
    unsigned long timestamp;
    String sourceManager;
};

struct SearchQuery {
    String text;
    SearchDomain domain;
    bool partialMatch;
    bool semanticSearch;
    size_t maxResults;
    unsigned long startDate;
    unsigned long endDate;
    String categoryFilter;

    SearchQuery() noexcept : domain(SearchDomain::ALL), partialMatch(true),
        semanticSearch(false), maxResults(20), startDate(0), endDate(0) {}
};

class SmartSearch {
public:
    SmartSearch() noexcept;
    ~SmartSearch() noexcept;

    SmartSearch(const SmartSearch&) = delete;
    SmartSearch& operator=(const SmartSearch&) = delete;
    SmartSearch(SmartSearch&&) = delete;
    SmartSearch& operator=(SmartSearch&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] std::vector<SearchResult> search(const SearchQuery& query) noexcept;
    [[nodiscard]] std::vector<SearchResult> quickSearch(const String& text, SearchDomain domain = SearchDomain::ALL) noexcept;
    void clearCache() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "SmartSearch";
    static constexpr size_t kMaxCachedResults = 50;
    static constexpr size_t kMaxResultsPerDomain = 10;

    std::vector<SearchResult> searchMemory(const SearchQuery& query) noexcept;
    std::vector<SearchResult> searchConversations(const SearchQuery& query) noexcept;
    std::vector<SearchResult> searchReminders(const SearchQuery& query) noexcept;
    std::vector<SearchResult> searchKnowledgeBase(const SearchQuery& query) noexcept;
    std::vector<SearchResult> searchTimeline(const SearchQuery& query) noexcept;

    bool m_initialized;
    std::vector<SearchResult> m_cache;
    String m_lastQuery;
    unsigned long m_lastSearchTime;
};

extern SmartSearch smartSearch;

#endif // AURA_SMART_SEARCH_H
