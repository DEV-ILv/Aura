#ifndef AURA_MEMORY_MANAGER_H
#define AURA_MEMORY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

/**
 * @enum MemoryCategory
 * @brief Categories for memory entries
 */
enum class MemoryCategory : uint8_t {
    USER,           ///< User profile information
    PREFERENCE,     ///< User preferences
    REMINDER,       ///< Reminders
    PROJECT,        ///< Project information
    CONVERSATION,   ///< Conversation summary
    FACT,           ///< Facts and knowledge
    LOCATION,       ///< Location data
    GOAL,           ///< User goals
    CUSTOM          ///< Custom memory entries
};

/**
 * @struct MemoryEntry
 * @brief Single memory entry with metadata
 */
struct MemoryEntry {
    String          id;             ///< Unique identifier
    MemoryCategory  category;       ///< Memory category
    String          key;            ///< Memory key
    String          value;          ///< Memory value
    unsigned long   timestamp;      ///< Creation timestamp (millis)
    uint8_t         priority;       ///< Priority (0-255, higher = more important)
    uint16_t        accessCount;    ///< Number of times accessed
    unsigned long   lastAccessed;   ///< Last access timestamp
    bool            persistent;     ///< Survives eviction
    bool            favorite;       ///< Marked as favorite
    unsigned long   expiryTime;     ///< Expiry timestamp (0 = never)
    uint8_t         importance;     ///< Computed importance score (0-255)
    String          tags;           ///< Comma-separated tags for semantic search
    bool            pinned;         ///< User-pinned (never evicted or deleted)
    bool            archived;       ///< Moved to archive (hidden from normal queries)
    float           confidence;     ///< Confidence score (0.0-1.0), how certain this memory is correct
    String          source;         ///< Source identifier (conversation ID, module name, etc.)
    String          summary;        ///< Auto-generated brief summary
    String          contextName;    ///< Assistant context tag (e.g. "STUDY", "CODING")

    MemoryEntry() noexcept
        : category(MemoryCategory::CUSTOM), timestamp(0), priority(0),
          accessCount(0), lastAccessed(0), persistent(false), favorite(false),
          expiryTime(0), importance(0), pinned(false), archived(false),
          confidence(1.0f) {}

    MemoryEntry(MemoryCategory cat, const String& k, const String& v,
                uint8_t prio = 0, bool persist = false) noexcept
        : category(cat), key(k), value(v), priority(prio),
          accessCount(0), lastAccessed(0), persistent(persist), favorite(false),
          expiryTime(0), importance(0), pinned(false), archived(false),
          confidence(1.0f) {}

    /**
     * @brief Compute confidence-weighted importance
     */
    [[nodiscard]] float computeEffectiveImportance() const noexcept {
        return static_cast<float>(importance) * confidence;
    }

    /**
     * @brief Compute a relevance score for ranking
     * @return Score (higher = more relevant)
     */
    [[nodiscard]] float computeRelevanceScore() const noexcept {
        float score = 0.0f;
        score += static_cast<float>(importance) * 2.0f * confidence;
        if (favorite) score += 50.0f;
        if (pinned) score += 80.0f;
        unsigned long age = millis() - lastAccessed;
        float recencyFactor = (age > 0) ? 1000.0f / static_cast<float>(age) : 1.0f;
        score += recencyFactor * 10.0f;
        score += static_cast<float>(accessCount) * 0.5f;
        score += static_cast<float>(priority) * 0.5f;
        return score;
    }
};

/**
 * @struct SearchCriteria
 * @brief Search parameters for memory lookup
 */
struct MemoryRevision {
    String          id;             ///< Unique revision ID
    String          memoryId;       ///< Parent memory entry ID
    String          previousValue;  ///< Value before change
    unsigned long   timestamp;      ///< When revision was created
    String          tags;           ///< Tags snapshot at revision time

    MemoryRevision() noexcept : timestamp(0) {}
};

