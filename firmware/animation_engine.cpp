#include "animation_engine.h"
#define _USE_MATH_DEFINES
#include <math.h>

AnimationEngine animationEngine;

AnimationEngine::AnimationEngine() noexcept : m_initialized(false) {
    for (auto& a : m_animations) a.active = false;
}

AnimationEngine::~AnimationEngine() noexcept = default;

bool AnimationEngine::initialize() noexcept {
    if (m_initialized) return true;
    m_initialized = true;
    LOG_INFO(kLogCategory, "AnimationEngine initialized");
    return true;
}

void AnimationEngine::update() noexcept {
    if (!m_initialized) return;
    for (auto& a : m_animations) {
        if (!a.active) continue;
        unsigned long now = millis();
        unsigned long elapsed = now - a.startTime;
        if (elapsed >= a.duration) {
            if (a.loop) {
                a.startTime = now;
            } else {
                a.active = false;
            }
        }
    }
}

void AnimationEngine::startAnimation(const Animation& anim) noexcept {
    for (auto& a : m_animations) {
        if (!a.active) {
            a = anim;
            a.startTime = millis();
            a.active = true;
            LOG_DEBUG(kLogCategory, "Animation started: type=%d target=%s", 
                      static_cast<int>(anim.type), anim.targetId.c_str());
            return;
        }
    }
}

void AnimationEngine::stopAnimation(const String& targetId) noexcept {
    for (auto& a : m_animations) {
        if (a.active && a.targetId == targetId) {
            a.active = false;
        }
    }
}

void AnimationEngine::stopAll() noexcept {
    for (auto& a : m_animations) a.active = false;
}

float AnimationEngine::getProgress(const Animation& anim) const noexcept {
    unsigned long elapsed = millis() - anim.startTime;
    if (elapsed >= anim.duration) return 1.0f;
    return static_cast<float>(elapsed) / static_cast<float>(anim.duration);
}

float AnimationEngine::applyEasing(float t, EasingFunction func) const noexcept {
    if (t >= 1.0f) return 1.0f;
    if (t <= 0.0f) return 0.0f;

    switch (func) {
        case EasingFunction::LINEAR:
            return t;
        case EasingFunction::EASE_IN_QUAD:
            return t * t;
        case EasingFunction::EASE_OUT_QUAD:
            return t * (2.0f - t);
        case EasingFunction::EASE_IN_OUT_QUAD:
            return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
        case EasingFunction::EASE_IN_CUBIC:
            return t * t * t;
        case EasingFunction::EASE_OUT_CUBIC:
            t = t - 1.0f;
            return t * t * t + 1.0f;
        case EasingFunction::EASE_IN_OUT_CUBIC:
            return (t < 0.5f) ? (4.0f * t * t * t) : ((t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f);
        case EasingFunction::EASE_OUT_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            return pow(2.0f, -10.0f * t) * sin((t - 0.075f) * (2.0f * M_PI) / 0.3f) + 1.0f;
        }
        case EasingFunction::EASE_OUT_BOUNCE: {
            if (t < (1.0f / 2.75f)) return 7.5625f * t * t;
            if (t < (2.0f / 2.75f)) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
            if (t < (2.5f / 2.75f)) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
            t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f;
        }
        default:
            return t;
    }
}

uint8_t AnimationEngine::getCurrentValue(const Animation& anim) const noexcept {
    float progress = getProgress(anim);
    float eased = applyEasing(progress, anim.easing);
    return static_cast<uint8_t>(anim.fromValue + (anim.toValue - anim.fromValue) * eased);
}

bool AnimationEngine::isAnimating(const String& targetId) const noexcept {
    for (const auto& a : m_animations) {
        if (a.active && a.targetId == targetId) return true;
    }
    return false;
}

bool AnimationEngine::isInitialized() const noexcept {
    return m_initialized;
}