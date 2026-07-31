#include "personality_manager.h"
#include "json_helpers.h"

PersonalityManager personalityManager;

PersonalityManager::PersonalityManager() noexcept
    : m_initialized(false), m_dirty(false), m_activeProfileIndex(0) {
    m_profiles.reserve(kMaxProfiles);
}

PersonalityManager::~PersonalityManager() noexcept {
    if (m_dirty) saveProfileState();
}

bool PersonalityManager::initialize() noexcept {
    if (m_initialized) {
        Logger::warning(kLogCategory, "Already initialized");
        return true;
    }

    loadBuiltinProfiles();

    if (!loadProfileState()) {
        m_activeProfileIndex = 0;
        m_profiles[0].enabled = true;
    }

    m_initialized = true;
    Logger::info(kLogCategory, "Initialized (%u profiles, active: %s)",
        m_profiles.size(), m_profiles[m_activeProfileIndex].name.c_str());
    return true;
}

void PersonalityManager::update() noexcept {
    if (!m_initialized) return;

    static unsigned long lastSave = 0;
    unsigned long now = millis();

    if (m_dirty && (now - lastSave > 5000)) {
        lastSave = now;
        if (saveProfileState()) m_dirty = false;
    }
}

bool PersonalityManager::activateProfile(const String& profileId) noexcept {
    size_t idx = findProfile(profileId);
    if (idx == SIZE_MAX) {
        Logger::warning(kLogCategory, "Profile '%s' not found", profileId.c_str());
        return false;
    }

    if (idx != m_activeProfileIndex) {
        m_activeProfileIndex = idx;
        m_dirty = true;
        Logger::info(kLogCategory, "Activated profile: %s", m_profiles[idx].name.c_str());
    }
    return true;
}

const PersonalityProfile& PersonalityManager::getActiveProfile() const noexcept {
    return m_profiles[m_activeProfileIndex];
}

PersonalityProfile PersonalityManager::getProfile(const String& profileId) const noexcept {
    size_t idx = findProfile(profileId);
    if (idx == SIZE_MAX) return PersonalityProfile();
    return m_profiles[idx];
}

const std::vector<PersonalityProfile>& PersonalityManager::getAllProfiles() const noexcept {
    return m_profiles;
}

bool PersonalityManager::addProfile(const PersonalityProfile& profile) noexcept {
    if (m_profiles.size() >= kMaxProfiles) return false;
    m_profiles.push_back(profile);
    m_dirty = true;
    Logger::info(kLogCategory, "Profile '%s' added", profile.name.c_str());
    return true;
}

bool PersonalityManager::removeProfile(const String& profileId) noexcept {
    if (profileId == "jarvis" || profileId == "professional" ||
        profileId == "teacher" || profileId == "programmer" ||
        profileId == "friendly" || profileId == "minimal") {
        Logger::warning(kLogCategory, "Cannot remove built-in profile '%s'", profileId.c_str());
        return false;
    }

    size_t idx = findProfile(profileId);
    if (idx == SIZE_MAX) return false;

    m_profiles.erase(m_profiles.begin() + static_cast<ptrdiff_t>(idx));
    if (m_activeProfileIndex >= m_profiles.size()) m_activeProfileIndex = 0;
    m_dirty = true;
    return true;
}

String PersonalityManager::getActiveProfileId() const noexcept {
    if (m_profiles.empty()) return "";
    return m_profiles[m_activeProfileIndex].id;
}

bool PersonalityManager::isInitialized() const noexcept {
    return m_initialized;
}

void PersonalityManager::loadBuiltinProfiles() noexcept {
    m_profiles.clear();

    // Jarvis
    {
        PersonalityProfile p;
        p.id = "jarvis"; p.name = "Jarvis";
        p.systemPrompt = "You are Jarvis, an AI desktop assistant. You are helpful, efficient, and have a British-inspired formal yet friendly tone. You are proactive and anticipate needs.";
        p.voice = "en-GB-Neural2-B"; p.ledTheme = 0x00BFFF;
        p.startupSound = "jarvis_startup"; p.responseStyle = "formal-friendly";
        p.enabled = true;
        m_profiles.push_back(p);
    }

    // Professional
    {
        PersonalityProfile p;
        p.id = "professional"; p.name = "Professional";
        p.systemPrompt = "You are a professional AI assistant. You communicate clearly, concisely, and with strict professionalism. You prioritize accuracy and efficiency in all responses.";
        p.voice = "en-US-Neural2-D"; p.ledTheme = 0xFFFFFF;
        p.startupSound = "professional_startup"; p.responseStyle = "professional";
        p.enabled = true;
        m_profiles.push_back(p);
    }

    // Teacher
    {
        PersonalityProfile p;
        p.id = "teacher"; p.name = "Teacher";
        p.systemPrompt = "You are a patient and knowledgeable teacher. You explain concepts thoroughly, provide examples, and encourage learning. You break down complex topics into simple steps.";
        p.voice = "en-US-Neural2-A"; p.ledTheme = 0xFFD700;
        p.startupSound = "teacher_startup"; p.responseStyle = "educational";
        p.enabled = true;
        m_profiles.push_back(p);
    }

    // Programmer
    {
        PersonalityProfile p;
        p.id = "programmer"; p.name = "Programmer";
        p.systemPrompt = "You are a skilled programmer assistant. You provide code examples, debugging help, and technical explanations. You are precise and detail-oriented.";
        p.voice = "en-US-Neural2-I"; p.ledTheme = 0x00FF00;
        p.startupSound = "programmer_startup"; p.responseStyle = "technical";
        p.enabled = true;
        m_profiles.push_back(p);
    }

    // Friendly
    {
        PersonalityProfile p;
        p.id = "friendly"; p.name = "Friendly";
        p.systemPrompt = "You are a warm and friendly AI companion. You are casual, empathetic, and cheerful. You use everyday language and make conversations feel natural.";
        p.voice = "en-US-Neural2-F"; p.ledTheme = 0xFF69B4;
        p.startupSound = "friendly_startup"; p.responseStyle = "casual-warm";
        p.enabled = true;
        m_profiles.push_back(p);
    }

    // Minimal
    {
        PersonalityProfile p;
        p.id = "minimal"; p.name = "Minimal";
        p.systemPrompt = "You are a minimalist AI assistant. You give the shortest possible answers that are still correct. No small talk, no extra words, just the answer.";
        p.voice = "en-US-Neural2-J"; p.ledTheme = 0x808080;
        p.startupSound = "minimal_startup"; p.responseStyle = "minimal";
        p.enabled = true;
        m_profiles.push_back(p);
    }
}

