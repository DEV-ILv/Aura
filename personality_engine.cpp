#include "personality_engine.h"

#include <cstring>
#include "personality_manager.h"
#include "logger.h"

PersonalityEngine personalityEngine;

namespace {
void copyShort(char* dst, size_t cap, const String& src) noexcept {
    if (dst == nullptr || cap == 0) return;
    const char* s = src.c_str();
    strncpy(dst, s ? s : "", cap - 1);
    dst[cap - 1] = '\0';
}
}  // namespace

PersonalityEngine::PersonalityEngine() noexcept
    : m_active(false),
      m_knobs{VocabStyle::NEUTRAL, 1, 1, 1, true} {
    m_profileId[0] = '\0';
    m_profileName[0] = '\0';
    m_styleName[0] = '\0';
}

PersonalityEngine::~PersonalityEngine() noexcept {}

void PersonalityEngine::refresh() noexcept {
    if (!personalityManager.isInitialized()) {
        m_active = false;
        copyShort(m_profileId, sizeof(m_profileId), "jarvis");
        copyShort(m_profileName, sizeof(m_profileName), "Jarvis");
        copyShort(m_styleName, sizeof(m_styleName), "formal-friendly");
        m_knobs = PersonalityKnobs{VocabStyle::NEUTRAL, 1, 1, 1, true};
        return;
    }

    const PersonalityProfile& profile = personalityManager.getActiveProfile();
    m_active = true;
    copyShort(m_profileId, sizeof(m_profileId), profile.id);
    copyShort(m_profileName, sizeof(m_profileName), profile.name);
    copyShort(m_styleName, sizeof(m_styleName), profile.responseStyle);
    resolveKnobs(profile.id, profile.responseStyle);
}

void PersonalityEngine::resolveKnobs(const String& profileId,
                                     const String& responseStyle) noexcept {
    // Default: neutral, balanced.
    PersonalityKnobs k{VocabStyle::NEUTRAL, 1, 1, 1, true};
    const char* id = profileId.c_str();

    if (strcmp(id, "jarvis") == 0 || responseStyle == "formal-friendly") {
        k = {VocabStyle::NEUTRAL, 1, 1, 2, true};
    } else if (strcmp(id, "professional") == 0 || responseStyle == "professional") {
        k = {VocabStyle::FORMAL, 0, 1, 2, true};
    } else if (strcmp(id, "teacher") == 0 || responseStyle == "educational") {
        k = {VocabStyle::NEUTRAL, 0, 2, 2, true};
    } else if (strcmp(id, "programmer") == 0 || responseStyle == "technical") {
        k = {VocabStyle::NEUTRAL, 0, 1, 2, true};
    } else if (strcmp(id, "friendly") == 0 || responseStyle == "casual-warm") {
        k = {VocabStyle::CASUAL, 2, 1, 1, true};
    } else if (strcmp(id, "minimal") == 0 || responseStyle == "minimal") {
        k = {VocabStyle::CASUAL, 0, 0, 1, false};
    }

    m_knobs = k;
}

bool PersonalityEngine::isActive() const noexcept {
    return m_active;
}

const char* PersonalityEngine::profileId() const noexcept {
    return m_profileId;
}

const char* PersonalityEngine::profileName() const noexcept {
    return m_profileName;
}

const char* PersonalityEngine::styleName() const noexcept {
    return m_styleName;
}

const PersonalityKnobs& PersonalityEngine::knobs() const noexcept {
    return m_knobs;
}

String PersonalityEngine::greeting() noexcept {
    return sentenceEngine.pickGreeting(m_knobs.style);
}

String PersonalityEngine::closing() noexcept {
    return sentenceEngine.pickClosing(m_knobs.style);
}

String PersonalityEngine::transition() noexcept {
    return sentenceEngine.pickTransition(m_knobs.style);
}

String PersonalityEngine::connective() noexcept {
    return sentenceEngine.pickConnector();
}

String PersonalityEngine::possessiveVerb() noexcept {
    return sentenceEngine.pickPossessiveVerb(m_knobs.style);
}

String PersonalityEngine::adjective() noexcept {
    return sentenceEngine.pickAdjective(m_knobs.style);
}

String PersonalityEngine::ending() noexcept {
    return sentenceEngine.pickEnding(m_knobs.style);
}

String PersonalityEngine::confidencePhrase(float confidence) noexcept {
    return sentenceEngine.pickConfidence(confidence);
}

String PersonalityEngine::synonym(const char* key) noexcept {
    return sentenceEngine.pickSynonym(key);
}

String PersonalityEngine::dataOpening(const char* noun, size_t count) noexcept {
    String opening = "You ";
    opening += possessiveVerb();
    opening += " ";
    opening += sentenceEngine.countPhrase(count, noun);
    return opening;
}
