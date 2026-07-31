#ifndef AURA_ANIMATION_ENGINE_H
#define AURA_ANIMATION_ENGINE_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "logger.h"

enum class EasingFunction : uint8_t {
    LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_OUT_ELASTIC,
    EASE_OUT_BOUNCE
};

enum class AnimationType : uint8_t {
    FADE_IN,
    FADE_OUT,
    SLIDE_UP,
    SLIDE_DOWN,
    SLIDE_LEFT,
    SLIDE_RIGHT,
    PULSE,
    SPIN,
    PROGRESS
};

struct Animation {
    AnimationType type;
    EasingFunction easing;
    unsigned long startTime;
    unsigned long duration;
    uint8_t fromValue;
    uint8_t toValue;
    bool    active;
    bool    loop;
    String  targetId;     // Widget or element ID
    void*   context;      // Optional context pointer
};

class AnimationEngine {
public:
    AnimationEngine() noexcept;
    ~AnimationEngine() noexcept;

    [[nodiscard]] bool initialize() noexcept;
    void update() noexcept;

    void startAnimation(const Animation& anim) noexcept;
    void stopAnimation(const String& targetId) noexcept;
    void stopAll() noexcept;

    [[nodiscard]] float getProgress(const Animation& anim) const noexcept;
    [[nodiscard]] float applyEasing(float t, EasingFunction func) const noexcept;
    [[nodiscard]] uint8_t getCurrentValue(const Animation& anim) const noexcept;
    [[nodiscard]] bool isAnimating(const String& targetId) const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;

private:
    static constexpr const char* kLogCategory = "AnimationEngine";
    static constexpr size_t kMaxAnimations = 16;

    bool m_initialized;
    Animation m_animations[kMaxAnimations];
};

extern AnimationEngine animationEngine;

#endif