bool PersonalityManager::save() noexcept {
    return saveProfileState();
}

bool PersonalityManager::saveProfileState() noexcept {
    String json;
    json.reserve(1024);
    json += "{\"active\":\"";
    json += escapeJson(m_profiles[m_activeProfileIndex].id);
    json += "\",\"voiceTone\":\"";
    json += escapeJson(m_voiceTone);
    json += "\",\"speechRate\":\"";
    json += escapeJson(m_speechRate);
    json += "\",\"voiceGender\":\"";
    json += escapeJson(m_voiceGender);
    json += "\"}";

    StorageStatus status = storageManager.writeFile(kStoragePath, json, StorageType::SPIFFS);
    if (status == StorageStatus::SUCCESS) {
        m_dirty = false;
        return true;
    }
    return false;
}

bool PersonalityManager::loadProfileState() noexcept {
    String content;
    StorageStatus status = storageManager.readFile(kStoragePath, content, StorageType::SPIFFS);
    if (status != StorageStatus::SUCCESS || content.isEmpty()) return false;

    String search = "\"active\":\"";
    int start = content.indexOf(search);
    if (start < 0) return false;
    start += search.length();
    int end = content.indexOf('"', start);
    if (end < 0) return false;

    String activeId = content.substring(start, end);
    size_t idx = findProfile(activeId);
    if (idx != SIZE_MAX) {
        m_activeProfileIndex = idx;
    }

    auto extractField = [&](const char* key) -> String {
        String q = String("\"") + key + "\":\"";
        int s = content.indexOf(q);
        if (s < 0) return "";
        s += q.length();
        int e = content.indexOf('"', s);
        return (e < 0) ? "" : content.substring(s, e);
    };

    m_voiceTone = extractField("voiceTone");
    m_speechRate = extractField("speechRate");
    m_voiceGender = extractField("voiceGender");

    return true;
}

size_t PersonalityManager::findProfile(const String& id) const noexcept {
    for (size_t i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == id) return i;
    }
    return SIZE_MAX;
}

bool PersonalityManager::setVoiceTone(const String& toneId) noexcept {
    std::vector<String> tones = getAvailableTones();
    bool valid = false;
    for (const auto& t : tones) { if (t == toneId) { valid = true; break; } }
    if (!valid) return false;
    m_voiceTone = toneId;
    m_dirty = true;
    return true;
}

String PersonalityManager::getVoiceTone() const noexcept {
    return m_voiceTone.isEmpty() ? "neutral" : m_voiceTone;
}

bool PersonalityManager::setSpeechRate(const String& rate) noexcept {
    if (rate != "slow" && rate != "normal" && rate != "fast") return false;
    m_speechRate = rate;
    m_dirty = true;
    return true;
}

String PersonalityManager::getSpeechRate() const noexcept {
    return m_speechRate.isEmpty() ? "normal" : m_speechRate;
}

bool PersonalityManager::setVoiceGender(const String& gender) noexcept {
    if (gender != "male" && gender != "female" && gender != "neutral") return false;
    m_voiceGender = gender;
    m_dirty = true;
    return true;
}

String PersonalityManager::getVoiceGender() const noexcept {
    return m_voiceGender.isEmpty() ? "neutral" : m_voiceGender;
}

String PersonalityManager::applyVoiceModulation(const String& text) const noexcept {
    String result = text;
    result = applyTone(result);
    result = applyRate(result);
    return result;
}

std::vector<String> PersonalityManager::getAvailableTones() const noexcept {
    std::vector<String> tones;
    tones.reserve(kMaxTones);
    tones.push_back("neutral");
    tones.push_back("cheerful");
    tones.push_back("formal");
    tones.push_back("friendly");
    tones.push_back("professional");
    tones.push_back("casual");
    tones.push_back("empathetic");
    tones.push_back("humorous");
    return tones;
}

String PersonalityManager::applyTone(const String& text) const noexcept {
    String tone = m_voiceTone.isEmpty() ? "neutral" : m_voiceTone;
    if (tone == "cheerful") {
        String t = text;
        if (!t.isEmpty() && t[t.length() - 1] != '!' && t[t.length() - 1] != '?') t += "!";
        return t;
    }
    return text;
}

String PersonalityManager::applyRate(const String& text) const noexcept {
    return text;
}
