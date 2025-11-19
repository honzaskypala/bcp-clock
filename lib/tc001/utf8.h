#pragma once
#include <Arduino.h>

namespace Utf8 {

static const uint32_t REPLACEMENT_CHAR = 0xFFFD;

static inline bool isCont(uint8_t b) {
    return (b & 0xC0) == 0x80;
}

// Decode one UTF-8 code point from s[i...], advance i by the number of bytes consumed.
// Returns REPLACEMENT_CHAR (U+FFFD) on invalid sequences but always consumes at least 1 byte.
static uint32_t decode(const char* s, size_t len, size_t& i) {
    if (i >= len) return REPLACEMENT_CHAR;

    uint8_t b0 = static_cast<uint8_t>(s[i++]);

    if (b0 < 0x80) {
        // 1-byte ASCII
        return b0;
    }

    // Determine expected length from leading bits
    if ((b0 & 0xE0) == 0xC0) {
        // 2-byte sequence
        if (i >= len) return REPLACEMENT_CHAR;
        uint8_t b1 = static_cast<uint8_t>(s[i]);
        if (!isCont(b1)) return REPLACEMENT_CHAR;
        i++;
        uint32_t cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        // Overlong check
        if (cp < 0x80) return REPLACEMENT_CHAR;
        return cp;
    } else if ((b0 & 0xF0) == 0xE0) {
        // 3-byte sequence
        if (i + 1 >= len) return REPLACEMENT_CHAR;
        uint8_t b1 = static_cast<uint8_t>(s[i]);
        uint8_t b2 = static_cast<uint8_t>(s[i + 1]);
        if (!isCont(b1) || !isCont(b2)) return REPLACEMENT_CHAR;
        i += 2;
        uint32_t cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        // Overlong and surrogate range check
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return REPLACEMENT_CHAR;
        return cp;
    } else if ((b0 & 0xF8) == 0xF0) {
        // 4-byte sequence
        if (i + 2 >= len) return REPLACEMENT_CHAR;
        uint8_t b1 = static_cast<uint8_t>(s[i]);
        uint8_t b2 = static_cast<uint8_t>(s[i + 1]);
        uint8_t b3 = static_cast<uint8_t>(s[i + 2]);
        if (!isCont(b1) || !isCont(b2) || !isCont(b3)) return REPLACEMENT_CHAR;
        i += 3;
        uint32_t cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) |
                      ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        // Overlong and range check
        if (cp < 0x10000 || cp > 0x10FFFF) return REPLACEMENT_CHAR;
        return cp;
    }

    // Invalid leading byte
    return REPLACEMENT_CHAR;
}

// Count Unicode code points in a UTF-8 encoded Arduino String.
static size_t length(const String& s) {
    const char* p = s.c_str();
    size_t n = s.length(); // bytes
    size_t i = 0;
    size_t count = 0;
    while (i < n) {
        (void)decode(p, n, i);
        count++;
    }
    return count;
}

// Get the Unicode code point at code point index 'index' (0-based).
// Returns true on success and writes the code point to 'out'.
// Returns false if index is out of range.
static bool codepointAt(const String& s, size_t index, uint32_t& out) {
    const char* p = s.c_str();
    size_t n = s.length();
    size_t i = 0;
    size_t cpIdx = 0;
    while (i < n) {
        uint32_t cp = decode(p, n, i);
        if (cpIdx == index) {
            out = cp;
            return true;
        }
        cpIdx++;
    }
    return false;
}

// Return substring defined by code point start and count.
// If start is beyond the end, returns empty string.
// If count exceeds the available code points, returns up to the end.
static String substringByCodepoints(const String& s, size_t start, size_t count) {
    const char* p = s.c_str();
    size_t n = s.length();
    size_t i = 0;
    size_t cpIdx = 0;

    // Find byte offset for start
    size_t startByte = n;
    while (i < n && cpIdx < start) {
        (void)decode(p, n, i);
        cpIdx++;
    }
    startByte = (cpIdx == start) ? i : n;

    // Advance 'count' code points to find end
    size_t taken = 0;
    while (i < n && taken < count) {
        (void)decode(p, n, i);
        taken++;
    }
    size_t endByte = i;

    if (startByte > n) return String();
    if (endByte < startByte) endByte = startByte;
    return s.substring(startByte, endByte);
}

// Find byte offset of the code point at index 'cpIndex' (useful for low-level ops).
// Returns n (s.length()) if index is out of range (i.e., points past the end).
static size_t byteOffsetOf(const String& s, size_t cpIndex) {
    const char* p = s.c_str();
    size_t n = s.length();
    size_t i = 0;
    size_t cpIdx = 0;
    while (i < n) {
        if (cpIdx == cpIndex) return i;
        (void)decode(p, n, i);
        cpIdx++;
    }
    return n;
}

} // namespace Utf8