#ifndef AURA_VERSION_H
#define AURA_VERSION_H

#include <Arduino.h>

// ============================================================================
// AURA OS - Single Version Configuration
// ============================================================================
// CHANGE ONLY THE CONSTANTS BELOW to upgrade firmware versioning.
// All display, speech, web, and API output is derived automatically
// from this file. Do not hardcode version strings elsewhere.
// ============================================================================

namespace aura {
namespace version {

// -- Semantic Version --------------------------------------------------------
constexpr uint8_t kMajor           = 1;
constexpr uint8_t kMinor           = 0;
constexpr uint8_t kPatch           = 0;

// -- Mark Release (Roman numeral, 1-based) -----------------------------------
constexpr uint8_t kMark            = 3;

// -- Identity ----------------------------------------------------------------
constexpr const char* kCodename    = "Phoenix";
constexpr const char* kChannel     = "Development";

// -- Derived Semantic Version String (must match kMajor.kMinor.kPatch) -------
constexpr const char* kSemVer      = "1.0.0";

// -- Roman Numeral -----------------------------------------------------------
constexpr const char* kMarkRoman() noexcept {
    return kMark == 1  ? "I"    :
           kMark == 2  ? "II"   :
           kMark == 3  ? "III"  :
           kMark == 4  ? "IV"   :
           kMark == 5  ? "V"    :
           kMark == 6  ? "VI"   :
           kMark == 7  ? "VII"  :
           kMark == 8  ? "VIII" :
           kMark == 9  ? "IX"   :
           kMark == 10 ? "X"    :
           "I";
}

// -- Mark in words (for TTS) -------------------------------------------------
constexpr const char* kMarkWords() noexcept {
    return kMark == 1  ? "One"   :
           kMark == 2  ? "Two"   :
           kMark == 3  ? "Three" :
           kMark == 4  ? "Four"  :
           kMark == 5  ? "Five"  :
           kMark == 6  ? "Six"   :
           kMark == 7  ? "Seven" :
           kMark == 8  ? "Eight" :
           kMark == 9  ? "Nine"  :
           kMark == 10 ? "Ten"   :
           "One";
}

// -- Build Information (compiler-provided) -----------------------------------
constexpr const char* kBuildDate   = __DATE__;
constexpr const char* kBuildTime   = __TIME__;

// -- Full OS Display Name (for logging / boot screen) ------------------------
constexpr const char* kOsName      = "AURA OS";

} // namespace version
} // namespace aura

// -- Common Aliases (for convenience) ----------------------------------------
#define AURA_OS_NAME     aura::version::kOsName
#define AURA_SEMVER      aura::version::kSemVer
#define AURA_MARK_ROMAN  aura::version::kMarkRoman()
#define AURA_MARK_WORDS  aura::version::kMarkWords()

#endif
