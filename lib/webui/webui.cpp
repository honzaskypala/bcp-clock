// Shared look of the device web pages (captive portal and config page)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "webui.h"

namespace WebUI {

static const char COMMON_CSS[] =
    "html,body{margin:0;padding:0;background:#fff;color:#111;font-family:Arial,Helvetica,sans-serif}"
    ".wrap{padding:18px;max-width:820px;margin:0 auto}"
    "h1{background:#5b1b66;color:#fff;font-weight:800;text-align:center;padding:14px 10px;margin:18px 0;font-size:36px;line-height:1.15;letter-spacing:.5px}"
    ".status{font-size:18px;font-weight:600;text-align:center;color:#fff;padding:10px 12px;border-radius:4px;margin:0 0 18px}"
    ".status.ok{background:#2e7d32}"
    ".status.error{background:#c3002f}"
    ".secHeader{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;margin:28px 0 12px;padding-top:18px;border-top:1px solid #000}"
    ".secHeader.first{margin-top:0;padding-top:0;border-top:0}"
    ".secHeader h2{margin:0;font-size:22px}"
    ".row{display:flex;align-items:center;gap:16px;margin:14px 0}"
    ".row label{flex:0 0 120px;font-size:20px}"
    "input[type=text],input[type=password]{flex:1 1 auto;width:100%;font-size:18px;padding:10px 12px;border:1px solid #888;border-radius:4px;box-sizing:border-box}"
    ".actions{margin-top:26px;display:flex;justify-content:center}"
    ".btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;font-size:14px;font-weight:600;padding:6px 14px;border:1px solid #777;border-radius:4px;background:#eaeaea;color:#111;text-decoration:none;cursor:pointer}"
    ".btn:hover{filter:brightness(.95)}"
    ".btn:disabled{opacity:.65;cursor:default}"
    ".btnMain{min-width:60%;max-width:600px;font-size:26px;font-weight:700;padding:14px 24px}"
    ".btnDanger{color:#c3002f;font-weight:700}"
    ".storedRow{display:flex;justify-content:space-between;align-items:center;border:1px solid #ccc;border-radius:4px;padding:8px 12px;margin-bottom:8px}"
    ".storedRow:nth-child(even){background:#f0f9ff}"
    ".storedSSID{font-size:16px;overflow:hidden;text-overflow:ellipsis}"
    "a.del{text-decoration:none;font-size:22px;line-height:1}"
    "a.del:hover{filter:brightness(.85)}"
    "@media (max-width:580px){"
        "h1{font-size:28px;padding:12px 8px;margin:8px 0 14px}"
        ".row{flex-wrap:wrap;gap:6px}"
        ".row label{flex:0 0 100%}"
        ".btnMain{width:100%;min-width:0}"
    "}";

String buildPageStart(const String& title, const char *pageCss) {
    String page;
    page.reserve(4096);
    page += "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'><title>";
    page += escapeHtml(title);
    page += "</title><style>";
    page += COMMON_CSS;
    page += pageCss;
    page += "</style></head><body><div class='wrap'><h1>";
    page += escapeHtml(title);
    page += "</h1>";
    return page;
}

String buildPageEnd() {
    return "</div></body></html>";
}

String buildSectionHeader(const String& title, const String& buttonHtml, bool first) {
    return String("<div class='secHeader") + (first ? " first" : "") + "'><h2>" + escapeHtml(title) + "</h2>" +
           buttonHtml + "</div>";
}

String escapeHtml(const String& in) {
    String out;
    out.reserve(in.length() + 10);
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

} // namespace WebUI
