/** @file http_pages.cpp @brief Gate 5 split HTTP service module. */
#include "http_api.h"
#include "scpi_api.h"


void handleLiveStatePage() {
  noteHttpRequest();

  Serial.println("HTTP /live");
  Serial.flush();

  String html;
  html.reserve(7000);

  appendCommonPageHeader(html, "E-Resistor Live State");

  html += "<h1>Live State</h1>";
  html += "<p class='small'>This page polls <code>/state</code> every 2 seconds. ";
  html += "Use <a href='/state'>/state</a> directly for plain-text API/debug output.</p>";

  html += "<div class='card'>";
  html += "<h2>Live plain-text state</h2>";
  html += "<pre id='liveState'>Loading...</pre>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Channel masks</h2>";
  html += "<table>";
  html += "<tr><th>Channel</th><th>Mask</th><th>Resistance</th><th>Apply count</th></tr>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<tr><td>CH";
    html += String(ch + 1);
    html += "</td><td><code>";
    html += hex16(channelMask[ch]);
    html += "</code></td><td>";
    html += calculateOutputResistanceText(ch, channelMask[ch]);
    html += "</td><td>";
    html += String(applyCounter[ch]);
    html += "</td></tr>";
  }
  html += "</table>";
  html += "</div>";

  appendLiveStateScript(html);
  appendCommonPageFooter(html);

  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleRoot() {
  noteHttpRequest();

  Serial.println("HTTP /");
  Serial.flush();

  String html;
  html.reserve(10000);

  appendCommonPageHeader(html, "E-Resistor Control");

  html += "<h1>E-Resistor</h1>";


  appendSafetySummaryCard(html);

  html += "<p class='warn'>";
  html += "For first hardware test use only 0000 or 0001. ";
  html += "Do not use FFFF until resistor/MOSFET power path is verified.";
  html += "</p>";

  html += "<div class='grid'>";
  html += "<form method='POST' action='/identify_led' class='metric metric-link' title='Blink WS2812 blue for 5 seconds'>";
  html += "<div class='metric-label'>Device identification</div>";
  html += "<div class='metric-value'>Blue blink 5 s</div>";
  html += "<div class='small'>Click this whole card to identify the connected PCB. WS2812 status LED on GP16 will use a noticeable bright-blue blink pattern.</div>";
  html += "<button type='submit' class='button'>Blink</button></form>";
  html += "<div class='metric'><div class='metric-label'>Firmware / serial</div><div class='metric-value'>";
  html += FIRMWARE_VERSION;
  html += "</div><div class='small'>SN ";
  html += deviceSerialNumber;
  html += "<br>Build ";
  html += FIRMWARE_BUILD_DATE;
  html += " ";
  html += FIRMWARE_BUILD_TIME;
  html += "<br>IP ";
  html += ipToString(eth.localIP());
  html += "<br>HTTP 80 &bull; SCPI 5025";
  html += "</div></div>";
  html += "</div>";

  html += "<h2>Manual channel control</h2>";

  html += "<div class='table-scroll'>";
  html += "<table class='control-table'>";
  html += "<tr>";
  html += "<th>Channel</th>";
  html += "<th>Current mask</th>";
  html += "<th>Active 16-bit indicator<br><span class='small'>bit 15 left, bit 0 right</span></th>";
  html += "<th>Resistance</th>";
  html += "<th>Target resistance</th>";
  html += "<th>New mask and Apply</th>";
  html += "</tr>";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    uint16_t mask = channelMask[ch];

    html += "<tr>";

    html += "<td><b>CH";
    html += String(ch + 1);
    html += "</b></td>";

    html += "<td><code>";
    html += hex16(mask);
    html += "</code></td>";

    html += "<td>";
    appendBitIndicator(html, ch, mask);
    html += "</td>";

    html += "<td><span class='res ";
    html += resistanceCssClass(ch, mask);
    html += "'>";
    html += calculateOutputResistanceText(ch, mask);
    html += "</span></td>";

    html += "<td>";
    html += "<form class='inline-form' action='/target_apply' method='POST'>";
    html += "<input type='hidden' name='ch' value='";
    html += String(ch + 1);
    html += "'>";
    html += "<input class='target' name='target' placeholder='10000000' maxlength='12' inputmode='decimal' title='Target resistance in ohms, for example 10000000 for 10 MOhm'>";
    html += "<button type='submit'>Nearest</button>";
    html += "</form>";
    html += "</td>";

    html += "<td>";
    html += "<form class='manual-mask-form' action='/set' method='POST'>";
    html += "<input type='hidden' name='ch' value='";
    html += String(ch + 1);
    html += "'>";
    html += "<input class='mask' name='mask' value='";
    html += hex16(mask);
    html += "' maxlength='6' pattern='^(0x|0X)?[0-9A-Fa-f]{1,4}$'>";
    html += "<button class='apply' type='submit'>Apply CH";
    html += String(ch + 1);
    html += "</button>";
    html += "</form>";
    html += "</td>";

    html += "</tr>";
  }

  html += "</table>";
  html += "</div>";

  html += "<form action='/alloff' method='POST' style='margin-top:16px;'>";
  html += "<button class='off' type='submit'>FORCE ALL CHANNELS OFF</button>";
  html += "</form>";

  html += "<p class='small'>";
  html += "Resistance is calculated as the parallel equivalent of all active resistor branches. ";
  html += "Click any bit indicator to toggle that bit ON/OFF. ";
  html += "Mask 0x0000 means all MOSFETs OFF and output is OPEN.";
  html += "</p>";

  appendProfileManager(html);

  appendCommonPageFooter(html);

  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void appendNetworkSettingsForm(String& html) {
  html += "<div class='card'>";
  html += "<h2>Ethernet interface</h2>";
  html += "<p class='small'>Default mode is static IP. Changes are stored to LittleFS and used on next boot. Current active IP remains until power-cycle/restart.</p>";

  html += "<div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Active IP</div><div class='metric-value'>";
  html += ipToString(eth.localIP());
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Configured boot IP</div><div class='metric-value'>";
  html += ipToString(DEVICE_IP);
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Mode</div><div class='metric-value'>Static</div></div>";
  html += "<div class='metric'><div class='metric-label'>SCPI port</div><div class='metric-value'>5025</div></div>";
  html += "</div>";

  html += "<form method='POST' action='/network_save'>";
  html += "<table>";
  html += "<tr><th>Setting</th><th>Value</th></tr>";
  html += "<tr><td>Mode</td><td><select name='mode'><option value='static' selected>Static IP</option></select></td></tr>";

  html += "<tr><td>Device IP</td><td><input name='ip' value='";
  html += ipToString(DEVICE_IP);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";

  html += "<tr><td>Subnet mask</td><td><input name='subnet' value='";
  html += ipToString(DEVICE_SUBNET);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";

  html += "<tr><td>Gateway</td><td><input name='gateway' value='";
  html += ipToString(DEVICE_GATEWAY);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";

  html += "<tr><td>DNS</td><td><input name='dns' value='";
  html += ipToString(DEVICE_DNS);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";
  html += "</table>";
  html += "<p><button class='apply' type='submit'>Save Ethernet Settings</button></p>";
  html += "</form>";

  html += "<form method='POST' action='/network_delete' data-confirm='Delete saved Ethernet settings from LittleFS and restore default boot IP after restart?'>";
  html += "<p><button class='off' type='submit'>Delete saved Ethernet settings</button></p>";
  html += "</form>";

  html += "<div class='notice'>";
  html += "<b>Default safe values:</b> IP ";
  html += DEFAULT_DEVICE_IP_TEXT;
  html += ", subnet 255.255.255.0, gateway 0.0.0.0, DNS 0.0.0.0. ";
  html += "If you set an unreachable IP, reflash or erase LittleFS to return to defaults.";
  html += "</div>";

  html += "<h3>Stored network config file</h3>";
  html += "<pre><code>";
  html += networkConfigToText();
  html += "</code></pre>";
  html += "</div>";
}

