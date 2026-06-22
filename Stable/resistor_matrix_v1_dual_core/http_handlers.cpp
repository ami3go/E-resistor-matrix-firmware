/**
 * @file http_handlers.cpp
 * @brief HTTP web UI, JSON-like status endpoint, settings pages, profiles, safety, SCPI help, and command handlers.
 */

#include "app.h"

// 11_http_handlers.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// HTTP handlers
// ============================================================

/**
 * @brief Handle Ping.
 */
void handlePing() {
  noteHttpRequest();

  Serial.println("HTTP /ping");
  Serial.flush();

  sendNoCacheHeaders();
  server.send(200, "text/plain", "pong\n");
}

/**
 * @brief Return the current machine state as a compact text/JSON-like response.
 */
void handleState() {
  noteHttpRequest();

  Serial.println("HTTP /state");
  Serial.flush();

  String text;
  text.reserve(1600);

  text += "status=";
  text += statusText;
  text += "\n";

  text += "ip=";
  text += ipToString(eth.localIP());
  text += "\n";

  text += "w5500_version=0x";
  text += String(w5500Version, HEX);
  text += "\n";

  text += "ws2812_gp=16\n";
  text += "scpi_port=5025\n";
  text += "littlefs=";
  text += littleFsReady ? "ready" : "not_ready";
  text += "\n";

  text += "shift_registers_ready=";
  text += shiftRegistersReady ? "1" : "0";
  text += "\n";

  text += "outputs_known_safe=";
  text += outputsKnownSafe ? "1" : "0";
  text += "\n";

  text += "fatal_safe_state=";
  text += fatalSafeStateActive ? "1" : "0";
  text += "\n";

  text += "cpu_mhz=";
  text += String(F_CPU / 1000000UL);
  text += "\n";

  text += "heap_total_bytes=";
  text += String(getHeapTotalBytes());
  text += "\n";

  text += "heap_used_bytes=";
  text += String(getHeapUsedBytes());
  text += "\n";

  text += "heap_free_bytes=";
  text += String(getHeapFreeBytes());
  text += "\n";

  text += "heap_used_percent=";
  text += String(getHeapUsedPercent(), 1);
  text += "\n";

  text += "core0_load_percent=";
  text += String(runtimeCore0LoadPct, 1);
  text += "\n";

  text += "loop_rate_hz=";
  text += String(runtimeLoopsPerSecond, 1);
  text += "\n";

  text += "loop_busy_max_us=";
  text += String(runtimeLoopMaxUs);
  text += "\n";

  text += "http_request_count=";
  text += String(httpRequestCount);
  text += "\n";

  text += "scpi_command_count=";
  text += String(scpiCommandCount);
  text += "\n";

  text += "core1_state=";
  text += core1EngineReady ? "ready" : "not_ready";
  text += "\n";

  text += "core1_outputs_ready=";
  text += core1OutputsReady ? "1" : "0";
  text += "\n";

  text += "core1_fault=";
  text += core1Fault ? "1" : "0";
  text += "\n";

  text += "core1_heartbeat_ms=";
  text += String((uint32_t)core1HeartbeatMs);
  text += "\n";

  text += "core1_loop_count=";
  text += String((uint32_t)core1LoopCounter);
  text += "\n";

  text += "core1_command_count=";
  text += String((uint32_t)core1CommandCounter);
  text += "\n";

  text += "core1_queue_overflow_count=";
  text += String((uint32_t)core1QueueOverflowCounter);
  text += "\n";

  text += "configured_ip=";
  text += ipToString(DEVICE_IP);
  text += "\n";

  text += "configured_subnet=";
  text += ipToString(DEVICE_SUBNET);
  text += "\n";

  text += "configured_gateway=";
  text += ipToString(DEVICE_GATEWAY);
  text += "\n";

  text += "configured_dns=";
  text += ipToString(DEVICE_DNS);
  text += "\n";

  text += "safety_min_ohm=";
  text += String(safetyMinOhm, 3);
  text += "\n";

  text += "safety_max_active_bits=";
  text += String(safetyMaxActiveBits);
  text += "\n";

  text += "expert_mode=";
  text += safetyExpertMode ? "1" : "0";
  text += "\n";

  text += "last_scpi_command=";
  text += lastScpiCommand;
  text += "\n\n";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    text += "ch";
    text += String(ch + 1);
    text += "=";
    text += hex16(channelMask[ch]);
    text += " resistance=";
    text += calculateOutputResistanceText(ch, channelMask[ch]);
    text += " count=";
    text += String(applyCounter[ch]);
    text += "\n";
  }

  sendNoCacheHeaders();
  server.send(200, "text/plain", text);
}


/**
 * @brief Handle Live State Page.
 */
