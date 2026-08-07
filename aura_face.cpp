#include "aura_face.h"
#include <math.h>

AuraFace auraFace;

AuraFace::AuraFace() noexcept
    : m_display(nullptr)
    , m_targetExpression(FaceExpression::IDLE)
    , m_openness(0.6f)
    , m_attention(0.0f)
    , m_pupilX(0)
    , m_pupilY(0)
    , m_pupilTargetX(0)
    , m_pupilTargetY(0)
    , m_nextGlanceAt(0)
    , m_nextBlinkAt(0)
    , m_blinkStart(0)
    , m_blinking(false)
    , m_nextPulseAt(0)
    , m_pulseStart(0)
    , m_eventStart(0)
    , m_event(static_cast<uint8_t>(FaceEvent::NONE))
    , m_lastRender(0) {
    reset();
}

void AuraFace::setDisplay(Adafruit_SH1106G* display) noexcept {
    m_display = display;
}

void AuraFace::reset() noexcept {
    nextRandom(); // advance seed once
    m_targetExpression = FaceExpression::IDLE;
    m_openness = 0.0f;                    // starts closed for the boot reveal
    m_attention = 0.0f;
    m_pupilX = 0; m_pupilY = 0;
    m_pupilTargetX = 0; m_pupilTargetY = 0;
    m_nextGlanceAt = millis() + 1500U + (nextRandom() % 2500U);
    m_nextBlinkAt = millis() + 2200U + (nextRandom() % 1800U);
    m_blinking = false;
    m_nextPulseAt = millis() + kPulseIntervalMs;
    m_pulseStart = 0;
    m_event = static_cast<uint8_t>(FaceEvent::NONE);
}

void AuraFace::setExpression(FaceExpression expr) noexcept {
    if (expr >= FaceExpression::MAX) expr = FaceExpression::IDLE;
    m_targetExpression = expr;
    // Give a soft, deliberate transition cue: schedule an early blink.
    if (m_nextBlinkAt > millis() + 600U) {
        m_nextBlinkAt = millis() + 300U + (nextRandom() % 300U);
    }
}

void AuraFace::notify(FaceEvent event) noexcept {
    m_event = static_cast<uint8_t>(event);
    m_eventStart = millis();
    if (event == FaceEvent::TOUCH || event == FaceEvent::WAKE) {
        m_blinking = true;
        m_blinkStart = millis();
    }
}

void AuraFace::setAttention(uint8_t level) noexcept {
    m_attention = (level > 255) ? 1.0f : (float)level / 255.0f;
}

// ============================================================================
// Randomness (xorshift32 — cheap, no stdlib)
// ============================================================================

static uint32_t s_randState = 0x9E3779B9U;

uint32_t AuraFace::nextRandom() noexcept {
    uint32_t x = s_randState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_randState = x;
    return x;
}

float AuraFace::randUnit() noexcept {
    return (float)(nextRandom() & 0xFFFFU) / 65535.0f;
}

