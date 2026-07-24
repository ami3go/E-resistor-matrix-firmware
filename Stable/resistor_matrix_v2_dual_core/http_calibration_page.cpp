/**
 * @file http_calibration_page.cpp
 * @brief Streamed Gate 5 calibration page with bounded temporary memory.
 */
#include "http_api.h"

static void updateMinimumHeap(uint32_t& minimumFree) {
  const uint32_t current = getHeapFreeBytes();
  if (current < minimumFree) minimumFree = current;
}

static void streamChannelOptions(HttpResponseWriter& out, uint8_t selectedChannel) {
  for (uint8_t ch = 1; ch <= CHANNEL_COUNT; ++ch) {
    out.printf("<option value='%u'%s>CH%u</option>", unsigned(ch),
               ch == selectedChannel ? " selected" : "", unsigned(ch));
  }
}

void streamCombinedChannelResistorTable(HttpResponseWriter& out) {
  out.write("<div class='card'><h2>Current channel resistor tables</h2>");
  out.write("<p class='small'>One row per shift-register bit. Values are streamed directly from the active numeric calibration tables.</p>");
  out.write("<div class='table-scroll'><table class='resistor-matrix-table'><tr><th>Bit</th><th>MOSFET</th>");
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) out.printf("<th>CH%u</th>", unsigned(ch + 1));
  out.write("</tr>");
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
    out.printf("<tr><td><code>%u</code></td><td><code>%s</code></td>",
               unsigned(bit), mosfetNameForBit(bit));
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      const float value = getRuntimeResistanceOhms(ch, bit);
      if (isfinite(value) && value > 0.0f) {
        out.printf("<td class='resistor-cell'><span>%.6f</span></td>", double(value));
      } else {
        out.write("<td class='resistor-cell'><span>-</span></td>");
      }
    }
    out.write("</tr>");
    if ((bit & 3U) == 3U) out.flush();
  }
  out.write("</table></div></div>");
}

void handleSettings() {
  noteHttpRequest();
  Serial.println("HTTP /settings (streamed)");
  Serial.flush();

  int selectedChannel = server.hasArg("ch") ? server.arg("ch").toInt() : 1;
  if (selectedChannel < 1 || selectedChannel > int(CHANNEL_COUNT)) selectedChannel = 1;
  const uint8_t selected = uint8_t(selectedChannel);
  const uint8_t channelIndex = uint8_t(selected - 1U);

  const uint32_t heapBefore = getHeapFreeBytes();
  uint32_t minimumFree = heapBefore;
  HttpResponseWriter out;
  if (!out.begin(200, "text/html; charset=utf-8")) {
    server.send(500, "text/plain", "Unable to start streamed response\n");
    return;
  }

  streamCommonPageHeader(out, "E-Resistor Calibration");
  out.write("<h1>Calibration</h1><p>Status: <code>"); out.write(statusText); out.write("</code></p>");
  streamCombinedChannelResistorTable(out);
  updateMinimumHeap(minimumFree);

  out.write("<div class='card'><h2>Upload per-channel ResistorBitInfo table</h2>");
  out.write("<p class='small'>Upload CSV lines or copied C++ initializer lines. Gate 5 streams this page through a fixed 512-byte response buffer.</p>");
  out.write("<form method='GET' action='/settings'>View channel: <select name='ch'>");
  streamChannelOptions(out, selected);
  out.write("</select><button type='submit'>Load</button></form>");

  out.write("<form method='POST' action='/upload_config'><p>Upload to channel: <select name='ch'>");
  streamChannelOptions(out, selected);
  out.write("</select></p><p><input type='file' id='fileInput' accept='.csv,.h,.txt'>");
  out.write("<span class='small'>The flash-backed app.js resource loads the selected file into the text box.</span></p>");
  out.write("<textarea id='configText' name='config'>");
  out.write("bit,mosfet_name,nominal_resistance\n");
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
    const float value = getRuntimeResistanceOhms(channelIndex, bit);
    out.printf("%u,%s,%.6f\n", unsigned(bit), mosfetNameForBit(bit), double(value));
  }
  out.write("</textarea><p><button class='apply' type='submit'>Upload / Save Channel Config</button></p></form>");
  updateMinimumHeap(minimumFree);

  out.write("<h2>Expected CSV format</h2><pre><code>bit,mosfet_name,nominal_resistance\n0,Q16,626R\n1,Q15,1.24k\n...\n15,Q1,20M\n</code></pre>");
  out.write("<h2>Download calibration files</h2><div class='card'>");
  out.write("<p class='small'>API v1 is canonical; compatibility aliases remain available during the deprecation period.</p>");
  out.write("<p><a class='button' href='/api/v1/calibration/files'>List calibration files</a> ");
  out.write("<a class='button' href='/api/v1/calibration/download_all'>Download all active tables</a></p>");
  out.printf("<p><a class='button' href='/api/v1/calibration/download?ch=%u'>Download CH%u CSV</a></p>",
             unsigned(selected), unsigned(selected));
  out.write("<pre><code>GET /api/v1/calibration/files\nGET /api/v1/calibration/download?ch=1\nGET /api/v1/calibration/download_all\nSCPI: CAL:FILES?, CAL:FILE? CH1, CAL:ALL:FILES?\n</code></pre></div>");

  out.write("<h2>Calibration metadata</h2><div class='card'><form method='POST' action='/meta_save'>");
  out.write("<p>Channel: <select name='ch'>"); streamChannelOptions(out, selected); out.write("</select></p>");
  out.write("<p><input name='date' placeholder='Calibration date YYYY-MM-DD'> <input name='valid_until' placeholder='Valid until YYYY-MM-DD'></p>");
  out.write("<p><input name='operator' placeholder='Operator'> <input name='dmm' placeholder='DMM model, e.g. HP 34401A'></p>");
  out.write("<p><input name='report_id' placeholder='Report ID / PDF filename'> <button type='submit'>Save metadata</button></p></form>");
  out.write("<form method='POST' action='/meta_delete' data-confirm='Delete metadata for selected channel?'>");
  out.write("<p>Delete metadata for channel: <select name='ch'>"); streamChannelOptions(out, selected);
  out.write("</select> <button class='off' type='submit'>Delete metadata</button></p></form></div>");
  out.write("<h2>Accepted .h initializer line format</h2><pre><code>{0,  \"Q16\", \"626R\"},\n{1,  \"Q15\", \"1.24k\"},\n...\n{15, \"Q1\",  \"20M\"},\n</code></pre></div>");
  streamCommonPageFooter(out);
  out.end();

  updateMinimumHeap(minimumFree);
  httpCalibrationPageLastTempBytes = heapBefore >= minimumFree ? heapBefore - minimumFree : 0U;
  if (httpCalibrationPageLastTempBytes > httpCalibrationPagePeakTempBytes) {
    httpCalibrationPagePeakTempBytes = httpCalibrationPageLastTempBytes;
  }
}
