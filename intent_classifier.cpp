#include "intent_classifier.h"

static constexpr const char* kGreetingKW[]       = {"hello","hi","hey","good morning","good evening","good afternoon","what's up"};
static constexpr const char* kSmallTalkKW[]      = {"how are you","how's it going","how do you feel","what's your name","who are you","nice to meet"};
static constexpr const char* kCapabilitiesKW[]   = {"what can you do","help","capabilities","what do you do","features","abilities"};
static constexpr const char* kReminderKW[]       = {"reminder","remind","reminders","reminded","remind me","my reminders"};
static constexpr const char* kGoalKW[]           = {"goal","goals","objective","objectives","working on","project"};
static constexpr const char* kHabitKW[]          = {"habit","habits","habit tracking","my habits"};
static constexpr const char* kPlannerKW[]        = {"schedule","plan","today","planner","task","tasks","exam","deadline"};
static constexpr const char* kMemoryKW[]         = {"remember","memory","what did i","last conversation","recall","do you remember"};
static constexpr const char* kKnowledgeKW[]      = {"what is","who is","tell me about","what are","define","what's a"};
static constexpr const char* kSettingsKW[]       = {"settings","personality","volume","brightness","wifi","language"};
static constexpr const char* kWifiStatusKW[]     = {"wifi status","internet","connection status","network","am i connected","wifi connection"};
static constexpr const char* kStorageStatusKW[]  = {"storage","sd card","space left","disk space","free space","memory space"};
static constexpr const char* kPersonalityKW[]    = {"personality active","active personality","current personality","what personality","who am i speaking","profile"};
static constexpr const char* kDecisionKW[]       = {"decision","decide","compare","choose","which option","best choice","rank","risk"};
static constexpr const char* kLearningKW[]       = {"what you learned","what did you learn","patterns","observations","learned","learning"};
static constexpr const char* kRecommendationKW[] = {"recommend","recommendation","suggestion","suggest","what should i","advise","advice"};
static constexpr const char* kPredictionKW[]     = {"predict","prediction","forecast","probability","chance of","what will","future"};
static constexpr const char* kDocumentKW[]       = {"document","documents","file","files","text file","upload","documentation"};
static constexpr const char* kWorkspaceKW[]      = {"workspace","workspaces","project space","my projects","organize"};
static constexpr const char* kDeveloperKW[]      = {"developer","diagnostics","performance","system health","metrics","debug","status report"};
static constexpr const char* kStudyKW[]          = {"i want to study","study","learn","learning","subject","subjects","course","courses"};
static constexpr const char* kFlashcardKW[]      = {"flashcard","flash cards","flashcards","show me flashcards"};
static constexpr const char* kQuizKW[]           = {"quiz","quiz me","test me","examine","question me"};
static constexpr const char* kPairKW[]           = {"pair","pair with","connect device","link device","bluetooth pair"};
static constexpr const char* kSyncKW[]           = {"sync","synchronize","synchronise","sync with"};
static constexpr const char* kDashboardKW[]      = {"dashboard","show dashboard","show my dashboard","main screen","home screen"};
static constexpr const char* kCreateSkillKW[]    = {"create skill","create a new skill","make a skill","new skill","add skill"};