float AuraFace::clamp01(float v) noexcept {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float AuraFace::easeOutCubic(float t) noexcept {
    t = clamp01(t);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// ============================================================================
// Ambient behaviour
// ============================================================================

void AuraFace::updateBlink(unsigned long now) noexcept {
    if (!m_blinking) {
        if (now >= m_nextBlinkAt) {
            m_blinking = true;
            m_blinkStart = now;
        }
        return;
    }
    const float p = (float)(now - m_blinkStart) / (float)kBlinkDurationMs;
    if (p >= 1.0f) {
        m_blinking = false;
        m_nextBlinkAt = now + kBlinkIntervalMinMs
            + (nextRandom() % (kBlinkIntervalMaxMs - kBlinkIntervalMinMs));
    }
}

void AuraFace::updatePupil(unsigned long now) noexcept {
    const unsigned long dt = (m_lastRender == 0) ? 33U : (now - m_lastRender);
    if (now >= m_nextGlanceAt) {
        m_pupilTargetX = (int16_t)((int32_t)(nextRandom() % (2 * kPupilMaxX + 1))) - kPupilMaxX;
        m_pupilTargetY = (int16_t)((int32_t)(nextRandom() % (2 * kPupilMaxY + 1))) - kPupilMaxY;
        m_nextGlanceAt = now + kGlanceIntervalMs + (nextRandom() % 2600U);
    }
    const float k = clamp01((float)dt / 120.0f);
    m_pupilX += (int16_t)((float)(m_pupilTargetX - m_pupilX) * k);
    m_pupilY += (int16_t)((float)(m_pupilTargetY - m_pupilY) * k);
}

// ============================================================================
// Expression parameters
// ============================================================================

static float eyeOpenTarget(FaceExpression e) noexcept {
    switch (e) {
        case FaceExpression::LISTENING: return 0.92f;
        case FaceExpression::THINKING:  return 0.50f;
        case FaceExpression::SPEAKING:  return 0.72f;
        case FaceExpression::HAPPY:     return 0.58f;
        case FaceExpression::CONCERNED: return 0.42f;
        case FaceExpression::CRITICAL:  return 0.28f;
        case FaceExpression::OFFLINE:   return 0.05f;
        case FaceExpression::IDLE:
        default:                        return 0.60f;
    }
}

// ============================================================================
// Drawing
// ============================================================================

void AuraFace::drawEye(int16_t cx, int16_t cy, float openness,
                       int16_t pupilDx, int16_t pupilDy) noexcept {
    const float o = clamp01(openness);
    const int16_t ry = (int16_t)((float)kEyeRy * o);
    if (ry < 2) {
        // Closed / sleeping — a soft horizontal line (no pupil).
        m_display->drawFastHLine(cx - kEyeRx, cy, 2 * kEyeRx, SH110X_WHITE);
        return;
    }
    m_display->fillEllipse(cx, cy, kEyeRx, ry, SH110X_WHITE);
    if (o > 0.28f) {
        const int16_t pr = 2;
        m_display->fillCircle(cx + pupilDx, cy + pupilDy, pr, SH110X_BLACK);
        // tiny highlight for life
        m_display->fillCircle(cx + pupilDx - 1, cy + pupilDy - 1, 1, SH110X_WHITE);
    }
}

void AuraFace::drawArc(int16_t cx, int16_t cy, int16_t r,
                       float a0, float a1, uint8_t thickness) noexcept {
    const uint8_t segs = 8;
    int16_t prevX = 0, prevY = 0;
    for (uint8_t i = 0; i <= segs; ++i) {
        const float a = a0 + (a1 - a0) * (float)i / (float)segs;
        const float s = sinf(a), c = cosf(a);
        const int16_t bx = cx + (int16_t)((float)r * c);
        const int16_t by = cy + (int16_t)((float)r * s);
        for (uint8_t t = 0; t <= thickness; ++t) {
            m_display->drawPixel(cx + (int16_t)((float)(r + t) * c),
                                 cy + (int16_t)((float)(r + t) * s), SH110X_WHITE);
        }
        if (i > 0) {
            m_display->drawLine(prevX, prevY, bx, by, SH110X_WHITE);
        }
        prevX = bx; prevY = by;
    }
}

void AuraFace::drawBrows(float tilt) noexcept {
    // tilt > 0 slopes downward toward the centre (concerned), < 0 lifts outward.
    const int8_t sl = (int8_t)(2.0f * tilt);
    const int16_t ly = kFaceCy - kEyeRy - 3;
    m_display->drawLine(kFaceCx - kEyeSep - kEyeRx, ly, kFaceCx - kEyeSep + kEyeRx, ly + sl, SH110X_WHITE);
    m_display->drawLine(kFaceCx + kEyeSep - kEyeRx, ly + sl, kFaceCx + kEyeSep + kEyeRx, ly, SH110X_WHITE);
}

void AuraFace::drawThinkingDots(unsigned long now) noexcept {
    const float t = (float)(now % 628) * 0.01f;
    for (uint8_t i = 0; i < 3; ++i) {
        const float a = t + (float)i * 2.0944f;
        const int16_t dx = (int16_t)(13.0f * cosf(a));
        const int16_t dy = (int16_t)(9.0f * sinf(a)) / 2;
        m_display->fillCircle(kFaceCx + dx, kFaceCy + dy, 1, SH110X_WHITE);
    }
}

void AuraFace::drawListeningPulse(unsigned long now) noexcept {
    if (now < m_nextPulseAt) return;
    const uint32_t dur = 700U;
    const uint32_t dt = now - m_pulseStart;
    if (dt > dur) {
        m_nextPulseAt = now + kPulseIntervalMs + (nextRandom() % 1600U);
        return;
    }
    const float k = easeOutCubic((float)dt / (float)dur);
    const int16_t r = 8 + (int16_t)(k * 14.0f);
    m_display->drawCircle(kFaceCx, kFaceCy, r, SH110X_WHITE);
}

void AuraFace::drawMouth(float amount, unsigned long now) noexcept {
    // Irregular cadence so talking never looks mechanical.
    float v = 0.5f + 0.5f * sinf((float)now * 0.011f);
    v = v * (0.6f + 0.4f * sinf((float)now * 0.0047f + 1.3f));
    const int16_t h = 3 + (int16_t)(clamp01(v) * 4.0f * clamp01(amount) + 0.5f);
    const int16_t x = kFaceCx - kMouthHalfW;
    const int16_t y = kMouthCy - h / 2;
    m_display->fillRoundRect(x, y, 2 * kMouthHalfW, h, 2, SH110X_WHITE);
    m_display->drawFastHLine(x, y, 2 * kMouthHalfW, SH110X_BLACK); // subtle split
}

void AuraFace::drawEventGlyph(unsigned long now) noexcept {
    if (m_event == static_cast<uint8_t>(FaceEvent::NONE)) return;
    const uint32_t dt = now - m_eventStart;
    if (dt > kEventDurationMs) {
        m_event = static_cast<uint8_t>(FaceEvent::NONE);
        return;
    }
    const float k = clamp01((float)dt / (float)kEventDurationMs);   // 0..1
    const float fade = 1.0f - k;

    switch (static_cast<FaceEvent>(m_event)) {
        case FaceEvent::WIFI_CONNECTED: {
            // small wifi arcs, top-right
            const int16_t bx = 116, by = 16;
            drawArc(bx, by, 6, 3.35f, 3.95f, 1);
            drawArc(bx, by, 9, 3.35f, 3.95f, 1);
            drawArc(bx, by, 13, 3.05f, 4.25f, 1);
            m_display->fillCircle(bx, by + 5, 1, SH110X_WHITE);
            // gentle smile under the face
            drawArc(kFaceCx, kMouthCy + 4, 9, 0.25f * 3.14159f, 0.75f * 3.14159f, 1);
            break;
        }
        case FaceEvent::TOUCH:
        case FaceEvent::WAKE: {
            // expanding acknowledgement ring around the face
            const int16_t r = 6 + (int16_t)(k * 14.0f);
            m_display->drawCircle(kFaceCx, kFaceCy, r, SH110X_WHITE);
            break;
        }
        case FaceEvent::STORAGE: {
            // small SD card, bottom-right
            const int16_t sx = 112, sy = 52;
            m_display->drawRect(sx - 5, sy - 5, 10, 10, SH110X_WHITE);
            m_display->fillRect(sx - 2, sy - 5, 4, 3, SH110X_WHITE);
            break;
        }
        case FaceEvent::OTA: {
            // small pulsing progress dots, bottom-left
            for (uint8_t i = 0; i < 3; ++i) {
                const float ph = (float)((dt / 180U + i) % 3) / 3.0f;
                const uint8_t r = (uint8_t)(1.0f + fade * (ph * 2.0f));
                m_display->fillCircle(6 + i * 5, 58 - (int16_t)(ph * 3.0f), r, SH110X_WHITE);
            }
            break;
        }
        case FaceEvent::NONE:
        default:
            break;
    }
    (void)fade;
}

// ============================================================================
// Main render
// ============================================================================

void AuraFace::render(unsigned long now) noexcept {
    if (!m_display) return;

    updateBlink(now);
    updatePupil(now);

    // Ease eye openness toward the expression target.
    float target = eyeOpenTarget(m_targetExpression);
    if (m_targetExpression == FaceExpression::LISTENING) {
        target = clamp01(target + m_attention * 0.06f);
    }
    float dtK = clamp01((float)(now - m_lastRender) / 33.0f);
    m_openness += (target - m_openness) * dtK * 0.35f;

    // Blink closes the eyes momentarily.
    float open = m_openness;
    if (m_blinking) {
        const float p = clamp01((float)(now - m_blinkStart) / (float)kBlinkDurationMs);
        open = m_openness * (1.0f - sinf(p * 3.14159f));
    }

    // Gentle breathing — subtle vertical drift & size.
    const float breathe = sinf((float)now * 0.00165f);
    const int16_t cy = kFaceCy + (int16_t)(breathe * 0.9f);
    open *= 1.0f + breathe * 0.03f;

    // Pupils (thinking scans left/right).
    int16_t px = m_pupilX, py = m_pupilY;
    if (m_targetExpression == FaceExpression::THINKING) {
        px = (int16_t)(sinf((float)now * 0.0035f) * 3.0f);
        py = 0;
    }

    m_display->clearDisplay();

    // Draw the eyes according to expression style.
    switch (m_targetExpression) {
        case FaceExpression::HAPPY: {
            // soft arched eyes
            drawArc(kFaceCx - kEyeSep, cy, kEyeRx + 2, 0.18f * 3.14159f, 0.82f * 3.14159f, 1);
            drawArc(kFaceCx + kEyeSep, cy, kEyeRx + 2, 0.18f * 3.14159f, 0.82f * 3.14159f, 1);
            // small smile
            drawArc(kFaceCx, kMouthCy + 2, 7, 0.25f * 3.14159f, 0.75f * 3.14159f, 1);
            break;
        }
        case FaceExpression::CONCERNED: {
            drawEye(kFaceCx - kEyeSep, cy, open, px, py);
            drawEye(kFaceCx + kEyeSep, cy, open, px, py);
            drawBrows(1.0f);
            break;
        }
        case FaceExpression::CRITICAL: {
            drawBrows(-0.6f);
            drawEye(kFaceCx - kEyeSep, cy, open, 0, 0);
            drawEye(kFaceCx + kEyeSep, cy, open, 0, 0);
            // calm warning mark
            m_display->drawFastVLine(kFaceCx, kMouthCy - 2, 6, SH110X_WHITE);
            m_display->fillCircle(kFaceCx, kMouthCy + 5, 1, SH110X_WHITE);
            break;
        }
        case FaceExpression::SPEAKING:
            drawEye(kFaceCx - kEyeSep, cy, open, px, py);
            drawEye(kFaceCx + kEyeSep, cy, open, px, py);
            drawMouth(1.0f, now);
            break;
        case FaceExpression::THINKING:
            drawEye(kFaceCx - kEyeSep, cy, open, px, py);
            drawEye(kFaceCx + kEyeSep, cy, open, px, py);
            drawThinkingDots(now);
            break;
        case FaceExpression::LISTENING:
            drawEye(kFaceCx - kEyeSep, cy, open, px, py);
            drawEye(kFaceCx + kEyeSep, cy, open, px, py);
            drawListeningPulse(now);
            break;
        case FaceExpression::IDLE:
            drawEye(kFaceCx - kEyeSep, cy, open, px, py);
            drawEye(kFaceCx + kEyeSep, cy, open, px, py);
            // Small gentle breathing smile.
            {
                const int16_t smileR = 7 + (int16_t)(breathe * 1.5f);
                const int16_t my = kMouthCy + 2 + (int16_t)(breathe * 0.9f);
                drawArc(kFaceCx, my, smileR, 0.30f * 3.14159f, 0.70f * 3.14159f, 1);
            }
            break;
        case FaceExpression::OFFLINE:
            drawEye(kFaceCx - kEyeSep, cy, 0.0f, 0, 0);
            drawEye(kFaceCx + kEyeSep, cy, 0.0f, 0, 0);
            break;
        default:
            break;
    }

    drawEventGlyph(now);
    m_lastRender = now;
}