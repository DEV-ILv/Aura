#ifndef AURA_JSON_HELPERS_H
#define AURA_JSON_HELPERS_H

#include <Arduino.h>

static String escapeJson(const String& raw) noexcept {
    String escaped;
    escaped.reserve(raw.length() + 16);
    for (size_t i = 0; i < raw.length(); ++i) {
        const char c = raw[i];
        switch (c) {
            case '"':  escaped += "\\\"";  break;
            case '\\': escaped += "\\\\";  break;
            case '\b': escaped += "\\b";   break;
            case '\f': escaped += "\\f";   break;
            case '\n': escaped += "\\n";   break;
            case '\r': escaped += "\\r";   break;
            case '\t': escaped += "\\t";   break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<uint8_t>(c));
                    escaped += buf;
                } else {
                    escaped += c;
                }
                break;
        }
    }
    return escaped;
}
 
/// Generate a unique hex identifier using millis, monotonic counter, and MAC.
static inline String generateId() noexcept {
    static unsigned long s_counter = 0;
    s_counter++;
    unsigned long now = millis();
    uint32_t mix = static_cast<uint32_t>(now) ^ (s_counter << 16) ^ (ESP.getEfuseMac() & 0xFFFFFFFF);
    String id; id.reserve(12);
    static const char hex[] = "0123456789abcdef";
    uint32_t val = mix;
    for (size_t i = 0; i < 12; ++i) {
        id += hex[val & 0x0F];
        val = (val >> 2) ^ (val << 3) ^ (s_counter + i);
    }
    return id;
}
 
 #endif
