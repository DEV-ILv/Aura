#include "study_manager.h"
#include "json_helpers.h"
#include "event_bus.h"

StudyManager studyManager;

StudyManager::StudyManager() noexcept : m_initialized(false), m_lastIdCounter(0) {
    m_subjects.reserve(kMaxSubjects);
    m_sessions.reserve(kMaxSessions);
    m_flashcards.reserve(kMaxFlashCards);
}

StudyManager::~StudyManager() noexcept {
    if (m_initialized) save();
}

bool StudyManager::initialize() noexcept {
    if (m_initialized) { Logger::warning(kLogCategory, "Already initialized"); return true; }
    if (!storageManager.isHealthy()) { Logger::error(kLogCategory, "Storage not healthy"); return false; }
    load();
    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u subjects, %u sessions, %u flashcards)",
        m_subjects.size(), m_sessions.size(), m_flashcards.size());
    return true;
}

void StudyManager::update() noexcept {
    if (!m_initialized) return;
    static unsigned long lastSave = 0;
    unsigned long now = millis();
    if ((now - lastSave > 5000) && save()) { lastSave = now; }
}

String StudyManager::addSubject(const StudySubject& subject) noexcept {
    if (!m_initialized || m_subjects.size() >= kMaxSubjects) return "";
    StudySubject s = subject;
    s.id = generateId();
    s.createdAt = millis();
    m_subjects.push_back(s);
    save();
    Logger::info(kLogCategory, "Subject '%s' added", s.name.c_str());
    return s.id;
}

bool StudyManager::removeSubject(const String& subjectId) noexcept {
    size_t idx = findSubject(subjectId);
    if (idx == SIZE_MAX) return false;
    m_subjects.erase(m_subjects.begin() + static_cast<ptrdiff_t>(idx));
    save();
    Logger::info(kLogCategory, "Subject '%s' removed", subjectId.c_str());
    return true;
}

bool StudyManager::updateSubject(const String& subjectId, const StudySubject& updates) noexcept {
    size_t idx = findSubject(subjectId);
    if (idx == SIZE_MAX) return false;
    StudySubject& s = m_subjects[idx];
    if (!updates.name.isEmpty()) s.name = updates.name;
    if (!updates.description.isEmpty()) s.description = updates.description;
    if (!updates.tags.isEmpty()) s.tags = updates.tags;
    s.masteryLevel = updates.masteryLevel;
    s.totalSessions = updates.totalSessions;
    s.totalMinutes = updates.totalMinutes;
    s.lastStudied = updates.lastStudied;
    s.streakDays = updates.streakDays;
    save();
    return true;
}

StudySubject StudyManager::getSubject(const String& subjectId) const noexcept {
    size_t idx = findSubject(subjectId);
    return (idx != SIZE_MAX) ? m_subjects[idx] : StudySubject();
}

std::vector<StudySubject> StudyManager::getAllSubjects() const noexcept {
    return m_subjects;
}

std::vector<StudySubject> StudyManager::getDueSubjects() const noexcept {
    std::vector<StudySubject> due;
    unsigned long now = millis();
    unsigned long sevenDaysMs = 7UL * 86400000UL;
    for (const auto& s : m_subjects) {
        if (s.masteryLevel < 50 || (now - s.lastStudied > sevenDaysMs)) {
            due.push_back(s);
        }
    }
    return due;
}

String StudyManager::startSession(const String& subjectId) noexcept {
    if (!m_initialized || m_sessions.size() >= kMaxSessions) return "";
    size_t idx = findSubject(subjectId);
    if (idx == SIZE_MAX) return "";
    StudySession ss;
    ss.id = generateId();
    ss.subjectId = subjectId;
    ss.startTime = millis();
    m_sessions.push_back(ss);
    save();
    Logger::info(kLogCategory, "Session '%s' started for subject '%s'", ss.id.c_str(), subjectId.c_str());
    return ss.id;
}