struct SearchCriteria {
    String          keyPattern;         ///< Partial or exact key match (empty = no filter)
    MemoryCategory  category;           ///< Category filter
    bool            filterCategory;     ///< Enable category filter
    uint8_t         minPriority;        ///< Minimum priority threshold
    bool            filterPriority;     ///< Enable priority filter
    uint8_t         minImportance;      ///< Minimum importance (0 = any)
    bool            filterImportance;   ///< Enable importance filter
    bool            favoritesOnly;      ///< Only return favorites
    bool            includeArchived;    ///< Include archived entries
    size_t          maxResults;         ///< Maximum results (0 = unlimited)
    bool            semanticSearch;     ///< Enable semantic (tag-based) search
    String          searchTags;         ///< Tags for semantic matching
    String          contextFilter;      ///< Filter by context name (empty = no filter)
    float           minConfidence;      ///< Minimum confidence threshold (0.0 = any)
    bool            filterConfidence;   ///< Enable confidence filter

    SearchCriteria() noexcept
        : keyPattern(""), category(MemoryCategory::CUSTOM), filterCategory(false),
          minPriority(0), filterPriority(false), minImportance(0), filterImportance(false),
          favoritesOnly(false), includeArchived(false), maxResults(0),
          semanticSearch(false), minConfidence(0.0f), filterConfidence(false) {}
};

/**
 * @struct ConversationRecord
 * @brief Conversation history entry
 */
struct ConversationRecord {
    String          id;             ///< Unique conversation ID
    String          title;          ///< Conversation title
    unsigned long   timestamp;      ///< Creation timestamp
    unsigned long   lastActivity;   ///< Last activity timestamp
    String          summary;        ///< AI-generated summary
    String          importantMessages; ///< Key messages excerpt
    uint16_t        messageCount;   ///< Number of messages
    bool            archived;       ///< Archived state
    bool            favorite;       ///< Favorite state

    ConversationRecord() noexcept
        : timestamp(0), lastActivity(0), messageCount(0),
          archived(false), favorite(false) {}
};

/**
 * @class MemoryManager
 * @brief Single authority for AI long-term memory management
 *
 * Decides WHAT to remember; delegates HOW to StorageManager.
 * Supports prioritized LRU eviction, JSON serialization,
 * and search across multiple dimensions.
 *
 * Thread-safe for FreeRTOS. Non-blocking. ESP32-optimized.
 */
class MemoryManager {
public:
    /**
     * @brief Constructor
     */
    MemoryManager() noexcept;

    /**
     * @brief Destructor
     */
    ~MemoryManager() noexcept;

    // Delete copy semantics
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Delete move semantics
    MemoryManager(MemoryManager&&) = delete;
    MemoryManager& operator=(MemoryManager&&) = delete;

    /**
     * @brief Initialize memory manager
     * @return true if initialization successful
     * @note Loads persisted memories from StorageManager
     */
    [[nodiscard]] bool initialize() noexcept;

    /**
     * @brief Scheduler-compatible update method
     */
    void run() noexcept;

    /**
     * @brief Update memory manager state
     * @note Call regularly from loop()
     */
    void update() noexcept;

    // ========================================================================
    // Memory CRUD
    // ========================================================================

    /**
     * @brief Store a new memory entry
     * @param category Memory category
     * @param key Memory key
     * @param value Memory value
     * @param priority Priority level (0-255)
     * @param persistent Whether entry survives eviction
     * @return Unique ID of the stored entry, or empty on failure
     */
    [[nodiscard]] String remember(
        MemoryCategory category,
        const String& key,
        const String& value,
        uint8_t priority = 0,
        bool persistent = false) noexcept;

    /**
     * @brief Remove a memory entry by ID
     * @param id Unique identifier of entry to remove
     * @return true if entry was found and removed
     */
    [[nodiscard]] bool forget(const String& id) noexcept;

    /**
     * @brief Update an existing memory entry
     * @param id Unique identifier
     * @param newKey New key (empty = keep existing)
     * @param newValue New value (empty = keep existing)
     * @param newPriority New priority (0xFF = keep existing)
     * @return true if entry was found and updated
     */
    [[nodiscard]] bool updateMemory(
        const String& id,
        const String& newKey,
        const String& newValue,
        uint8_t newPriority = 0xFF) noexcept;

    // ========================================================================
    // Query
    // ========================================================================

    /**
     * @brief Search memories by criteria
     * @param criteria Search parameters
     * @return Vector of matching MemoryEntry copies
     */
    [[nodiscard]] std::vector<MemoryEntry> search(
        const SearchCriteria& criteria) const noexcept;

