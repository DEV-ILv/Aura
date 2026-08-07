#include "aura_mood.h"

const char* auraMoodName(const AuraMood mood) noexcept {
    switch (mood) {
        case AuraMood::BOOT:            return "BOOT";
        case AuraMood::IDLE:            return "IDLE";
        case AuraMood::LISTENING:       return "LISTENING";
        case AuraMood::THINKING:        return "THINKING";
        case AuraMood::PROCESSING:      return "PROCESSING";
        case AuraMood::SPEAKING:        return "SPEAKING";
        case AuraMood::HAPPY:           return "HAPPY";
        case AuraMood::SUCCESS:         return "SUCCESS";
        case AuraMood::REMINDER:        return "REMINDER";
        case AuraMood::WARNING:         return "WARNING";
        case AuraMood::ERROR:           return "ERROR";
        case AuraMood::CRITICAL:        return "CRITICAL";
        case AuraMood::OTA:             return "OTA";
        case AuraMood::OFFLINE:         return "OFFLINE";
        case AuraMood::SLEEP:           return "SLEEP";
        case AuraMood::WAKE:            return "WAKE";
        case AuraMood::WIFI_CONNECTING: return "WIFI_CONNECTING";
        case AuraMood::WIFI_CONNECTED:  return "WIFI_CONNECTED";
        case AuraMood::SETUP:           return "SETUP";
        default:                        return "UNKNOWN";
    }
}