const IntentClassifier::IntentPattern IntentClassifier::kPatterns[] = {
    { IntentType::GREETING,         kGreetingKW,      sizeof(kGreetingKW) / sizeof(kGreetingKW[0])      },
    { IntentType::SMALL_TALK,       kSmallTalkKW,     sizeof(kSmallTalkKW) / sizeof(kSmallTalkKW[0])    },
    { IntentType::CAPABILITIES,     kCapabilitiesKW,  sizeof(kCapabilitiesKW) / sizeof(kCapabilitiesKW[0]) },
    { IntentType::REMINDER_QUERY,   kReminderKW,      sizeof(kReminderKW) / sizeof(kReminderKW[0])      },
    { IntentType::GOAL_QUERY,       kGoalKW,          sizeof(kGoalKW) / sizeof(kGoalKW[0])              },
    { IntentType::HABIT_QUERY,      kHabitKW,         sizeof(kHabitKW) / sizeof(kHabitKW[0])            },
    { IntentType::PLANNER_QUERY,    kPlannerKW,       sizeof(kPlannerKW) / sizeof(kPlannerKW[0])        },
    { IntentType::MEMORY_QUERY,     kMemoryKW,        sizeof(kMemoryKW) / sizeof(kMemoryKW[0])          },
    { IntentType::KNOWLEDGE_QUERY,  kKnowledgeKW,     sizeof(kKnowledgeKW) / sizeof(kKnowledgeKW[0])    },
    { IntentType::SETTINGS_QUERY,   kSettingsKW,      sizeof(kSettingsKW) / sizeof(kSettingsKW[0])      },
    { IntentType::WIFI_STATUS,      kWifiStatusKW,    sizeof(kWifiStatusKW) / sizeof(kWifiStatusKW[0])  },
    { IntentType::STORAGE_STATUS,   kStorageStatusKW, sizeof(kStorageStatusKW) / sizeof(kStorageStatusKW[0]) },
    { IntentType::PERSONALITY_QUERY,kPersonalityKW,   sizeof(kPersonalityKW) / sizeof(kPersonalityKW[0]) },
    { IntentType::DECISION_QUERY,   kDecisionKW,      sizeof(kDecisionKW) / sizeof(kDecisionKW[0])      },
    { IntentType::LEARNING_QUERY,   kLearningKW,      sizeof(kLearningKW) / sizeof(kLearningKW[0])      },
    { IntentType::RECOMMENDATION_QUERY, kRecommendationKW, sizeof(kRecommendationKW) / sizeof(kRecommendationKW[0]) },
    { IntentType::PREDICTION_QUERY, kPredictionKW,    sizeof(kPredictionKW) / sizeof(kPredictionKW[0])  },
    { IntentType::DOCUMENT_QUERY,   kDocumentKW,      sizeof(kDocumentKW) / sizeof(kDocumentKW[0])      },
    { IntentType::WORKSPACE_QUERY,  kWorkspaceKW,     sizeof(kWorkspaceKW) / sizeof(kWorkspaceKW[0])    },
    { IntentType::DEVELOPER_QUERY,  kDeveloperKW,     sizeof(kDeveloperKW) / sizeof(kDeveloperKW[0])    },
    { IntentType::INTENT_STUDY,     kStudyKW,         sizeof(kStudyKW) / sizeof(kStudyKW[0])            },
    { IntentType::INTENT_FLASHCARD, kFlashcardKW,     sizeof(kFlashcardKW) / sizeof(kFlashcardKW[0])    },
    { IntentType::INTENT_QUIZ,      kQuizKW,          sizeof(kQuizKW) / sizeof(kQuizKW[0])              },
    { IntentType::INTENT_PAIR,      kPairKW,          sizeof(kPairKW) / sizeof(kPairKW[0])              },
    { IntentType::INTENT_SYNC,      kSyncKW,          sizeof(kSyncKW) / sizeof(kSyncKW[0])              },
    { IntentType::INTENT_DASHBOARD, kDashboardKW,     sizeof(kDashboardKW) / sizeof(kDashboardKW[0])    },
    { IntentType::INTENT_CREATE_SKILL, kCreateSkillKW, sizeof(kCreateSkillKW) / sizeof(kCreateSkillKW[0]) }
};

IntentClassifier::IntentClassifier() noexcept {}
IntentClassifier::~IntentClassifier() noexcept {}

IntentResult IntentClassifier::classify(const String& text) const noexcept {
    if (text.isEmpty()) {
        return IntentResult();
    }

    String lower = text;
    lower.toLowerCase();

    IntentResult best;
    best.type = IntentType::UNKNOWN;
    best.confidence = 0.0f;

    for (size_t i = 0; i < kPatternCount; ++i) {
        float confidence = matchPattern(lower, kPatterns[i]);
        if (confidence > best.confidence) {
            best.confidence = confidence;
            best.type = kPatterns[i].type;
        }
    }

    if (best.confidence > 0.3f) {
        extractEntities(text, best);
    }

    return best;
}

float IntentClassifier::matchPattern(const String& text, const IntentPattern& pattern) const noexcept {
    size_t matches = 0;
    for (size_t i = 0; i < pattern.keywordCount; ++i) {
        if (containsKeyword(text, pattern.keywords[i])) {
            matches++;
        }
    }

    if (matches == 0) return 0.0f;

    float raw = static_cast<float>(matches) / static_cast<float>(pattern.keywordCount);
    return raw > 1.0f ? 1.0f : raw;
}

void IntentClassifier::extractEntities(const String& text, IntentResult& result) const noexcept {
    result.entityCount = 0;

    if (result.type == IntentType::KNOWLEDGE_QUERY || result.type == IntentType::MEMORY_QUERY) {
        const char* triggers[] = {"what is ","what are ","who is ","tell me about ","define ","remember ","what did i "};
        String lower = text;
        lower.toLowerCase();
        for (auto trigger : triggers) {
            int idx = lower.indexOf(trigger);
            if (idx >= 0) {
                String entity = text.substring(idx + strlen(trigger));
                entity.trim();
                if (entity.length() > 0 && entity.length() < 100) {
                    result.entities[result.entityCount++] = entity;
                }
                break;
            }
        }
    }

    if (result.entityCount == 0 && result.type != IntentType::UNKNOWN) {
        String lower = text;
        lower.toLowerCase();
        const char* topicWords[] = {"reminders","goals","habits","tasks","schedule","project","exam","memory","wifi","storage","personality","reminder","goal","habit","task","decision","recommendation","prediction","document","workspace","pattern"};
        for (auto word : topicWords) {
            if (containsKeyword(lower, word)) {
                result.entities[result.entityCount++] = String(word);
                break;
            }
        }
    }
}

bool IntentClassifier::containsKeyword(const String& text, const char* keyword) const noexcept {
    return text.indexOf(keyword) >= 0;
}