void handleLiveStatePage() {
  noteHttpRequest();

  Serial.println("HTTP /live");
  Serial.flush();

  String html;
  html.reserve(7000);

  appendCommonPageHeader(html, "RP2040 Matrix Live State");

  html += "<h1>Live State</h1>";
  html += "<p class='small'>This page polls <code>/state</code> every 2 seconds. ";
  html += "Use <a href='/state'>/state</a> directly for plain-text API/debug output.</p>";

  html += "<div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Status</div><div class='metric-value'>";
  html += statusText;
  html += "</div></div>";

  html += "<div class='metric'><div class='metric-label'>IP</div><div class='metric-value'>";
  html += ipToString(eth.localIP());
  html += "</div></div>";

  html += "<div class='metric'><div class='metric-label'>SCPI Port</div><div class='metric-value'>5025</div></div>";

  html += "<div class='metric'><div class='metric-label'>Uptime</div><div class='metric-value'>";
  html += String((millis() - bootMillis) / 1000UL);
  html += " s</div></div>";

  html += "<div class='metric'><div class='metric-label'>LittleFS</div><div class='metric-value'>";
  html += littleFsReady ? "ready" : "not ready";
  html += "</div></div>";

  html += "<div class='metric'><div class='metric-label'>W5500</div><div class='metric-value'>0x";
  html += String(w5500Version, HEX);
  html += "</div></div>";
  html += "</div>";

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


/**
 * @brief Handle Root.
 */
void handleRoot() {
  noteHttpRequest();

  Serial.println("HTTP /");
  Serial.flush();

  String html;
  html.reserve(10000);

  appendCommonPageHeader(html, "RP2040 Matrix Control");

  html += "<h1>RP2040 Resistor Matrix</h1>";

  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  html += "<p>IP: <code>";
  html += ipToString(eth.localIP());
  html += "</code>, HTTP: <code>80</code>, SCPI: <code>5025</code></p>";

  html += "<p>WS2812 heartbeat: <code>GP16</code></p>";

  appendSafetySummaryCard(html);

  html += "<p class='warn'>";
  html += "For first hardware test use only 0000 or 0001. ";
  html += "Do not use FFFF until resistor/MOSFET power path is verified.";
  html += "</p>";

  html += "<h2>Manual channel control</h2>";

  html += "<div class='table-scroll'>";
  html += "<table class='control-table'>";
  html += "<tr>";
  html += "<th>Channel</th>";
  html += "<th>Current mask</th>";
  html += "<th>Active 16-bit indicator<br><span class='small'>bit 15 left, bit 0 right</span></th>";
  html += "<th>Calculated output resistance</th>";
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
    html += "<form class='inline-form' action='/target_apply' method='GET'>";
    html += "<input type='hidden' name='ch' value='";
    html += String(ch + 1);
    html += "'>";
    html += "<input class='target' name='target' placeholder='10000' title='Target resistance in ohms'>";
    html += "<button type='submit'>Nearest</button>";
    html += "</form>";
    html += "</td>";

    html += "<td>";
    html += "<form action='/set' method='GET'>";
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

  html += "<form action='/alloff' method='GET' style='margin-top:16px;'>";
  html += "<button class='off' type='submit'>FORCE ALL CHANNELS OFF</button>";
  html += "</form>";

  html += "<p class='small'>";
  html += "Resistance is calculated as the parallel equivalent of all active resistor branches. ";
  html += "Click any bit indicator to toggle that bit ON/OFF. ";
  html += "Mask 0x0000 means all MOSFETs OFF and output is OPEN.";
  html += "</p>";

  appendProfileManager(html);

  html += "<h2>Current channel resistor tables</h2>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<h3>CH";
    html += String(ch + 1);
    html += "</h3>";
    html += "<table>";
    html += "<tr><th>Bit</th><th>Branch</th><th>Resistance</th></tr>";

    for (uint8_t i = 0; i < BIT_COUNT; i++) {
      html += "<tr><td>";
      html += String(channelResistorTable[ch][i].bit);
      html += "</td><td>";
      html += channelResistorTable[ch][i].mosfet_name;
      html += "</td><td>";
      html += channelResistorTable[ch][i].nominal_resistance;
      html += "</td></tr>";
    }

    html += "</table>";
  }

  appendCommonPageFooter(html);

  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}


/**
 * @brief Append Network Settings Form.
 * @param html HTML string that receives generated markup.
 */
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

  html += "<div class='notice'>";
  html += "<b>Default safe values:</b> IP 192.168.7.50, subnet 255.255.255.0, gateway 0.0.0.0, DNS 0.0.0.0. ";
  html += "If you set an unreachable IP, reflash or erase LittleFS to return to defaults.";
  html += "</div>";

  html += "<h3>Stored network config file</h3>";
  html += "<pre><code>";
  html += networkConfigToText();
  html += "</code></pre>";
  html += "</div>";
}

