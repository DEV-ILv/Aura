#ifndef AURA_PERFORMANCE_OPTIMIZATIONS_H
#define AURA_PERFORMANCE_OPTIMIZATIONS_H

#include <Arduino.h>
#include "config.h"

/**
 * @file performance_optimizations.h
 * @brief Performance optimization utilities and patterns
 *
 * This file provides reusable performance patterns for all subsystems:
 * - Static buffers to reduce heap allocations
 * - Shared string pools
 * - Cache-friendly data structures
 * - Non-blocking patterns
 */

namespace aura {
namespace perf {

// ============================================================================
// Fixed-size string buffer (avoids String heap allocation)
// ============================================================================
template <size_t N>
class FixedString {
public:
    FixedString() noexcept : m_len(0) { m_buf[0] = '\0'; }

    void set(const char* src) noexcept {
        size_t i = 0;
        while (i < N - 1 && src[i]) { m_buf[i] = src[i]; ++i; }
        m_buf[i] = '\0';
        m_len = i;
    }

    void append(const char* src) noexcept {
        size_t i = m_len;
        size_t j = 0;
        while (i < N - 1 && src[j]) { m_buf[i] = src[j]; ++i; ++j; }
        m_buf[i] = '\0';
        m_len = i;
    }

    void clear() noexcept { m_buf[0] = '\0'; m_len = 0; }
    const char* c_str() const noexcept { return m_buf; }
    size_t length() const noexcept { return m_len; }
    bool empty() const noexcept { return m_len == 0; }

private:
    char m_buf[N];
    size_t m_len;
};

// ============================================================================
// Ring buffer (fixed-size, no allocations)
// ============================================================================
template <typename T, size_t N>
class RingBuffer {
public:
    RingBuffer() noexcept : m_head(0), m_tail(0), m_count(0) {}

    bool push(const T& item) noexcept {
        if (m_count >= N) return false;
        m_buf[m_head] = item;
        m_head = (m_head + 1) % N;
        m_count++;
        return true;
    }

    bool pop(T& item) noexcept {
        if (m_count == 0) return false;
        item = m_buf[m_tail];
        m_tail = (m_tail + 1) % N;
        m_count--;
        return true;
    }

    T& operator[](size_t idx) noexcept { return m_buf[(m_tail + idx) % N]; }
    const T& operator[](size_t idx) const noexcept { return m_buf[(m_tail + idx) % N]; }

    size_t size() const noexcept { return m_count; }
    bool empty() const noexcept { return m_count == 0; }
    bool full() const noexcept { return m_count >= N; }
    void clear() noexcept { m_head = m_tail; m_count = 0; }
    size_t capacity() const noexcept { return N; }

private:
    T m_buf[N];
    size_t m_head;
    size_t m_tail;
    size_t m_count;
};

// ============================================================================
// Shared buffer pool (reduces repeated allocations)
// ============================================================================
template <size_t BUFFER_SIZE, size_t POOL_SIZE>
class BufferPool {
public:
    BufferPool() noexcept : m_count(0) {
        for (auto& free : m_free) free = false;
    }

    char* acquire() noexcept {
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            if (!m_free[i]) {
                m_free[i] = true;
                m_buffers[i][0] = '\0';
                return m_buffers[i];
            }
        }
        return nullptr; // Pool exhausted
    }

    void release(char* buf) noexcept {
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            if (m_buffers[i] == buf) {
                m_free[i] = false;
                return;
            }
        }
    }

private:
    char m_buffers[POOL_SIZE][BUFFER_SIZE];
    bool m_free[POOL_SIZE];
    size_t m_count;
};

// ============================================================================
// Non-blocking patterns
// ============================================================================

// Cooperative yield point — call in long-running loops
inline void yieldIfNeeded() noexcept {
    static unsigned long lastYield = 0;
    unsigned long now = millis();
    if (now - lastYield > 5) {  // Yield every ~5ms
        lastYield = now;
        delay(1);  // FreeRTOS yield
    }
}

// Batch processor — process N items per call, resume next call
template <typename T>
class BatchProcessor {
public:
    BatchProcessor() noexcept : m_index(0), m_total(0) {}

    void start(T* items, size_t count, size_t batchSize = 5) noexcept {
        m_items = items;
        m_total = count;
        m_index = 0;
        m_batchSize = batchSize;
    }

    size_t process() noexcept {
        size_t processed = 0;
        size_t end = m_index + m_batchSize;
        if (end > m_total) end = m_total;

        for (; m_index < end; ++m_index) {
            processItem(m_items[m_index]);
            processed++;
        }

        return processed;
    }

    bool done() const noexcept { return m_index >= m_total; }
    size_t progress() const noexcept {
        return m_total > 0 ? (m_index * 100 / m_total) : 100;
    }

private:
    void processItem(T& item) noexcept { (void)item; }
    T* m_items;
    size_t m_index;
    size_t m_total;
    size_t m_batchSize;
};

// JSON builder — pre-allocated buffer to avoid String concatenation
class JSONBuilder {
public:
    JSONBuilder(char* buffer, size_t size) noexcept
        : m_buf(buffer), m_size(size), m_pos(0) {
        if (m_size > 0) m_buf[0] = '{';
        m_pos = 1;
    }

    void addString(const char* key, const char* value) noexcept {
        int n = snprintf(m_buf + m_pos, m_size - m_pos,
                        "\"%s\":\"%s\",", key, value);
        if (n > 0) m_pos += n;
        if (m_pos >= m_size) m_pos = m_size - 1;
    }

    void addInt(const char* key, int value) noexcept {
        int n = snprintf(m_buf + m_pos, m_size - m_pos,
                        "\"%s\":%d,", key, value);
        if (n > 0) m_pos += n;
        if (m_pos >= m_size) m_pos = m_size - 1;
    }

    void addFloat(const char* key, float value, int precision = 1) noexcept {
        char fmt[16];
        snprintf(fmt, sizeof(fmt), "\"%%s\":%%.%df,", precision);
        int n = snprintf(m_buf + m_pos, m_size - m_pos, fmt, key, value);
        if (n > 0) m_pos += n;
        if (m_pos >= m_size) m_pos = m_size - 1;
    }

    void addBool(const char* key, bool value) noexcept {
        int n = snprintf(m_buf + m_pos, m_size - m_pos,
                        "\"%s\":%s,", key, value ? "true" : "false");
        if (n > 0) m_pos += n;
        if (m_pos >= m_size) m_pos = m_size - 1;
    }

    void finalize() noexcept {
        // Replace trailing comma with closing brace
        if (m_pos > 1 && m_buf[m_pos - 1] == ',') m_pos--;
        if (m_pos < m_size) {
            m_buf[m_pos] = '}';
            m_buf[m_pos + 1] = '\0';
        }
    }

    const char* c_str() const noexcept { return m_buf; }
    size_t length() const noexcept { return m_pos + 1; }

private:
    char* m_buf;
    size_t m_size;
    size_t m_pos;
};

} // namespace perf
} // namespace aura

#endif