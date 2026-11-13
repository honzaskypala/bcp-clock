// Shared look of the device web pages (captive portal and config page)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>

namespace WebUI {

// Start of a page: head with the common CSS plus pageCss, body and the title banner
String buildPageStart(const String& title, const char *pageCss = "");
String buildPageEnd();

// Section heading with a separator line above it; buttonHtml is shown at the right end.
// The first section right below the banner has no separator line.
String buildSectionHeader(const String& title, const String& buttonHtml = "", bool first = false);

String escapeHtml(const String& in);

} // namespace WebUI