/**
 * @brief Handle Network.
 */
void handleNetwork() {
  noteHttpRequest();

  Serial.println("HTTP /network");
  Serial.flush();

  String html;
  html.reserve(7000);
  appendCommonPageHeader(html, "RP2040 Matrix Ethernet");

  html += "<h1>Ethernet Settings</h1>";
  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  appendNetworkSettingsForm(html);

  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

/**
 * @brief Handle Network Save.
 */
void handleNetworkSave() {
  noteHttpRequest();

  Serial.println("HTTP /network_save");
  Serial.flush();

  IPAddress newIp;
  IPAddress newSubnet;
  IPAddress newGateway;
  IPAddress newDns;

  if (!server.hasArg("ip") || !parseIpAddressText(server.arg("ip"), newIp)) {
    server.send(400, "text/plain", "Bad IP address\n");
    return;
  }

  if (!server.hasArg("subnet") || !parseIpAddressText(server.arg("subnet"), newSubnet)) {
    server.send(400, "text/plain", "Bad subnet mask\n");
    return;
  }

  if (!server.hasArg("gateway") || !parseIpAddressText(server.arg("gateway"), newGateway)) {
    server.send(400, "text/plain", "Bad gateway\n");
    return;
  }

  if (!server.hasArg("dns") || !parseIpAddressText(server.arg("dns"), newDns)) {
    server.send(400, "text/plain", "Bad DNS\n");
    return;
  }

  DEVICE_IP = newIp;
  DEVICE_SUBNET = newSubnet;
  DEVICE_GATEWAY = newGateway;
  DEVICE_DNS = newDns;

  bool saved = saveNetworkConfigToLittleFS();

  if (saved) {
    setStatus("Ethernet settings saved. Restart required.");
    clearLastError();
  } else {
    setStatus("Ethernet settings changed in RAM only. LittleFS save failed.");
    setLastError("Ethernet config save failed");
  }

  appendLogEvent("Ethernet settings saved");

  server.sendHeader("Location", "/network", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Handle Runtime Page.
 */
void handleRuntimePage() {
  noteHttpRequest();

  String html;
  html.reserve(5000);
  appendCommonPageHeader(html, "RP2040 Matrix Runtime");
  html += "<h1>Runtime Monitor</h1>";
  appendRuntimeMonitorInfo(html);
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

/**
 * @brief Handle Files Page.
 */
void handleFilesPage() {
  noteHttpRequest();

  String html;
  html.reserve(9000);
  appendCommonPageHeader(html, "RP2040 Matrix Files");
  html += "<h1>LittleFS Files</h1>";
  appendLittleFsStorageInfo(html);
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

/**
 * @brief Handle Settings.
 */
void handleSettings() {
  noteHttpRequest();

  Serial.println("HTTP /settings");
  Serial.flush();

  String selectedChStr = server.hasArg("ch") ? server.arg("ch") : "1";
  int selectedCh = selectedChStr.toInt();
  if (selectedCh < 1 || selectedCh > 8) {
    selectedCh = 1;
  }

  uint8_t chIndex = uint8_t(selectedCh - 1);
  String currentConfig = channelConfigToText(chIndex);

  String html;
  html.reserve(13000);

  appendCommonPageHeader(html, "RP2040 Matrix Settings");

  html += "<h1>Settings</h1>";

  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  html += "<div class='card'>";

  html += "<h2>Upload per-channel ResistorBitInfo table</h2>";
  html += "<p class='small'>";
  html += "Runtime firmware cannot compile a C++ .h file after upload. ";
  html += "This page accepts either CSV lines or copied C++ initializer lines from a header file, ";
  html += "then stores the parsed values as the channel runtime resistor table.";
  html += "</p>";

  html += "<form method='GET' action='/settings'>";
  html += "View channel: <select name='ch'>";
  for (uint8_t ch = 1; ch <= CHANNEL_COUNT; ch++) {
    html += "<option value='";
    html += String(ch);
    html += "'";
    if (ch == selectedCh) {
      html += " selected";
    }
    html += ">CH";
    html += String(ch);
    html += "</option>";
  }
  html += "</select>";
  html += "<button type='submit'>Load</button>";
  html += "</form>";

  html += "<form method='POST' action='/upload_config'>";
  html += "<p>Upload to channel: <select name='ch'>";
  for (uint8_t ch = 1; ch <= CHANNEL_COUNT; ch++) {
    html += "<option value='";
    html += String(ch);
    html += "'";
    if (ch == selectedCh) {
      html += " selected";
    }
    html += ">CH";
    html += String(ch);
    html += "</option>";
  }
  html += "</select></p>";

  html += "<p>";
  html += "<input type='file' id='fileInput' accept='.csv,.h,.txt'>";
  html += "<span class='small'>Choose a CSV/.h file; browser will load it into the text box before upload.</span>";
  html += "</p>";

  html += "<textarea id='configText' name='config'>";
  html += currentConfig;
  html += "</textarea>";

  html += "<p>";
  html += "<button class='apply' type='submit'>Upload / Save Channel Config</button>";
  html += "</p>";
  html += "</form>";

  html += "<h2>Expected CSV format</h2>";
  html += "<pre><code>";
  html += "bit,mosfet_name,nominal_resistance\n";
  html += "0,Q16,626R\n";
  html += "1,Q15,1.24k\n";
  html += "...\n";
  html += "15,Q1,20M\n";
  html += "</code></pre>";

  html += "<h2>Calibration metadata</h2>";
  html += "<div class='card'><form method='POST' action='/meta_save'>";
  html += "<p>Channel: <select name='ch'>";
  for (uint8_t ch = 1; ch <= CHANNEL_COUNT; ch++) {
    html += "<option value='";
    html += String(ch);
    html += "'";
    if (ch == selectedCh) {
      html += " selected";
    }
    html += ">CH";
    html += String(ch);
    html += "</option>";
  }
  html += "</select></p>";
  html += "<p><input name='date' placeholder='Calibration date YYYY-MM-DD'> ";
  html += "<input name='valid_until' placeholder='Valid until YYYY-MM-DD'></p>";
  html += "<p><input name='operator' placeholder='Operator'> ";
  html += "<input name='dmm' placeholder='DMM model, e.g. HP 34401A'></p>";
  html += "<p><input name='report_id' placeholder='Report ID / PDF filename'> ";
  html += "<button type='submit'>Save metadata</button></p>";
  html += "</form></div>";

  html += "<h2>Accepted .h initializer line format</h2>";
  html += "<pre><code>";
  html += "{0,  \"Q16\", \"626R\"},\n";
  html += "{1,  \"Q15\", \"1.24k\"},\n";
  html += "...\n";
  html += "{15, \"Q1\",  \"20M\"},\n";
  html += "</code></pre>";

  html += "<script>";
  html += "document.getElementById('fileInput').addEventListener('change', function(evt){";
  html += "var f=evt.target.files[0]; if(!f) return;";
  html += "var r=new FileReader();";
  html += "r.onload=function(e){document.getElementById('configText').value=e.target.result;};";
  html += "r.readAsText(f);";
  html += "});";
  html += "</script>";

  html += "</div>";

  appendCommonPageFooter(html);

  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

/**
 * @brief Handle Upload Config.
 */
void handleUploadConfig() {
  noteHttpRequest();

  Serial.println("HTTP /upload_config");
  Serial.flush();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!server.hasArg("config")) {
    server.send(400, "text/plain", "Missing config text\n");
    return;
  }

  String configText = server.arg("config");
  char error[96];

  if (!parseChannelConfigText(ch, configText, error, sizeof(error))) {
    setStatus(error);
    setLastError(error);
    server.send(400, "text/plain", String("Config parse failed: ") + error + "\n");
    return;
  }

  bool saved = saveChannelConfigToLittleFS(ch);

  char msg[128];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u config uploaded%s",
    unsigned(ch + 1),
    saved ? " and saved" : " to RAM only"
  );

  setStatus(msg);
  clearLastError();

  Serial.println(msg);
  Serial.flush();
  appendLogEvent(msg);

  redirectToSettings();
}

/**
 * @brief Handle Upload Config Api.
 */
void handleUploadConfigApi() {
  noteHttpRequest();

  Serial.println("HTTP API calibration upload");
  Serial.flush();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "ERR,bad_channel\n");
    return;
  }

  String configText;

  if (server.hasArg("config")) {
    configText = server.arg("config");
  } else if (server.hasArg("plain")) {
    configText = server.arg("plain");
  } else {
    server.send(400, "text/plain", "ERR,missing_config\n");
    return;
  }

  char error[96];

  if (!parseChannelConfigText(ch, configText, error, sizeof(error))) {
    setStatus(error);
    setLastError(error);
    server.send(400, "text/plain", String("ERR,parse,") + error + "\n");
    return;
  }

  bool saved = saveChannelConfigToLittleFS(ch);

  char msg[128];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u calibration uploaded by API%s",
    unsigned(ch + 1),
    saved ? " and saved" : " to RAM only"
  );

  setStatus(msg);
  clearLastError();
  appendLogEvent(msg);

  Serial.println(msg);
  Serial.flush();

  String response;
  response.reserve(220);
  response += "OK\n";
  response += "channel=";
  response += String(ch + 1);
  response += "\n";
  response += "saved=";
  response += saved ? "1" : "0";
  response += "\n";
  response += "serial=";
  response += deviceSerialNumber;
  response += "\n";
  response += "path=";
  response += channelConfigPath(ch);
  response += "\n";

  server.send(200, "text/plain", response);
}


/**
 * @brief Handle Download Config.
 */
void handleDownloadConfig() {
  noteHttpRequest();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  sendNoCacheHeaders();
  server.send(200, "text/plain", channelConfigToText(ch));
}


/**
 * @brief Handle Toggle Bit.
 */
void handleToggleBit() {
  noteHttpRequest();

  uint8_t ch = 0;
  uint8_t bit = 0;

  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!parseBit(bit)) {
    server.send(400, "text/plain", "Bad bit. Use bit=0..15\n");
    return;
  }

  uint16_t oldMask = channelMask[ch];
  uint16_t newMask = oldMask ^ (uint16_t(1) << bit);

  char safetyReason[128];
  if (!checkMaskSafety(ch, newMask, safetyReason, sizeof(safetyReason))) {
    setLedMode(LED_FAULT);
    setLastError(safetyReason);
    server.send(400, "text/plain", String(safetyReason) + "\n");
    return;
  }

  bool ok = applyChannelMask(ch, newMask);

  if (!ok) {
    setLedMode(LED_FAULT);
    server.send(500, "text/plain", "Bit toggle apply failed\n");
    return;
  }

  char msg[128];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u bit %u toggled %s: 0x%04X -> 0x%04X",
    unsigned(ch + 1),
    unsigned(bit),
    (newMask & (uint16_t(1) << bit)) ? "ON" : "OFF",
    unsigned(oldMask),
    unsigned(newMask)
  );

  setStatus(msg);
  appendLogEvent(msg);
  clearLastError();
  setLedMode(LED_OK);

  redirectToRoot();
}