    /**
     * @brief Quick search by key (exact or partial match)
     * @param keyPattern Key to search for
     * @param exactMatch If true, requires exact match
     * @return Vector of matching entries
     */
    [[nodiscard]] std::vector<MemoryEntry> search(
        const String& keyPattern,
        bool exactMatch = false) const noexcept;

    /**
     * @brief Check if a memory entry exists by key
     * @param key Key to check
     * @return true if an entry with this key exists
     */
    [[nodiscard]] bool exists(const String& key) const noexcept;

    /**
     * @brief Get a single memory entry by ID
     * @param id Unique identifier
     * @return Copy of the entry (empty id if not found)
     */
    [[nodiscard]] MemoryEntry get(const String& id) const noexcept;

    /**
     * @brief Get all memory entries
     * @return Vector of all entries
     */
    [[nodiscard]] const std::vector<MemoryEntry>& getAll() const noexcept;

    /**
     * @brief Get all entries in a category
     * @param category Category to filter by
     * @return Vector of matching entries
     */
    [[nodiscard]] std::vector<MemoryEntry> getByCategory(
        MemoryCategory category) const noexcept;

    // ========================================================================
    // Metadata Operations
    // ========================================================================

    /**
     * @brief Mark/unmark a memory as favorite
     * @param id Unique identifier
     * @param favorite New favorite state
     * @return true if entry was found
     */
    [[nodiscard]] bool markFavorite(const String& id, bool favorite = true) noexcept;

    /**
     * @brief Set priority of a memory entry
     * @param id Unique identifier
     * @param priority New priority (0-255)
     * @return true if entry was found
     */
    [[nodiscard]] bool setPriority(const String& id, uint8_t priority) noexcept;

    /**
     * @brief Increment access count for a memory entry
     * @param id Unique identifier
     * @return true if entry was found
     */
    [[nodiscard]] bool incrementAccess(const String& id) noexcept;

    // ========================================================================
    // Bulk Operations
    // ========================================================================

    /**
     * @brief Clear all memory entries
     * @note Does NOT persist automatically
     */
    void clear() noexcept;

    /**
     * @brief Clear all entries in a category
     * @param category Category to clear
     */
    void clearCategory(MemoryCategory category) noexcept;

    // ========================================================================
    // Serialization
    // ========================================================================

    /**
     * @brief Export all memories as JSON string
     * @return JSON-formatted string
     */
    [[nodiscard]] String exportJson() const noexcept;

    /**
     * @brief Import memories from JSON string
     * @param json JSON-formatted memory data
     * @return Number of entries imported
     */
    [[nodiscard]] size_t importJson(const String& json) noexcept;

    // ========================================================================
    // Persistence
    // ========================================================================

    /**
     * @brief Save all memories to persistent storage via StorageManager
     * @return true if save was successful
     */
    [[nodiscard]] bool save() noexcept;

    /**
     * @brief Load all memories from persistent storage via StorageManager
     * @return true if load was successful
     */
    [[nodiscard]] bool load() noexcept;

    // ========================================================================
    // Status
    // ========================================================================

    /**
     * @brief Check if module is initialized
     * @return true if initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Get total number of stored memory entries
     * @return Entry count
     */
    [[nodiscard]] size_t memoryCount() const noexcept;

    // ========================================================================
    // Conversation History
    // ========================================================================

    /**
     * @brief Save a conversation record
     * @param record Conversation record
     * @return true if saved
     */
    [[nodiscard]] bool saveConversation(const ConversationRecord& record) noexcept;

    /**
     * @brief Get all conversation records
     * @return Vector of conversation records
     */
    [[nodiscard]] const std::vector<ConversationRecord>& getConversations() const noexcept;

    /**
     * @brief Search conversations by text
     * @param query Search text
     * @return Matching conversation records
     */
    [[nodiscard]] std::vector<ConversationRecord> searchConversations(const String& query) const noexcept;

    /**
     * @brief Delete a conversation record
     * @param conversationId Conversation ID
     * @return true if deleted
     */
    [[nodiscard]] bool deleteConversation(const String& conversationId) noexcept;

    /**
     * @brief Archive a conversation
     * @param conversationId Conversation ID
     * @return true if archived
     */
    [[nodiscard]] bool archiveConversation(const String& conversationId) noexcept;