bool StudyManager::endSession(const String& sessionId, uint8_t performance, const String& notes, const String& topics) noexcept {
    size_t idx = findSession(sessionId);
    if (idx == SIZE_MAX) return false;
    StudySession& ss = m_sessions[idx];
    unsigned long now = millis();
    ss.durationMinutes = (now - ss.startTime) / 60000UL;
    if (ss.durationMinutes == 0) ss.durationMinutes = 1;
    ss.performance = performance;
    ss.notes = notes;
    ss.topicsCovered = topics;

    size_t subIdx = findSubject(ss.subjectId);
    if (subIdx != SIZE_MAX) {
        StudySubject& sub = m_subjects[subIdx];
        sub.totalSessions++;
        sub.totalMinutes += ss.durationMinutes;
        sub.lastStudied = now;
        sub.masteryLevel = (sub.masteryLevel + performance) / 2;
        if (sub.masteryLevel > 100) sub.masteryLevel = 100;
        sub.streakDays++;
    }
    save();
    Logger::info(kLogCategory, "Session '%s' ended (duration: %u min, perf: %u)", sessionId.c_str(), ss.durationMinutes, performance);
    if (eventBus.isInitialized()) {
        String data = "{\"sessionId\":\"" + sessionId + "\",\"subjectId\":\"" + ss.subjectId
            + "\",\"duration\":" + String(ss.durationMinutes)
            + ",\"performance\":" + String(performance) + "}";
        eventBus.publish(EventType::STUDY_SESSION_COMPLETED, "StudyManager", data);
    }
    return true;
}

std::vector<StudySession> StudyManager::getSessions(const String& subjectId) const noexcept {
    std::vector<StudySession> result;
    for (const auto& s : m_sessions) {
        if (s.subjectId == subjectId) result.push_back(s);
    }
    return result;
}

std::vector<StudySession> StudyManager::getRecentSessions(unsigned long since) const noexcept {
    std::vector<StudySession> result;
    for (const auto& s : m_sessions) {
        if (s.startTime >= since) result.push_back(s);
    }
    return result;
}

String StudyManager::addFlashCard(const String& subjectId, const String& question, const String& answer, uint8_t difficulty) noexcept {
    if (!m_initialized || m_flashcards.size() >= kMaxFlashCards) return "";
    if (findSubject(subjectId) == SIZE_MAX) return "";
    FlashCard fc;
    fc.id = generateId();
    fc.subjectId = subjectId;
    fc.question = question;
    fc.answer = answer;
    fc.difficulty = difficulty;
    m_flashcards.push_back(fc);
    save();
    Logger::info(kLogCategory, "FlashCard '%s' added", fc.id.c_str());
    return fc.id;
}

bool StudyManager::removeFlashCard(const String& cardId) noexcept {
    size_t idx = findFlashCard(cardId);
    if (idx == SIZE_MAX) return false;
    m_flashcards.erase(m_flashcards.begin() + static_cast<ptrdiff_t>(idx));
    save();
    return true;
}

bool StudyManager::reviewFlashCard(const String& cardId, bool correct) noexcept {
    size_t idx = findFlashCard(cardId);
    if (idx == SIZE_MAX) return false;
    FlashCard& fc = m_flashcards[idx];
    fc.timesReviewed++;
    fc.lastReviewed = millis();
    if (correct) {
        fc.correctCount++;
        fc.easeFactor += 0.1f;
        if (fc.easeFactor > 3.0f) fc.easeFactor = 3.0f;
    } else {
        fc.incorrectCount++;
        fc.easeFactor -= 0.2f;
        if (fc.easeFactor < 1.3f) fc.easeFactor = 1.3f;
        fc.timesReviewed = 0;
    }
    save();
    Logger::info(kLogCategory, "FlashCard '%s' reviewed (correct: %s, ease: %.1f)", cardId.c_str(), correct ? "yes" : "no", fc.easeFactor);
    return true;
}

std::vector<FlashCard> StudyManager::getDueFlashCards(const String& subjectId, size_t maxCards) const noexcept {
    std::vector<FlashCard> due;
    unsigned long now = millis();
    for (const auto& fc : m_flashcards) {
        if (fc.subjectId == subjectId && now >= getNextReview(fc)) {
            due.push_back(fc);
            if (due.size() >= maxCards) break;
        }
    }
    return due;
}

std::vector<FlashCard> StudyManager::getAllFlashCards(const String& subjectId) const noexcept {
    std::vector<FlashCard> result;
    for (const auto& fc : m_flashcards) {
        if (fc.subjectId == subjectId) result.push_back(fc);
    }
    return result;
}

size_t StudyManager::totalStudyMinutes() const noexcept {
    size_t total = 0;
    for (const auto& s : m_sessions) total += s.durationMinutes;
    return total;
}

size_t StudyManager::totalFlashCards() const noexcept {
    return m_flashcards.size();
}

float StudyManager::averagePerformance() const noexcept {
    if (m_sessions.empty()) return 0.0f;
    uint32_t sum = 0;
    for (const auto& s : m_sessions) sum += s.performance;
    return static_cast<float>(sum) / static_cast<float>(m_sessions.size());
}

bool StudyManager::isInitialized() const noexcept {
    return m_initialized;
}

