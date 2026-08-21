#ifndef AURA_RECOMMENDATION_ENGINE_H
#define AURA_RECOMMENDATION_ENGINE_H

#include <Arduino.h>
#include "config.h"

/**
 * @struct RecommendationHint
 * @brief A lightweight, natural-language advice snippet produced before a
 *        response is generated.
 */
struct RecommendationHint {
    bool available;
    String advice;
    String category;   ///< e.g. "habit", "planner", "goal", "prediction"

    RecommendationHint() noexcept : available(false) {}
};

/**
 * @class RecommendationEngine
 * @brief Evaluates goals, habits, planner, predictions, learning patterns and
 *        executive recommendations before a response is generated, and returns
 *        one natural advice snippet to inject into the reply.
 *
 * Rule-based and cheap: evaluates at most a handful of managers and returns a
 * single hint. Never blocks.
 */
class RecommendationEngine {
public:
    RecommendationEngine() noexcept;
    ~RecommendationEngine() noexcept;

    RecommendationEngine(const RecommendationEngine&) = delete;
    RecommendationEngine& operator=(const RecommendationEngine&) = delete;

    /**
     * @brief Build the single best advice hint for the current moment.
     * @return A RecommendationHint; available==false when nothing useful found.
     */
    RecommendationHint build() noexcept;
};

extern RecommendationEngine recommendationEngine;

#endif // AURA_RECOMMENDATION_ENGINE_H