/**
 * @brief Handle Target Apply.
 */
void handleTargetApply() {
  noteHttpRequest();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!server.hasArg("target")) {
    server.send(400, "text/plain", "Missing target resistance in ohms\n");
    return;
  }

  double targetOhm = server.arg("target").toFloat();
  if (targetOhm <= 0.0) {
    server.send(400, "text/plain", "Bad target resistance\n");
    return;
  }

  uint16_t mask = 0;
  double calcOhm = 0.0;
  double errPct = 0.0;

  if (!findNearestMaskForTarget(ch, targetOhm, mask, calcOhm, errPct)) {
    server.send(400, "text/plain", "No safe mask found for target\n");
    return;
  }

  char reason[128];
  if (!checkMaskSafety(ch, mask, reason, sizeof(reason))) {
    server.send(400, "text/plain", String(reason) + "\n");
    return;
  }

  if (!applyChannelMask(ch, mask)) {
    server.send(500, "text/plain", "Apply failed\n");
    return;
  }

  char msg[128];
  snprintf(msg, sizeof(msg), "CH%u target %.3f Ohm -> 0x%04X, calc %.3f Ohm, err %.4f%%",
           unsigned(ch + 1), targetOhm, unsigned(mask), calcOhm, errPct);
  setStatus(msg);
  appendLogEvent(msg);
  clearLastError();

  redirectToRoot();
}

