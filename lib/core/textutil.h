// String helpers: time thresholds, hostname, stored WiFi list, hashing
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>
#include <vector>

// ---- Durations ("h:mm:ss", "m:ss", "m.ss" or minutes; a leading "-" negates) ----
String secondsToTime(long seconds);
long timeToSeconds(const String& time);

// ---- Hostname ----
// Keep only characters valid in a hostname (letters, digits, inner hyphens), fall back to the default
String sanitizeHostname(const String& name);

// ---- Newline separated list ----
void splitList(const String& src, std::vector<String>& out);
String joinList(const std::vector<String>& list);

// ---- Hashing ----
uint32_t fnv1a32(const uint8_t *data, size_t len, uint32_t h = 2166136261UL);
uint32_t fnv1a32(const String& s);
String makePassKey(const String& ssid);   // NVS key of a stored WiFi password: "pwd_<hash hex>"