bool StudyManager::save() noexcept {
    String json; json.reserve(4096);

    json += "{\"items\":[";
    for (size_t i = 0; i < m_subjects.size(); ++i) {
        if (i > 0) json += ",";
        const auto& s = m_subjects[i];
        json += "{\"id\":\"" + escapeJson(s.id) + "\",\"name\":\"" + escapeJson(s.name) + "\",";
        json += "\"desc\":\"" + escapeJson(s.description) + "\",\"tags\":\"" + escapeJson(s.tags) + "\",";
        json += "\"mastery\":" + String(s.masteryLevel) + ",";
        json += "\"sessions\":" + String(s.totalSessions) + ",";
        json += "\"minutes\":" + String(s.totalMinutes) + ",";
        json += "\"lastStudied\":" + String(s.lastStudied) + ",";
        json += "\"created\":" + String(s.createdAt) + ",";
        json += "\"streak\":" + String(s.streakDays) + "}";
    }
    json += "]}";
    if (storageManager.writeFile(kStorageSubjectsPath, json, StorageType::SPIFFS) != StorageStatus::SUCCESS) return false;

    json = "{\"items\":[";
    for (size_t i = 0; i < m_sessions.size(); ++i) {
        if (i > 0) json += ",";
        const auto& s = m_sessions[i];
        json += "{\"id\":\"" + escapeJson(s.id) + "\",\"subj\":\"" + escapeJson(s.subjectId) + "\",";
        json += "\"start\":" + String(s.startTime) + ",\"dur\":" + String(s.durationMinutes) + ",";
        json += "\"perf\":" + String(s.performance) + ",";
        json += "\"notes\":\"" + escapeJson(s.notes) + "\",";
        json += "\"topics\":\"" + escapeJson(s.topicsCovered) + "\"}";
    }
    json += "]}";
    if (storageManager.writeFile(kStorageSessionsPath, json, StorageType::SPIFFS) != StorageStatus::SUCCESS) return false;

    json = "{\"items\":[";
    for (size_t i = 0; i < m_flashcards.size(); ++i) {
        if (i > 0) json += ",";
        const auto& c = m_flashcards[i];
        json += "{\"id\":\"" + escapeJson(c.id) + "\",\"subj\":\"" + escapeJson(c.subjectId) + "\",";
        json += "\"q\":\"" + escapeJson(c.question) + "\",\"a\":\"" + escapeJson(c.answer) + "\",";
        json += "\"diff\":" + String(c.difficulty) + ",\"rev\":" + String(c.timesReviewed) + ",";
        json += "\"cor\":" + String(c.correctCount) + ",\"inc\":" + String(c.incorrectCount) + ",";
        json += "\"last\":" + String(c.lastReviewed) + ",\"ease\":" + String(c.easeFactor, 1) + "}";
    }
    json += "]}";
    if (storageManager.writeFile(kStorageFlashCardsPath, json, StorageType::SPIFFS) != StorageStatus::SUCCESS) return false;

    return true;
}