/**
 * @brief Handle Profiles Page.
 */
void handleProfilesPage() {
  noteHttpRequest();

  String html;
  html.reserve(9000);
  appendCommonPageHeader(html, "RP2040 Matrix Profiles");
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

/**
 * @brief Handle Profile Save.
 */
void handleProfileSave() {
  noteHttpRequest();

  String name = server.hasArg("name") ? server.arg("name") : "profile";
  bool ok = saveCurrentProfile(name);

  if (ok) {
    String msg = String("Profile saved: ") + sanitizeName(name);
    setStatus(msg.c_str());
    appendLogEvent(msg.c_str());
    clearLastError();
  } else {
    setStatus("Profile save failed");
    setLastError("Profile save failed");
  }

  server.sendHeader("Location", "/profiles", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Handle Profile Apply.
 */
void handleProfileApply() {
  noteHttpRequest();

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing profile name\n");
    return;
  }

  uint16_t masks[CHANNEL_COUNT];
  if (!loadProfileMasks(server.arg("name"), masks)) {
    server.send(404, "text/plain", "Profile not found\n");
    return;
  }

  char reason[128];
  if (!applyAllMasksSafely(masks, reason, sizeof(reason))) {
    server.send(400, "text/plain", String("Profile rejected/apply failed: ") + reason + "\n");
    return;
  }

  String msg = String("Profile applied: ") + sanitizeName(server.arg("name"));
  setStatus(msg.c_str());
  appendLogEvent(msg.c_str());
  clearLastError();

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Handle Profile Delete.
 */
void handleProfileDelete() {
  noteHttpRequest();

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing profile name\n");
    return;
  }

  String path = profilePathFromName(server.arg("name"));
  bool ok = littleFsReady && LittleFS.exists(path) && LittleFS.remove(path);

  String msg = ok ? String("Profile deleted: ") + sanitizeName(server.arg("name")) : "Profile delete failed";
  setStatus(msg.c_str());
  appendLogEvent(msg.c_str());

  server.sendHeader("Location", "/profiles", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Handle Safety Page.
 */
void handleSafetyPage() {
  noteHttpRequest();

  String html;
  html.reserve(7000);
  appendCommonPageHeader(html, "RP2040 Matrix Safety");
  html += "<h1>Safety Limits</h1>";
  appendSafetySummaryCard(html);
  html += "<div class='card'><form method='POST' action='/safety_save'>";
  html += "<table><tr><th>Setting</th><th>Value</th></tr>";
  html += "<tr><td>Minimum allowed calculated resistance, Ohm</td><td><input name='min_ohm' value='";
  html += String(safetyMinOhm, 3);
  html += "'></td></tr>";
  html += "<tr><td>Maximum active bits per channel</td><td><input name='max_bits' value='";
  html += String(safetyMaxActiveBits);
  html += "'></td></tr>";
  html += "<tr><td>Expert mode / bypass safety checks</td><td><select name='expert'><option value='0'";
  html += safetyExpertMode ? "" : " selected";
  html += ">OFF</option><option value='1'";
  html += safetyExpertMode ? " selected" : "";
  html += ">ON</option></select></td></tr>";
  html += "</table><p><button type='submit'>Save safety settings</button></p></form></div>";
  html += "<div class='notice'>Recommended default: minimum 300 Ohm, max active bits 16, expert OFF. Lower limits can stress resistors/MOSFETs depending on applied voltage.</div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

/**
 * @brief Handle Safety Save.
 */
void handleSafetySave() {
  noteHttpRequest();

  if (server.hasArg("min_ohm")) {
    double v = server.arg("min_ohm").toFloat();
    if (v > 0.0) {
      safetyMinOhm = v;
    }
  }

  if (server.hasArg("max_bits")) {
    int v = server.arg("max_bits").toInt();
    if (v >= 1 && v <= 16) {
      safetyMaxActiveBits = uint8_t(v);
    }
  }

  safetyExpertMode = server.hasArg("expert") && server.arg("expert") == "1";

  bool saved = saveSafetyConfigToLittleFS();
  setStatus(saved ? "Safety settings saved" : "Safety settings changed in RAM only");
  appendLogEvent("Safety settings updated");

  server.sendHeader("Location", "/safety", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Render the browser-visible SCPI command reference page.
 */
void handleScpiPage() {
  noteHttpRequest();

  String html;
  html.reserve(8000);
  appendCommonPageHeader(html, "RP2040 Matrix SCPI");
  html += "<h1>SCPI Interface</h1>";
  html += "<div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Port</div><div class='metric-value'>5025</div></div>";
  html += "<div class='metric'><div class='metric-label'>Client</div><div class='metric-value'>";
  html += (scpiClient && scpiClient.connected()) ? "connected" : "not connected";
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Command count</div><div class='metric-value'>";
  html += String(scpiCommandCount);
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Serial number</div><div class='metric-value'>";
  html += deviceSerialNumber;
  html += "</div></div>";
  html += "</div>";
  html += "<div class='card'><h2>Supported commands</h2><pre><code>";
  html += "*IDN?\n";
  html += "SYST:SER?\n";
  html += "*CLS\n";
  html += "SYST:ERR?\n";
  html += "SYST:ERR:CLEAR\n";
  html += "SYST:STAT?\n";
  html += "STATE?\n";
  html += "ALL:OFF\n";
  html += "OUTP:ALL OFF\n";
  html += "ROUT:ALL:MASK <m1>,<m2>,<m3>,<m4>,<m5>,<m6>,<m7>,<m8>\n";
  html += "CH1:MASK 0001\n";
  html += "CH1:MASK?\n";
  html += "ROUT:CHANnel1:MASK 0001\n";
  html += "ROUT:CHANnel1:MASK?\n";
  html += "CH1:RES?\n";
  html += "CH1:CONF?\n\n";
  html += "Calibration resistor table queries for PC-side nearest-mask calculation:\n";
  html += "CAL:RES?\n";
  html += "CAL:RESISTORS?\n";
  html += "CAL:RES? 3\n";
  html += "CAL:RES? CH3\n";
  html += "CAL:CHAN1:RES?\n";
  html += "CAL:CHANnel1:RESISTORS?\n";
  html += "CAL:CH1:TABLE?\n";
  html += "Response record: CHn:bit_index,mosfet_name,resistance_ohm;...\n";
  html += "</code></pre></div>";
  html += "<div class='card'><h2>Last SCPI command</h2><pre><code>";
  html += lastScpiCommand;
  html += "</code></pre></div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

/**
 * @brief Handle Log Page.
 */
void handleLogPage() {
  noteHttpRequest();

  String html;
  html.reserve(8000);
  appendCommonPageHeader(html, "RP2040 Matrix Event Log");
  html += "<h1>Event Log</h1><div class='card'><pre><code>";

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

/**
 * @brief Handle Backup Page.
 */
void handleBackupPage() {
  noteHttpRequest();

  String html;
  html.reserve(7000);
  appendCommonPageHeader(html, "RP2040 Matrix Backup");
  html += "<h1>Backup / Factory Reset</h1>";
  html += "<div class='card'><h2>Export</h2><p><a class='tab' href='/backup_download'>Download all text configuration</a></p></div>";
  html += "<div class='card'><h2>Factory reset</h2>";
  html += "<p class='warn'>Factory reset deletes stored LittleFS config files. Current running RAM state may remain until reboot.</p>";
  html += "<p><a class='tab off' href='/factory_reset?scope=calibration'>Delete calibration files</a></p>";
  html += "<p><a class='tab off' href='/factory_reset?scope=profiles'>Delete profiles</a></p>";
  html += "<p><a class='tab off' href='/factory_reset?scope=network'>Delete network config</a></p>";
  html += "<p><a class='tab off' href='/factory_reset?scope=all'>FULL FACTORY RESET</a></p>";
  html += "</div>";
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

/**
 * @brief Handle Backup Download.
 */
void handleBackupDownload() {
  noteHttpRequest();

  String out;
  out.reserve(9000);
  out += "# RP2040 Matrix backup\n";
  out += "# Serial number: ";
  out += deviceSerialNumber;
  out += "\n";
  out += "# Generated at uptime ms: ";
  out += String(millis());
  out += "\n\n[network]\n";
  out += networkConfigToText();
  out += "\n[safety]\n";
  out += "min_ohm,";
  out += String(safetyMinOhm, 6);
  out += "\nmax_active_bits,";
  out += String(safetyMaxActiveBits);
  out += "\nexpert_mode,";
  out += safetyExpertMode ? "1\n" : "0\n";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    out += "\n[ch";
    out += String(ch + 1);
    out += "_config]\n";
    out += channelConfigToText(ch);
  }

  sendNoCacheHeaders();
  server.send(200, "text/plain", out);
}

/**
 * @brief Delete Files By Prefix.
 * @param prefix Function parameter.
 */
void deleteFilesByPrefix(const char* prefix) {
  if (!littleFsReady) {
    return;
  }

  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String name = dir.fileName();
    if (!name.startsWith("/")) {
      name = "/" + name;
    }
    if (name.startsWith(prefix)) {
      LittleFS.remove(name);
    }
  }
}

/**
 * @brief Handle Factory Reset.
 */
void handleFactoryReset() {
  noteHttpRequest();

  String scope = server.hasArg("scope") ? server.arg("scope") : "";
  scope.toLowerCase();

  if (scope == "calibration") {
    deleteFilesByPrefix("/ch");
    deleteFilesByPrefix("/meta_ch");
    appendLogEvent("Factory reset: calibration files deleted");
  } else if (scope == "profiles") {
    deleteFilesByPrefix("/profile_");
    appendLogEvent("Factory reset: profiles deleted");
  } else if (scope == "network") {
    if (littleFsReady) {
      LittleFS.remove(networkConfigPath());
    }
    appendLogEvent("Factory reset: network config deleted");
  } else if (scope == "all") {
    deleteFilesByPrefix("/ch");
    deleteFilesByPrefix("/meta_ch");
    deleteFilesByPrefix("/profile_");
    if (littleFsReady) {
      LittleFS.remove(networkConfigPath());
      LittleFS.remove(safetyConfigPath());
    }
    appendLogEvent("Factory reset: all known config files deleted");
  } else {
    server.send(400, "text/plain", "Bad reset scope\n");
    return;
  }

  setStatus("Factory reset action complete. Restart recommended.");
  server.sendHeader("Location", "/backup", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Handle Watchdog Page.
 */
void handleWatchdogPage() {
  noteHttpRequest();

  String html;
  html.reserve(5000);
  appendCommonPageHeader(html, "RP2040 Matrix Watchdog");
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

/**
 * @brief Handle Calibration Meta Save.
 */
void handleCalibrationMetaSave() {
  noteHttpRequest();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel\n");
    return;
  }

  if (!littleFsReady) {
    server.send(500, "text/plain", "LittleFS not ready\n");
    return;
  }

  File f = LittleFS.open(calibrationMetaPath(ch), "w");
  if (!f) {
    server.send(500, "text/plain", "Failed to write metadata\n");
    return;
  }

  f.print("date,");
  f.println(server.hasArg("date") ? server.arg("date") : "");
  f.print("valid_until,");
  f.println(server.hasArg("valid_until") ? server.arg("valid_until") : "");
  f.print("operator,");
  f.println(server.hasArg("operator") ? server.arg("operator") : "");
  f.print("dmm,");
  f.println(server.hasArg("dmm") ? server.arg("dmm") : "");
  f.print("report_id,");
  f.println(server.hasArg("report_id") ? server.arg("report_id") : "");
  f.close();

  appendLogEvent("Calibration metadata saved");
  setStatus("Calibration metadata saved");

  server.sendHeader("Location", "/settings", true);
  server.send(303, "text/plain", "See Other\n");
}


/**
 * @brief Handle Set.
 */
void handleSet() {
  noteHttpRequest();

  Serial.println("HTTP /set begin");
  Serial.flush();

  setLedMode(LED_ACTIVITY);

  uint8_t ch = 0;
  uint16_t mask = 0;

  if (!parseChannel(ch)) {
    setLedMode(LED_FAULT);
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!parseMask(mask)) {
    setLedMode(LED_FAULT);
    server.send(400, "text/plain", "Bad mask. Use mask=0000..FFFF or 0x0000..0xFFFF\n");
    return;
  }

  Serial.print("Parsed CH");
  Serial.print(ch + 1);
  Serial.print(" mask=");
  Serial.println(hex16(mask));
  Serial.flush();

  char safetyReason[128];
  if (!checkMaskSafety(ch, mask, safetyReason, sizeof(safetyReason))) {
    setLedMode(LED_FAULT);
    setLastError(safetyReason);
    server.send(400, "text/plain", String(safetyReason) + "\n");
    return;
  }

  bool ok = applyChannelMask(ch, mask);

  if (!ok) {
    setLedMode(LED_FAULT);
    server.send(500, "text/plain", "Apply failed\n");
    return;
  }

  char msg[96];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u set to 0x%04X",
    unsigned(ch + 1),
    unsigned(mask)
  );

  setStatus(msg);
  appendLogEvent(msg);
  clearLastError();
  setLedMode(LED_OK);

  redirectToRoot();
}

/**
 * @brief Handle All Off.
 */
void handleAllOff() {
  noteHttpRequest();

  Serial.println("HTTP /alloff");
  Serial.flush();

  forceAllOff();
  appendLogEvent("ALL OFF by HTTP");
  clearLastError();

  redirectToRoot();
}

/**
 * @brief Handle Not Found.
 */
void handleNotFound() {
  noteHttpRequest();

  Serial.print("HTTP 404: ");
  Serial.println(server.uri());
  Serial.flush();

  server.send(404, "text/plain", "404 Not Found\n");
}

/**
 * @brief Register all HTTP routes and start the web server.
 */
void setupHttpServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/state", HTTP_GET, handleState);
  server.on("/live", HTTP_GET, handleLiveStatePage);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/toggle_bit", HTTP_GET, handleToggleBit);
  server.on("/alloff", HTTP_GET, handleAllOff);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/profiles", HTTP_GET, handleProfilesPage);
  server.on("/profile_save", HTTP_POST, handleProfileSave);
  server.on("/profile_apply", HTTP_GET, handleProfileApply);
  server.on("/profile_delete", HTTP_GET, handleProfileDelete);
  server.on("/target_apply", HTTP_GET, handleTargetApply);
  server.on("/safety", HTTP_GET, handleSafetyPage);
  server.on("/safety_save", HTTP_POST, handleSafetySave);
  server.on("/network", HTTP_GET, handleNetwork);
  server.on("/network_save", HTTP_POST, handleNetworkSave);
  server.on("/runtime", HTTP_GET, handleRuntimePage);
  server.on("/scpi", HTTP_GET, handleScpiPage);
  server.on("/files", HTTP_GET, handleFilesPage);
  server.on("/log", HTTP_GET, handleLogPage);
  server.on("/backup", HTTP_GET, handleBackupPage);
  server.on("/backup_download", HTTP_GET, handleBackupDownload);
  server.on("/factory_reset", HTTP_GET, handleFactoryReset);
  server.on("/watchdog", HTTP_GET, handleWatchdogPage);
  server.on("/meta_save", HTTP_POST, handleCalibrationMetaSave);
  server.on("/upload_config", HTTP_POST, handleUploadConfig);
  server.on("/api/upload_config", HTTP_POST, handleUploadConfigApi);
  server.on("/api/calibration/upload", HTTP_POST, handleUploadConfigApi);
  server.on("/config", HTTP_GET, handleDownloadConfig);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("HTTP server started on port 80");
  Serial.flush();
}

