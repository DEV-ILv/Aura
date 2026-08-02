#include "sentence_generation_engine.h"

SentenceGenerationEngine sentenceEngine;

SentenceGenerationEngine::SentenceGenerationEngine() noexcept
    : m_lastGreeting(0), m_lastClosing(0),
      m_lastTransition(0), m_lastConnector(0) {}

SentenceGenerationEngine::~SentenceGenerationEngine() noexcept {}

// ============================================================================
// Flash-resident fragment pools (stored in .rodata, no RAM cost)
// ============================================================================

const char* const SentenceGenerationEngine::kGreetings[3][5] = {
    // CASUAL
    {
        "Hey! I'm AURA, ready when you are",
        "Hi! AURA here, what can I do for you",
        "Hey there! I'm all ears",
        "Hi! Good to see you, what's up",
        "Hey! I'm here and ready to go"
    },
    // NEUTRAL
    {
        "Hello! I'm AURA, how can I help you today",
        "Hi there! I'm ready to assist you",
        "Hello! AURA here, what would you like to do",
        "Hi! I'm AURA, how can I be useful",
        "Hello! I'm at your service"
    },
    // FORMAL
    {
        "Good day, I am AURA, at your service",
        "Greetings, I am AURA, your personal assistant",
        "Hello, I am AURA. How may I assist you",
        "Welcome, I am AURA, ready to assist",
        "Good day, this is AURA. How may I help"
    }
};

const char* const SentenceGenerationEngine::kClosings[3][5] = {
    // CASUAL
    {
        "Let me know if you need anything else",
        "Happy to help, just say the word",
        "Anything else you want to tackle",
        "I'm around if you need me",
        "Want me to keep going with this"
    },
    // NEUTRAL
    {
        "Let me know if you need anything else",
        "Is there anything else I can do for you",
        "Feel free to ask if you need more",
        "I'm here if you need me",
        "Shall I continue with something else"
    },
    // FORMAL
    {
        "Please let me know if there is anything else I can assist with",
        "Should you require further assistance, I remain at your disposal",
        "Kindly let me know if you need anything further",
        "I am at your service should you need anything more",
        "Is there anything further you require"
    }
};

const char* const SentenceGenerationEngine::kTransitions[3][5] = {
    // CASUAL
    {
        "By the way",
        "On a side note",
        "Also",
        "And hey",
        "Quick note"
    },
    // NEUTRAL
    {
        "By the way",
        "Additionally",
        "Also",
        "On that note",
        "Incidentally"
    },
    // FORMAL
    {
        "Furthermore",
        "In addition",
        "Moreover",
        "Additionally",
        "Permit me to add"
    }
};

const char* const SentenceGenerationEngine::kVerbs[3][4] = {
    // CASUAL
    { "have", "have got", "have on your plate", "are holding" },
    // NEUTRAL
    { "have", "have listed", "currently have", "have in place" },
    // FORMAL
    { "have", "currently have", "have registered", "have in place" }
};

const char* const SentenceGenerationEngine::kAdjectives[3][6] = {
    // CASUAL
    { "solid", "nice", "great", "good", "pretty impressive", "strong" },
    // NEUTRAL
    { "solid", "good", "consistent", "notable", "respectable", "steady" },
    // FORMAL
    { "commendable", "consistent", "notable", "steady", "well-maintained", "creditable" }
};

const char* const SentenceGenerationEngine::kEndings[3][5] = {
    // CASUAL
    { "today", "right now", "for you", "at the moment", "at this point" },
    // NEUTRAL
    { "today", "at the moment", "right now", "currently", "for you" },
    // FORMAL
    { "today", "at present", "currently", "at this time", "for you" }
};

const char* const SentenceGenerationEngine::kConnectors[6] = {
    "and", "plus", "also", "on top of that", "what's more", "besides that"
};

const char* const SentenceGenerationEngine::kConfidenceHigh[3] = {
    "I'm fairly confident", "I'm quite sure", "It looks fairly certain"
};
const char* const SentenceGenerationEngine::kConfidenceMed[3] = {
    "It looks like", "From what I can tell", "I'd say"
};
const char* const SentenceGenerationEngine::kConfidenceLow[3] = {
    "I'm not entirely sure, but", "It's hard to say, though", "This is a rough estimate"
};

const char* const SentenceGenerationEngine::kNumberWords[21] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
    "sixteen", "seventeen", "eighteen", "nineteen", "twenty"
};

const char* const SentenceGenerationEngine::kSynonyms[6][4] = {
    // reminder
    { "reminder", "nudge", "callout", "heads-up" },
    // task
    { "task", "item", "to-do", "action" },
    // goal
    { "goal", "objective", "target", "milestone" },
    // memory
    { "memory", "note", "record", "entry" },
    // habit
    { "habit", "routine", "practice", "habit" },
    // subject
    { "subject", "topic", "course", "area" }
};

// ============================================================================
// Selection
// ============================================================================

const char* SentenceGenerationEngine::pickFrom(const char* const* pool, size_t count) noexcept {
    if (count == 0 || pool == nullptr) return "";
    size_t idx = static_cast<size_t>(random(static_cast<long>(count)));
    return pool[idx];
}