    /**
     * @brief Mark conversation as favorite
     * @param conversationId Conversation ID
     * @param favorite Favorite state
     * @return true if updated
     */
    [[nodiscard]] bool favoriteConversation(const String& conversationId, bool favorite = true) noexcept;

    /**
     * @brief Get conversation count
     * @return Number of stored conversations
     */
    [[nodiscard]] size_t conversationCount() const noexcept;

    // ========================================================================
    // Confidence & Source
    // ========================================================================

    /**
     * @brief Set confidence score for a memory entry
     * @param id Unique identifier
     * @param confidence Confidence (0.0-1.0)
     * @return true if entry was found
     */
    [[nodiscard]] bool setConfidence(const String& id, float confidence) noexcept;

    /**
     * @brief Set source for a memory entry
     * @param id Unique identifier
     * @param source Source string
     * @return true if entry was found
     */
    [[nodiscard]] bool setSource(const String& id, const String& source) noexcept;

    /**
     * @brief Set summary for a memory entry
     * @param id Unique identifier
     * @param summary Brief summary text
     * @return true if entry was found
     */
    [[nodiscard]] bool setSummary(const String& id, const String& summary) noexcept;

    /**
     * @brief Get memories by context
     * @param contextName Assistant context name
     * @return Matching entries
     */
    [[nodiscard]] std::vector<MemoryEntry> getByContext(const String& contextName) const noexcept;

    /**
     * @brief Find low-confidence memories that need verification
     * @param maxConfidence Maximum confidence threshold (default 0.5)
     * @return Entries below threshold
     */
    [[nodiscard]] std::vector<MemoryEntry> getLowConfidence(float maxConfidence = 0.5f) const noexcept;

    // ========================================================================
    // Smart Memory Ranking
    // ========================================================================

    /**
     * @brief Get ranked memories by relevance score
     * @param topN Number of top entries to return (0 = all)
     * @return Ranked vector of memory entries
     */
    [[nodiscard]] std::vector<MemoryEntry> getRankedMemories(size_t topN = MEMORY_RANK_TOP_N) const noexcept;

    /**
     * @brief Update importance scores for all entries
     */
    void updateImportanceScores() noexcept;

    // ========================================================================
    // Memory Importance Engine
    // ========================================================================

    /**
     * @brief Pin/unpin a memory (pinned memories are never evicted)
     * @param id Memory entry ID
     * @param pinned New pinned state
     * @return true if entry was found
     */
    [[nodiscard]] bool setPin(const String& id, bool pinned = true) noexcept;

    /**
     * @brief Archive/unarchive a memory
     * @param id Memory entry ID
     * @param archived New archived state
     * @return true if entry was found
     */
    [[nodiscard]] bool setArchive(const String& id, bool archived = true) noexcept;

    /**
     * @brief Get all archived memories
     * @return Vector of archived entries
     */
    [[nodiscard]] std::vector<MemoryEntry> getArchived() const noexcept;

    /**
     * @brief Get all pinned memories
     * @return Vector of pinned entries
     */
    [[nodiscard]] std::vector<MemoryEntry> getPinned() const noexcept;

    /**
     * @brief Age importance scores over time
     * @note Called periodically to decay old scores
     */
    void runAging() noexcept;

    // ========================================================================
    // Versioned Memory
    // ========================================================================

    /**
     * @brief Save a revision before memory update
     * @param memoryId Memory entry ID
     * @param previousValue Previous value to preserve
     * @return Revision ID, or empty on failure
     */
    [[nodiscard]] String saveRevision(const String& memoryId, const String& previousValue) noexcept;

    /**
     * @brief Get all revisions for a given memory
     * @param memoryId Memory entry ID
     * @return Vector of revisions (empty if none)
     */
    [[nodiscard]] std::vector<MemoryRevision> getRevisions(const String& memoryId) const noexcept;

    /**
     * @brief Restore a memory to a previous revision
     * @param revisionId Revision ID to restore
     * @return true if restore succeeded
     */
    [[nodiscard]] bool restoreRevision(const String& revisionId) noexcept;

    /**
     * @brief Compare two revisions and return a diff description
     * @param revisionId1 First revision ID
     * @param revisionId2 Second revision ID
     * @return Human-readable diff string
     */
    [[nodiscard]] String compareRevisions(const String& revisionId1, const String& revisionId2) const noexcept;

    // ========================================================================
    // Semantic Search
    // ========================================================================

