#ifndef AURA_STUDY_MANAGER_H
#define AURA_STUDY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"
#include "storage_manager.h"

struct StudySubject {
    String id;
    String name;
    String description;
    String tags;
    uint8_t masteryLevel;        // 0-100
    uint32_t totalSessions;
    uint32_t totalMinutes;
    unsigned long lastStudied;
    unsigned long createdAt;
    uint8_t streakDays;

    StudySubject() noexcept : masteryLevel(0), totalSessions(0), totalMinutes(0), lastStudied(0), createdAt(0), streakDays(0) {}
};

struct StudySession {
    String id;
    String subjectId;
    unsigned long startTime;
    unsigned long durationMinutes;
    uint8_t performance;        // 0-100
    String notes;
    String topicsCovered;

    StudySession() noexcept : startTime(0), durationMinutes(0), performance(0) {}
};

struct FlashCard {
    String id;
    String subjectId;
    String question;
    String answer;
    uint8_t difficulty;         // 1-5
    uint8_t timesReviewed;
    uint8_t correctCount;
    uint8_t incorrectCount;
    unsigned long lastReviewed;
    float easeFactor;           // For spaced repetition

    FlashCard() noexcept : difficulty(3), timesReviewed(0), correctCount(0), incorrectCount(0), lastReviewed(0), easeFactor(2.5f) {}
};

class StudyManager {
public:
    StudyManager() noexcept;
    ~StudyManager() noexcept;

    StudyManager(const StudyManager&) = delete;
    StudyManager& operator=(const StudyManager&) = delete;
    StudyManager(StudyManager&&) = delete;
    StudyManager& operator=(StudyManager&&) = delete;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    // Subjects
    [[nodiscard]] String addSubject(const StudySubject& subject) noexcept;
    [[nodiscard]] bool removeSubject(const String& subjectId) noexcept;
    [[nodiscard]] bool updateSubject(const String& subjectId, const StudySubject& updates) noexcept;
    [[nodiscard]] StudySubject getSubject(const String& subjectId) const noexcept;
    [[nodiscard]] std::vector<StudySubject> getAllSubjects() const noexcept;
    [[nodiscard]] std::vector<StudySubject> getDueSubjects() const noexcept;

    // Sessions
    [[nodiscard]] String startSession(const String& subjectId) noexcept;
    [[nodiscard]] bool endSession(const String& sessionId, uint8_t performance, const String& notes = "", const String& topics = "") noexcept;
    [[nodiscard]] std::vector<StudySession> getSessions(const String& subjectId) const noexcept;
    [[nodiscard]] std::vector<StudySession> getRecentSessions(unsigned long since) const noexcept;

    // Flashcards
    [[nodiscard]] String addFlashCard(const String& subjectId, const String& question, const String& answer, uint8_t difficulty = 3) noexcept;
    [[nodiscard]] bool removeFlashCard(const String& cardId) noexcept;
    [[nodiscard]] bool reviewFlashCard(const String& cardId, bool correct) noexcept;
    [[nodiscard]] std::vector<FlashCard> getDueFlashCards(const String& subjectId, size_t maxCards = 10) const noexcept;
    [[nodiscard]] std::vector<FlashCard> getAllFlashCards(const String& subjectId) const noexcept;

    // Statistics
    [[nodiscard]] size_t totalStudyMinutes() const noexcept;
    [[nodiscard]] size_t totalFlashCards() const noexcept;
    [[nodiscard]] float averagePerformance() const noexcept;

    [[nodiscard]] bool save() noexcept;
    [[nodiscard]] bool load() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "StudyManager";
    static constexpr const char* kStorageSubjectsPath = "/study_subjects.json";
    static constexpr const char* kStorageSessionsPath = "/study_sessions.json";
    static constexpr const char* kStorageFlashCardsPath = "/study_flashcards.json";
    static constexpr size_t kMaxSubjects = 32;
    static constexpr size_t kMaxSessions = 512;
    static constexpr size_t kMaxFlashCards = 256;

    String generateId() noexcept;
    size_t findSubject(const String& id) const noexcept;
    size_t findSession(const String& id) const noexcept;
    size_t findFlashCard(const String& id) const noexcept;
    unsigned long getNextReview(const FlashCard& card) const noexcept;

    bool m_initialized;
    std::vector<StudySubject> m_subjects;
    std::vector<StudySession> m_sessions;
    std::vector<FlashCard> m_flashcards;
    unsigned long m_lastIdCounter;
};

extern StudyManager studyManager;

#endif // AURA_STUDY_MANAGER_H
