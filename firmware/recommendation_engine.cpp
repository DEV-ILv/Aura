#include "recommendation_engine.h"

#include "habit_manager.h"
#include "planner_manager.h"
#include "goal_manager.h"
#include "reminder_manager.h"
#include "prediction_manager.h"
#include "learning_manager.h"
#include "executive_assistant.h"
#include "logger.h"

RecommendationEngine recommendationEngine;

RecommendationEngine::RecommendationEngine() noexcept {}
RecommendationEngine::~RecommendationEngine() noexcept {}

RecommendationHint RecommendationEngine::build() noexcept {
    RecommendationHint hint;

    // 1. Planner: highest-priority due task worth finishing.
    if (plannerManager.isInitialized()) {
        auto todays = plannerManager.getTodaysTasks();
        if (!todays.empty()) {
            PlannedTask* top = nullptr;
            for (auto& t : todays) {
                if (t.completed) continue;
                if (top == nullptr || t.priority > top->priority) top = &t;
            }
            if (top != nullptr) {
                hint.available = true;
                hint.category = "planner";
                hint.advice = "Finishing \"" + top->title +
                              "\" would be a strong next step, and it looks like the highest-priority item right now.";
                return hint;
            }
        }
    }

    // 2. Habit: a due habit keeps streaks alive.
    if (habitManager.isInitialized()) {
        auto due = habitManager.getDueHabits();
        if (!due.empty()) {
            const HabitEntry& h = due[0];
            hint.available = true;
            hint.category = "habit";
            hint.advice = "Your \"" + h.name +
                          "\" habit is due today - knocking it out now keeps your streak of " +
                          String(static_cast<unsigned long>(h.streak)) + " days intact.";
            return hint;
        }
    }

    // 3. Goal: one active goal at risk of slipping.
    if (goalManager.isInitialized()) {
        auto goals = goalManager.getActiveGoals();
        if (!goals.empty()) {
            for (const auto& g : goals) {
                if (g.progress < 40) {
                    hint.available = true;
                    hint.category = "goal";
                    hint.advice = "Your goal \"" + g.title +
                                  "\" is at " + String(static_cast<unsigned int>(g.progress)) +
                                  "% - a small push today would keep it on track.";
                    return hint;
                }
            }
        }
    }

    // 4. ExecutiveAssistant: surface an active recommendation.
    if (executiveAssistant.isInitialized()) {
        auto recs = executiveAssistant.getActiveRecommendations();
        if (!recs.empty()) {
            const Recommendation& r = recs[0];
            hint.available = true;
            hint.category = "recommendation";
            hint.advice = r.title + ": " + r.description;
            return hint;
        }
    }

    // 5. Prediction: a high-probability upcoming event.
    if (predictionManager.isInitialized()) {
        auto preds = predictionManager.getActivePredictions(0.7f);
        if (!preds.empty()) {
            const Prediction& p = preds[0];
            hint.available = true;
            hint.category = "prediction";
            hint.advice = "I expect \"" + p.targetName +
                          "\" with " + String(static_cast<unsigned int>(p.probability * 100)) +
                          "% probability - you may want to plan around it.";
            return hint;
        }
    }

    // 6. Learning: a learned pattern with a suggestion.
    if (learningManager.isInitialized()) {
        auto patterns = learningManager.getActivePatterns();
        if (!patterns.empty()) {
            for (const auto& p : patterns) {
                if (!p.suggestion.isEmpty()) {
                    hint.available = true;
                    hint.category = "learning";
                    hint.advice = "From your patterns, " + p.suggestion;
                    return hint;
                }
            }
        }
    }

    return hint;
}