    /**
     * @brief Search memories by semantic similarity (tag-based)
     * @param query Search query text
     * @param maxResults Maximum results
     * @return Matching entries sorted by relevance
     */
    [[nodiscard]] std::vector<MemoryEntry> semanticSearch(const String& query, size_t maxResults = 5) const noexcept;

    /**
     * @brief Find memories related to a given entry
     * @param memoryId Entry ID to find related memories for
     * @param maxResults Maximum results
     * @return Related entries
     */
    [[nodiscard]] std::vector<MemoryEntry> findRelated(const String& memoryId, size_t maxResults = 5) const noexcept;

    // ========================================================================
    // Automatic Maintenance
    // ========================================================================

    /**
     * @brief Run automatic memory maintenance
     * @note Called periodically to merge duplicates, archive old, remove expired
     * @return Number of entries cleaned up
     */
    [[nodiscard]] size_t runMaintenance() noexcept;

    /**
     * @brief Remove expired temporary memories
     * @return Number removed
     */
    [[nodiscard]] size_t removeExpired() noexcept;

    /**
     * @brief Merge duplicate memory entries (same key+value)
     * @return Number of duplicates merged
     */
    [[nodiscard]] size_t mergeDuplicates() noexcept;

    /**
     * @brief Fuzzy dedup — merge entries with similar keys (edit distance based)
     * @param threshold Similarity threshold (0.0-1.0, default 0.85)
     * @return Number of entries merged
     */
    [[nodiscard]] size_t semanticDedup(float threshold = 0.85f) noexcept;

    /**
     * @brief Consolidate low-confidence facts by cross-referencing with similar entries
     * @return Number of entries updated
     */
    [[nodiscard]] size_t consolidateFacts() noexcept;

private:
    // Constants
    static constexpr const char* kLogCategory = "MemoryManager";
    static constexpr const char* kStoragePath = "/memory.json";
    static constexpr const char* kConversationsPath = "/conversations.json";
    static constexpr size_t      kDefaultMaxEntries = 256;
    static constexpr size_t      kEvictionBatchSize = 16;
    static constexpr size_t      kGeneratedIdLength = 16;
    static constexpr uint8_t     kEvictionMinPriority = 64;
    static constexpr size_t      kMaxConversations = MAX_CONVERSATION_HISTORY;
    static constexpr unsigned long kMaintenanceInterval = MEMORY_CLEANUP_INTERVAL_MS;
    static constexpr const char* kHexChars = "0123456789abcdef";
    static constexpr size_t      kMaxRevisionsPerMemory = 10;
    static constexpr const char* kRevisionsPath = "/memory_revisions.json";
    static constexpr unsigned long kAutoAgeIntervalMs = 3600000;  // 1 hour
    static constexpr uint8_t     kMinImportanceAfterAge = 10;
    static constexpr float       kAgeDecayFactorPerHour = 0.9f;

    // Internal helpers
    String generateId() noexcept;
    size_t findEntry(const String& id) const noexcept;
    void evictIfNeeded() noexcept;
    void sortByEvictionPriority() noexcept;
    MemoryEntry parseEntry(const String& json, size_t& pos) const noexcept;
    String unescapeJson(const String& escaped, size_t& pos) const noexcept;

    // Internal helpers for new features
    float computeTagSimilarity(const String& tags1, const String& tags2) const noexcept;
    float computeKeySimilarity(const String& key1, const String& key2) const noexcept;
    size_t fuzzyFindKey(const String& keyPattern, float threshold = 0.85f) const noexcept;
    void saveConversations() noexcept;
    void loadConversations() noexcept;
    void saveRevisions() noexcept;
    void loadRevisions() noexcept;
    size_t findRevision(const String& revisionId) const noexcept;

    // Member variables
    bool                    m_initialized;
    bool                    m_dirty;
    std::vector<MemoryEntry> m_entries;
    std::vector<MemoryRevision> m_revisions;
    std::vector<ConversationRecord> m_conversations;
    size_t                  m_maxEntries;
    unsigned long           m_lastIdCounter;
    unsigned long           m_lastMaintenance;
    unsigned long           m_lastAgeRun;
};

/**
 * @brief Global memory manager instance
 */
extern MemoryManager memoryManager;

#endif // AURA_MEMORY_MANAGER_H