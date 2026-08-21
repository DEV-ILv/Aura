#ifndef AURA_SEMANTIC_SEARCH_MANAGER_H
#define AURA_SEMANTIC_SEARCH_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct SemanticResult {
    String source;
    String id;
    String title;
    String snippet;
    float relevance;
    unsigned long timestamp;

    SemanticResult() noexcept : relevance(0.0f), timestamp(0) {}
};

struct SearchCacheEntry {
    String query;
    unsigned long timestamp;
    std::vector<SemanticResult> results;

    SearchCacheEntry() noexcept : timestamp(0) {}
};

class SemanticSearchManager {
public:
    SemanticSearchManager() noexcept;
    ~SemanticSearchManager() noexcept;

    SemanticSearchManager(const SemanticSearchManager&) = delete;
    SemanticSearchManager& operator=(const SemanticSearchManager&) = delete;
    SemanticSearchManager(SemanticSearchManager&&) = delete;
    SemanticSearchManager& operator=(SemanticSearchManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    [[nodiscard]] std::vector<SemanticResult> search(const String& query) noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] size_t getCacheSize() const noexcept;
    void clearCache() noexcept;

private:
    static constexpr const char* kLogCategory = "SemanticSearch";
    static constexpr size_t kMaxResults = SEMANTIC_MAX_RESULTS;
    static constexpr size_t kMaxCache = SEMANTIC_CACHE_SIZE;
    static constexpr float kMinConfidence = SEMANTIC_MIN_CONFIDENCE;

    String normalize(const String& text) const noexcept;
    bool keywordMatch(const String& text, const String& keyword) const noexcept;
    float computeRelevance(const String& query, const String& target) const noexcept;
    std::vector<String> extractTerms(const String& text) const noexcept;
    bool checkCache(const String& normalized, std::vector<SemanticResult>& results) noexcept;
    void addCache(const String& normalized, const std::vector<SemanticResult>& results) noexcept;

    bool m_initialized;
    std::vector<SearchCacheEntry> m_cache;
};

extern SemanticSearchManager semanticSearchManager;

#endif
