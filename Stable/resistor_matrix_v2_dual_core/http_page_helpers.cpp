/**
 * @file http_page_helpers.cpp
 * @brief Shared HTML page rendering using flash-backed Gate 5 assets.
 */
#include "http_api.h"

static const char* kNavigationHtml =
  "<nav class='tabs'>"
  "<a class='tab' href='/'>Control</a>"
  "<a class='tab' href='/settings'>Calibration</a>"
  "<a class='tab' href='/profiles'>Profiles</a>"
  "<a class='tab' href='/safety'>Safety</a>"
  "<a class='tab' href='/network'>Ethernet</a>"
  "<a class='tab' href='/runtime'>Runtime</a>"
  "<a class='tab' href='/scpi'>SCPI</a>"
  "<a class='tab' href='/files'>Files</a>"
  "<a class='tab' href='/firmware'>Firmware</a>"
  "<a class='tab' href='/log'>Log</a>"
  "<a class='tab' href='/backup'>Backup</a>"
  "<a class='tab' href='/watchdog'>Watchdog</a>"
  "<a class='tab' href='/live'>Live State</a>"
  "</nav>";

void appendCommonPageHeader(String& html, const char* title) {
  html += "<!doctype html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='Cache-Control' content='no-store'>";
  html += "<title>"; html += title; html += "</title>";
  html += "<link rel='stylesheet' href='/assets/app.css?v="; html += FIRMWARE_VERSION; html += "'>";
  html += "<script defer src='/assets/app.js?v="; html += FIRMWARE_VERSION; html += "'></script>";
  html += "</head><body><header class='topbar'><div class='topbar-inner'>";
  html += "<div class='brand'><span class='dot'></span><span>E-Resistor</span><span class='serial'>SN ";
  html += deviceSerialNumber;
  html += "</span><span class='serial'>FW "; html += FIRMWARE_VERSION;
  html += "</span><span class='serial'>Build "; html += FIRMWARE_BUILD_DATE; html += "</span></div>";
  html += kNavigationHtml;
  html += "</div></header><main class='page'>";
}

void appendCommonPageFooter(String& html) {
  html += "</main></body></html>";
}

void streamCommonPageHeader(HttpResponseWriter& out, const char* title) {
  out.write("<!doctype html><html><head><meta charset='utf-8'>");
  out.write("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  out.write("<meta http-equiv='Cache-Control' content='no-store'><title>");
  out.write(title);
  out.write("</title><link rel='stylesheet' href='/assets/app.css?v=");
  out.write(FIRMWARE_VERSION);
  out.write("'><script defer src='/assets/app.js?v=");
  out.write(FIRMWARE_VERSION);
  out.write("'></script></head><body><header class='topbar'><div class='topbar-inner'>");
  out.write("<div class='brand'><span class='dot'></span><span>E-Resistor</span><span class='serial'>SN ");
  out.write(deviceSerialNumber);
  out.write("</span><span class='serial'>FW ");
  out.write(FIRMWARE_VERSION);
  out.write("</span><span class='serial'>Build ");
  out.write(FIRMWARE_BUILD_DATE);
  out.write("</span></div>");
  out.write(kNavigationHtml);
  out.write("</div></header><main class='page'>");
}

void streamCommonPageFooter(HttpResponseWriter& out) {
  out.write("</main></body></html>");
}