const char* SentenceGenerationEngine::pickGreeting(VocabStyle style) noexcept {
    const size_t styleIdx = static_cast<size_t>(style) % 3;
    size_t idx = static_cast<size_t>(random(5));
    if (idx == m_lastGreeting) idx = (idx + 1 + static_cast<size_t>(random(4))) % 5;
    m_lastGreeting = static_cast<uint8_t>(idx);
    return kGreetings[styleIdx][idx];
}

const char* SentenceGenerationEngine::pickClosing(VocabStyle style) noexcept {
    const size_t styleIdx = static_cast<size_t>(style) % 3;
    size_t idx = static_cast<size_t>(random(5));
    if (idx == m_lastClosing) idx = (idx + 1 + static_cast<size_t>(random(4))) % 5;
    m_lastClosing = static_cast<uint8_t>(idx);
    return kClosings[styleIdx][idx];
}

const char* SentenceGenerationEngine::pickTransition(VocabStyle style) noexcept {
    const size_t styleIdx = static_cast<size_t>(style) % 3;
    size_t idx = static_cast<size_t>(random(5));
    if (idx == m_lastTransition) idx = (idx + 1 + static_cast<size_t>(random(4))) % 5;
    m_lastTransition = static_cast<uint8_t>(idx);
    return kTransitions[styleIdx][idx];
}

const char* SentenceGenerationEngine::pickConnector() noexcept {
    size_t idx = static_cast<size_t>(random(6));
    if (idx == m_lastConnector) idx = (idx + 1 + static_cast<size_t>(random(5))) % 6;
    m_lastConnector = static_cast<uint8_t>(idx);
    return kConnectors[idx];
}

const char* SentenceGenerationEngine::pickPossessiveVerb(VocabStyle style) noexcept {
    const size_t styleIdx = static_cast<size_t>(style) % 3;
    return kVerbs[styleIdx][static_cast<size_t>(random(4))];
}

const char* SentenceGenerationEngine::pickAdjective(VocabStyle style) noexcept {
    const size_t styleIdx = static_cast<size_t>(style) % 3;
    return kAdjectives[styleIdx][static_cast<size_t>(random(6))];
}

const char* SentenceGenerationEngine::pickEnding(VocabStyle style) noexcept {
    const size_t styleIdx = static_cast<size_t>(style) % 3;
    return kEndings[styleIdx][static_cast<size_t>(random(5))];
}

const char* SentenceGenerationEngine::pickConfidence(float confidence) noexcept {
    if (confidence >= 0.66f) return pickFrom(kConfidenceHigh, 3);
    if (confidence >= 0.33f) return pickFrom(kConfidenceMed, 3);
    return pickFrom(kConfidenceLow, 3);
}

const char* SentenceGenerationEngine::pickSynonym(const char* key) noexcept {
    static const struct { const char* key; size_t index; } kMap[] = {
        { "reminder", 0 }, { "task", 1 }, { "goal", 2 },
        { "memory", 3 }, { "habit", 4 }, { "subject", 5 }
    };
    for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i) {
        if (strcmp(key, kMap[i].key) == 0) {
            return kSynonyms[kMap[i].index][static_cast<size_t>(random(4))];
        }
    }
    return key;
}

// ============================================================================
// Composition
// ============================================================================

String SentenceGenerationEngine::numberWord(size_t n) const noexcept {
    if (n <= 20) return kNumberWords[n];
    return String(static_cast<unsigned long>(n));
}

String SentenceGenerationEngine::countPhrase(size_t n, const char* noun) const noexcept {
    String phrase = numberWord(n);
    phrase += " ";
    phrase += noun;
    if (n != 1) phrase += "s";
    return phrase;
}

String SentenceGenerationEngine::join(const std::vector<String>& items,
                                      const char* separator,
                                      size_t maxItems) const noexcept {
    String result;
    size_t limit = (maxItems == 0) ? items.size() : (items.size() < maxItems ? items.size() : maxItems);
    for (size_t i = 0; i < limit; ++i) {
        if (i > 0) result += separator;
        result += items[i];
    }
    return result;
}

String SentenceGenerationEngine::listItems(const std::vector<String>& items,
                                           size_t maxItems) const noexcept {
    if (items.empty()) return "";
    size_t limit = (maxItems == 0) ? items.size() : (items.size() < maxItems ? items.size() : maxItems);
    String result;
    for (size_t i = 0; i < limit; ++i) {
        if (i > 0 && i + 1 == limit) {
            result += " and ";
        } else if (i > 0) {
            result += ", ";
        }
        result += items[i];
    }
    if (limit < items.size()) {
        result += " (and " + numberWord(items.size() - limit) + " more)";
    }
    return result;
}

String SentenceGenerationEngine::capitalise(const String& s) const noexcept {
    if (s.isEmpty()) return s;
    String out = s;
    out[0] = static_cast<char>(toupper(static_cast<unsigned char>(out[0])));
    return out;
}

String SentenceGenerationEngine::acknowledgement(const String& noun, VocabStyle style) noexcept {
    String ack;
    ack.reserve(48);
    ack += pickPossessiveVerb(style);
    ack += " a ";
    ack += pickAdjective(style);
    ack += " ";
    ack += pickSynonym(noun.c_str());
    return ack;
}