bool StudyManager::load() noexcept {
    String content;
    auto parseSubject = [&](const String& obj, StudySubject& s) {
        auto ext = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\"";
            int st = obj.indexOf(q);
            if (st < 0) {
                q = String("\"") + k + "\":";
                st = obj.indexOf(q);
                if (st < 0) return "";
                st += q.length();
                int en = st;
                while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}') en++;
                return obj.substring(st, en);
            }
            st += q.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        s.id = ext("id"); s.name = ext("name"); s.description = ext("desc"); s.tags = ext("tags");
        s.masteryLevel = ext("mastery").toInt();
        s.totalSessions = ext("sessions").toInt();
        s.totalMinutes = ext("minutes").toInt();
        s.lastStudied = ext("lastStudied").toInt();
        s.createdAt = ext("created").toInt();
        s.streakDays = ext("streak").toInt();
    };

    if (storageManager.fileExists(kStorageSubjectsPath, StorageType::SPIFFS)) {
        content = "";
        if (storageManager.readFile(kStorageSubjectsPath, content, StorageType::SPIFFS) == StorageStatus::SUCCESS && !content.isEmpty()) {
            m_subjects.clear();
            int pos = 0;
            while (true) {
                int s = content.indexOf('{', pos); if (s < 0) break;
                int e = content.indexOf('}', s); if (e < 0) break;
                String obj = content.substring(s, e + 1);
                StudySubject sub; parseSubject(obj, sub);
                if (!sub.id.isEmpty()) m_subjects.push_back(sub);
                pos = e + 1;
            }
        }
    }

    auto parseSession = [&](const String& obj, StudySession& s) {
        auto ext = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\"";
            int st = obj.indexOf(q);
            if (st < 0) {
                q = String("\"") + k + "\":";
                st = obj.indexOf(q);
                if (st < 0) return "";
                st += q.length();
                int en = st;
                while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}') en++;
                return obj.substring(st, en);
            }
            st += q.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        s.id = ext("id"); s.subjectId = ext("subj");
        s.startTime = ext("start").toInt();
        s.durationMinutes = ext("dur").toInt();
        s.performance = ext("perf").toInt();
        s.notes = ext("notes"); s.topicsCovered = ext("topics");
    };

    if (storageManager.fileExists(kStorageSessionsPath, StorageType::SPIFFS)) {
        content = "";
        if (storageManager.readFile(kStorageSessionsPath, content, StorageType::SPIFFS) == StorageStatus::SUCCESS && !content.isEmpty()) {
            m_sessions.clear();
            int pos = 0;
            while (true) {
                int s = content.indexOf('{', pos); if (s < 0) break;
                int e = content.indexOf('}', s); if (e < 0) break;
                String obj = content.substring(s, e + 1);
                StudySession ss; parseSession(obj, ss);
                if (!ss.id.isEmpty()) m_sessions.push_back(ss);
                pos = e + 1;
            }
        }
    }

    auto parseFlashCard = [&](const String& obj, FlashCard& c) {
        auto ext = [&](const char* k) -> String {
            String q = String("\"") + k + "\":\"";
            int st = obj.indexOf(q);
            if (st < 0) {
                q = String("\"") + k + "\":";
                st = obj.indexOf(q);
                if (st < 0) return "";
                st += q.length();
                int en = st;
                while (en < (int)obj.length() && obj[en] != ',' && obj[en] != '}') en++;
                return obj.substring(st, en);
            }
            st += q.length();
            int en = obj.indexOf('"', st);
            return (en < 0) ? "" : obj.substring(st, en);
        };
        c.id = ext("id"); c.subjectId = ext("subj"); c.question = ext("q"); c.answer = ext("a");
        c.difficulty = ext("diff").toInt();
        c.timesReviewed = ext("rev").toInt();
        c.correctCount = ext("cor").toInt();
        c.incorrectCount = ext("inc").toInt();
        c.lastReviewed = ext("last").toInt();
        c.easeFactor = ext("ease").toFloat();
    };

    if (storageManager.fileExists(kStorageFlashCardsPath, StorageType::SPIFFS)) {
        content = "";
        if (storageManager.readFile(kStorageFlashCardsPath, content, StorageType::SPIFFS) == StorageStatus::SUCCESS && !content.isEmpty()) {
            m_flashcards.clear();
            int pos = 0;
            while (true) {
                int s = content.indexOf('{', pos); if (s < 0) break;
                int e = content.indexOf('}', s); if (e < 0) break;
                String obj = content.substring(s, e + 1);
                FlashCard fc; parseFlashCard(obj, fc);
                if (!fc.id.isEmpty()) m_flashcards.push_back(fc);
                pos = e + 1;
            }
        }
    }

    return true;
}

String StudyManager::generateId() noexcept {
    unsigned long now = millis(); m_lastIdCounter++;
    uint32_t mix = static_cast<uint32_t>(now) ^ (m_lastIdCounter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) { id += hex[val & 0x0F]; val = (val >> 2) ^ (val << 3) ^ (m_lastIdCounter + i); }
    return id;
}

size_t StudyManager::findSubject(const String& id) const noexcept {
    for (size_t i = 0; i < m_subjects.size(); ++i) { if (m_subjects[i].id == id) return i; }
    return SIZE_MAX;
}

size_t StudyManager::findSession(const String& id) const noexcept {
    for (size_t i = 0; i < m_sessions.size(); ++i) { if (m_sessions[i].id == id) return i; }
    return SIZE_MAX;
}

size_t StudyManager::findFlashCard(const String& id) const noexcept {
    for (size_t i = 0; i < m_flashcards.size(); ++i) { if (m_flashcards[i].id == id) return i; }
    return SIZE_MAX;
}

unsigned long StudyManager::getNextReview(const FlashCard& card) const noexcept {
    if (card.timesReviewed == 0) return 0;
    unsigned long intervalMs = static_cast<unsigned long>(86400000.0f * powf(card.easeFactor, static_cast<float>(card.timesReviewed)));
    unsigned long maxInterval = 30UL * 86400000UL;
    if (intervalMs > maxInterval) intervalMs = maxInterval;
    return card.lastReviewed + intervalMs;
}