void handleNetwork() {
  noteHttpRequest();

  Serial.println("HTTP /network");
  Serial.flush();

  String html;
  html.reserve(7000);
  appendCommonPageHeader(html, "E-Resistor Ethernet");

  html += "<h1>Ethernet Settings</h1>";
  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  appendNetworkSettingsForm(html);

  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleRuntimePage() {
  noteHttpRequest();

  String html;
  html.reserve(5000);
  appendCommonPageHeader(html, "E-Resistor Runtime");
  html += "<h1>Runtime Monitor</h1>";
  appendRuntimeMonitorInfo(html);
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleFirmwarePage() {
  noteHttpRequest();

  String html;
  html.reserve(9000);
  appendCommonPageHeader(html, "E-Resistor Firmware");

  html += "<h1>Firmware</h1>";
  html += "<div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Firmware version</div><div class='metric-value'>";
  html += FIRMWARE_VERSION;
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Build</div><div class='metric-value'>";
  html += FIRMWARE_BUILD_DATE;
  html += " ";
  html += FIRMWARE_BUILD_TIME;
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Serial</div><div class='metric-value'>";
  html += deviceSerialNumber;
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Update status</div><div class='metric-value'>";
  html += firmwareUpdateStatus;
  html += "</div></div>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Firmware update by .bin upload</h2>";
  html += "<p class='small'>Upload an Arduino-Pico compiled <code>.bin</code> firmware image. ";
  html += "The firmware will first request all resistor outputs OFF, then write the image and reboot if the update is successful. ";
  html += "Use <code>.uf2</code> only for USB BOOTSEL drag-and-drop flashing; this web page expects <code>.bin</code>.</p>";

  html += "<div class='notice'><b>Safety:</b> Do not update while a calibration run or external test is active. ";
  html += "After pressing Update, the board may stop responding while flash is written and then reboot.</div>";

  uint64_t otaTotalBytes = 0;
  uint64_t otaUsedBytes = 0;
  uint64_t otaFreeBytes = 0;
  html += "<div class='notice'><b>OTA storage:</b> ";
  if (httpGetLittleFsCapacity(otaTotalBytes, otaUsedBytes, otaFreeBytes)) {
    html += "LittleFS total <code>"; html += formatBytesHuman(otaTotalBytes); html += "</code>, used <code>";
    html += formatBytesHuman(otaUsedBytes); html += "</code>, free <code>";
    html += formatBytesHuman(otaFreeBytes); html += "</code>. ";
    html += "The free space must be larger than the uploaded <code>.bin</code> file.";
  } else {
    html += "LittleFS capacity is unavailable. Web firmware update will fail unless the board is compiled with a Flash Size option that includes FS.";
  }
  html += "</div>";

  html += "<form method='POST' action='/firmware_update' enctype='multipart/form-data'>";
  html += "<p><input type='file' name='firmware' accept='.bin,application/octet-stream' required></p>";
  html += "<p><label><input type='checkbox' name='confirm' value='1' required> I confirm outputs may be forced OFF and the board may reboot.</label></p>";
  html += "<p><button class='apply' type='submit'>Upload .bin and update firmware</button></p>";
  html += "</form>";
  html += "</div>";

  html += "<div class='card'><h2>Version SCPI commands</h2><pre><code>";
  html += "*IDN?\n";
  html += "SYST:VERS?\n";
  html += "FIRM:VERS?\n";
  html += "FIRM:BUILD?\n";
  html += "</code></pre></div>";

  html += "<div class='notice'>Calibration and LittleFS file inventory is available on the <a href='/files'>Files tab</a>.</div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleFilesPage() {
  noteHttpRequest();

  String html;
  html.reserve(17500);
  appendCommonPageHeader(html, "E-Resistor Files");
  html += "<h1>Files</h1>";

  // Keep LittleFS capacity and file inventory at the top of this tab.
  appendLittleFsStorageInfo(html);

  html += "<div class='card'><h2>Calibration file readback</h2>";
  html += "<p class='small'>Back up all eight active calibration tables to one text file, or restore all eight tables from a previously exported file.</p>";
  if (server.hasArg("cal_import") && server.arg("cal_import") == "ok") {
    html += "<p class='ok'>All eight calibration tables were imported, saved to LittleFS, and activated.</p>";
  }
  html += "<p><a class='button' href='/calibration_download_all'>Download All</a> ";
  html += "<a class='button' href='/api/calibration/files'>List calibration files</a></p>";

  html += "<form id='calibrationImportForm' method='POST' action='/calibration_import_all' data-max-bytes='";
  html += String(CALIBRATION_BUNDLE_MAX_BYTES);
  html += "'>";
  html += "<p><input id='calibrationImportFile' type='file' accept='.txt,text/plain'> ";
  html += "<button id='calibrationImportButton' type='submit' disabled>Import from file</button></p>";
  html += "<textarea id='calibrationBundleText' name='bundle' style='display:none'></textarea>";
  html += "<p class='small'>Import accepts the single text file produced by Download All. All channels must be OFF. The firmware validates all eight channel blocks before replacing any calibration file.</p>";
  html += "</form>";

  html += "<pre><code>GET /api/calibration/files\nGET /api/calibration/download?ch=1\nGET /api/calibration/download_all</code></pre>";
  html += "</div>";

  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleProfilesPage() {
  noteHttpRequest();

  String html;
  html.reserve(9000);
  appendCommonPageHeader(html, "E-Resistor Profiles");
  html += "<h1>Profiles / Presets</h1>";
  appendProfileManager(html);

  html += "<div class='card'><h2>Current state</h2><table><tr><th>Channel</th><th>Mask</th><th>Resistance</th></tr>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<tr><td>CH";
    html += String(ch + 1);
    html += "</td><td><code>";
    html += hex16(channelMask[ch]);
    html += "</code></td><td>";
    html += calculateOutputResistanceText(ch, channelMask[ch]);
    html += "</td></tr>";
  }
  html += "</table></div>";

  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleSafetyPage() {
  noteHttpRequest();

  String html;
  html.reserve(12000);
  appendCommonPageHeader(html, "E-Resistor Safety");
  html += "<h1>Safety Limits</h1>";
  appendSafetySummaryCard(html);
  html += "<div class='card'>";
  html += "<h2>Per-channel limits</h2>";
  html += "<p class='small'>Each channel is checked against its own limits before a mask is applied or a nearest resistance is selected.</p>";
  html += "<form method='POST' action='/safety_save'>";
  html += "<div class='table-scroll'><table class='safety-table'><tr><th>Setting</th>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<th>CH"; html += String(ch + 1); html += "</th>";
  }
  html += "</tr>";

  html += "<tr><td>Minimum allowed calculated resistance, Ohm</td>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<td><input type='number' min='0.001' step='any' name='min_ohm_ch";
    html += String(ch + 1); html += "' value='";
    html += String(safetyMinOhm[ch], 3); html += "' required></td>";
  }
  html += "</tr>";

  html += "<tr><td>Maximum allowed calculated resistance, Ohm</td>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<td><input type='number' min='0.001' step='any' name='max_ohm_ch";
    html += String(ch + 1); html += "' value='";
    html += String(safetyMaxOhm[ch], 3); html += "' required></td>";
  }
  html += "</tr>";

  html += "<tr><td>Maximum active bits per channel</td>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<td><input type='number' min='1' max='";
    html += String(BIT_COUNT); html += "' step='1' name='max_bits_ch";
    html += String(ch + 1); html += "' value='";
    html += String(safetyMaxActiveBits[ch]); html += "' required></td>";
  }
  html += "</tr></table></div>";

  html += "<p><label>Expert mode / bypass all channel safety checks: <select name='expert'><option value='0'";
  html += safetyExpertMode ? "" : " selected";
  html += ">OFF</option><option value='1'";
  html += safetyExpertMode ? " selected" : "";
  html += ">ON</option></select></label></p>";
  html += "<p><button type='submit'>Save safety settings</button></p></form></div>";
  html += "<div class='notice'>Recommended defaults for every channel: minimum 300 Ohm, maximum 20 MOhm, maximum 16 active bits, expert OFF. Lower limits can stress resistors or MOSFETs depending on applied voltage.</div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleScpiPage() {
  noteHttpRequest();
  String html;
  html.reserve(8000);
  appendCommonPageHeader(html, "E-Resistor SCPI");
  html += "<h1>SCPI Interface</h1><div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Address</div><div class='metric-value'>";
  html += ipToString(eth.localIP()); html += ":5025</div></div>";
  html += "<div class='metric'><div class='metric-label'>Client</div><div class='metric-value'>";
  html += (scpiClient && scpiClient.connected()) ? "connected" : "not connected";
  html += "</div></div><div class='metric'><div class='metric-label'>Command count</div><div class='metric-value'>";
  html += String(scpiCommandCount); html += "</div></div></div>";
  html += "<div class='card'><h2>Supported commands</h2>";
  html += "<p class='small'>This table and the TCP HELP? response are generated from the same fixed-buffer command registry.</p>";
  html += "<div class='table-scroll'><table class='scpi-command-table'><tr><th>Command</th><th>Description</th></tr>";
  size_t commandCount = 0;
  const ScpiCommandDefinition* commands = scpiCommandRegistry(commandCount);
  for (size_t i = 0; i < commandCount; ++i) {
    if (commands[i].alias || !commands[i].helpText) continue;
    String command(commands[i].command);
    command.replace("&", "&amp;"); command.replace("<", "&lt;"); command.replace(">", "&gt;");
    html += "<tr><td><code>"; html += command; html += "</code></td><td>";
    html += commands[i].helpText; html += "</td></tr>";
  }
  html += "</table></div><p class='small'>Use channels 1 to 8. ROUTe:CHANnel aliases remain accepted.</p></div>";
  html += "<div class='card'><h2>Last SCPI command</h2><pre><code>";
  html += lastScpiCommand; html += "</code></pre></div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleLogPage() {
  noteHttpRequest();

  String html;
  html.reserve(8000);
  appendCommonPageHeader(html, "E-Resistor Event Log");
  html += "<h1>Event Log</h1>";
  html += "<p><a class='button' href='/log_download'>Export log as TXT</a></p>";
  html += "<div class='card'><h2>Boot status</h2>";
  html += "<table>";
  html += "<tr><th>Item</th><th>Status</th></tr>";
  html += "<tr><td>Boot status text</td><td><code>"; html += statusText; html += "</code></td></tr>";
  html += "<tr><td>Firmware version</td><td><code>"; html += FIRMWARE_VERSION; html += "</code></td></tr>";
  html += "<tr><td>Firmware build</td><td><code>"; html += FIRMWARE_BUILD_DATE; html += " "; html += FIRMWARE_BUILD_TIME; html += "</code></td></tr>";
  html += "<tr><td>Board serial</td><td><code>"; html += deviceSerialNumber; html += "</code></td></tr>";
  html += "<tr><td>Firmware serial</td><td><code>"; html += FIRMWARE_VERSION; html += "-"; html += deviceSerialNumber; html += "</code></td></tr>";
  html += "<tr><td>Uptime</td><td><code>"; html += String((millis() - bootMillis) / 1000UL); html += " s</code></td></tr>";
  html += "<tr><td>Core 1 engine</td><td><code>"; html += core1EngineReady ? "ready" : "not ready"; html += "</code></td></tr>";
  html += "<tr><td>Outputs known safe</td><td><code>"; html += outputsKnownSafe ? "yes" : "no"; html += "</code></td></tr>";
  html += "<tr><td>LittleFS</td><td><code>"; html += littleFsReady ? "ready" : "not ready"; html += "</code></td></tr>";
  html += "<tr><td>Ethernet fault</td><td><code>"; html += ethernetFault ? "yes" : "no"; html += "</code></td></tr>";
  html += "<tr><td>IP address</td><td><code>"; html += ipToString(eth.localIP()); html += "</code></td></tr>";
  html += "<tr><td>W5500 VERSIONR</td><td><code>0x"; html += String(w5500Version, HEX); html += "</code></td></tr>";
  html += "<tr><td>WS2812 heartbeat</td><td><code>GP16</code></td></tr>";
  html += "</table></div>";
  html += "<div class='card'><h2>Event history</h2><pre><code>";

  for (uint8_t i = 0; i < eventLogCount; i++) {
    uint8_t index = (eventLogHead + 32U - eventLogCount + i) % 32U;
    html += eventLog[index];
    html += "\n";
  }

  if (eventLogCount == 0) {
    html += "No events yet.\n";
  }

  html += "</code></pre></div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleBackupPage() {
  noteHttpRequest();

  String html;
  html.reserve(7000);
  appendCommonPageHeader(html, "E-Resistor Backup");
  html += "<h1>Backup / Factory Reset</h1>";
  html += "<div class='card'><h2>Export</h2><p><a class='tab' href='/backup_download'>Download all text configuration</a></p></div>";
  html += "<div class='card'><h2>Factory reset</h2>";
  html += "<p class='warn'>Factory reset deletes stored LittleFS config files. Current running RAM state may remain until reboot.</p>";
  html += "<div class='factory-grid'>";
  html += "<form method='POST' action='/factory_reset'><input type='hidden' name='scope' value='calibration'><button class='button off' type='submit'>Delete calibration files</button></form>";
  html += "<form method='POST' action='/factory_reset'><input type='hidden' name='scope' value='profiles'><button class='button off' type='submit'>Delete profiles</button></form>";
  html += "<form method='POST' action='/factory_reset'><input type='hidden' name='scope' value='network'><button class='button off' type='submit'>Delete network config</button></form>";
  html += "<form method='POST' action='/factory_reset'><input type='hidden' name='scope' value='all'><button class='button off' type='submit'>FULL FACTORY RESET</button></form>";
  html += "</div>";
  html += "</div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleWatchdogPage() {
  noteHttpRequest();

  String html;
  html.reserve(5000);
  appendCommonPageHeader(html, "E-Resistor Watchdog");
  html += "<h1>Watchdog / Uptime</h1>";
  html += "<div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Uptime</div><div class='metric-value'>";
  html += String((millis() - bootMillis) / 1000UL);
  html += " s</div></div>";
  html += "<div class='metric'><div class='metric-label'>Watchdog</div><div class='metric-value'>not enabled</div></div>";
  html += "</div>";
  html += "<div class='notice'>Watchdog page is prepared. I kept watchdog disabled by default to avoid unwanted resets while debugging Ethernet/LittleFS. It can be enabled later after the firmware is stable.</div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

