/**
 * @file http_page_helpers.cpp
 * @brief Shared HTML page header/footer rendering helpers.
 */

#include "app.h"

// 09_http_page_helpers.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// HTTP page helpers
// ============================================================

/**
 * @brief Append Common Page Header.
 * @param html HTML string that receives generated markup.
 * @param title Function parameter.
 */
void appendCommonPageHeader(String& html, const char* title) {
  html += "<!doctype html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='Cache-Control' content='no-store'>";
  html += "<title>";
  html += title;
  html += "</title>";

  html += "<style>";
  html += ":root{--bg:#0f1117;--surface:#171c24;--surface2:#202733;--surface3:#273041;--primary:#5b8cff;--primary2:#7ca6ff;--danger:#ff5d5d;--ok:#35d07f;--warn:#ffd166;--text:#eef3fb;--muted:#a9b3c3;--border:#303a49;--shadow:0 12px 30px rgba(0,0,0,.28);--radius:16px;}";
  html += "*{box-sizing:border-box;}";
  html += "body{margin:0;font-family:Arial,Helvetica,sans-serif;background:linear-gradient(135deg,#0f1117,#141b28 55%,#10141c);color:var(--text);}";
  html += ".topbar{position:sticky;top:0;z-index:10;background:rgba(15,17,23,.92);backdrop-filter:blur(10px);border-bottom:1px solid var(--border);box-shadow:0 4px 18px rgba(0,0,0,.22);}";
  html += ".topbar-inner{max-width:1600px;margin:0 auto;padding:14px 18px;display:flex;gap:16px;align-items:center;justify-content:space-between;flex-wrap:wrap;}";
  html += ".brand{font-size:18px;font-weight:800;letter-spacing:.2px;display:flex;gap:10px;align-items:center;flex-wrap:wrap;}";
  html += ".serial{font-size:12px;font-weight:700;color:var(--muted);border:1px solid var(--border);background:var(--surface2);padding:4px 8px;border-radius:999px;font-family:monospace;}";
  html += ".dot{width:12px;height:12px;border-radius:50%;background:var(--ok);box-shadow:0 0 14px var(--ok);display:inline-block;}";
  html += ".tabs{display:flex;gap:8px;flex-wrap:wrap;}";
  html += ".tab{color:var(--text);text-decoration:none;padding:9px 13px;border-radius:999px;border:1px solid var(--border);background:var(--surface);font-size:14px;font-weight:600;}";
  html += ".tab:hover{background:var(--surface3);border-color:var(--primary);}";
  html += ".page{max-width:1600px;margin:0 auto;padding:22px 18px 40px;}";
  html += "h1{font-size:30px;margin:10px 0 8px;}h2{font-size:21px;margin:26px 0 12px;}h3{font-size:17px;margin:20px 0 10px;}";
  html += ".card{background:linear-gradient(180deg,var(--surface),#141922);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin:16px 0;box-shadow:var(--shadow);}";
  html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;}";
  html += ".metric{background:var(--surface2);border:1px solid var(--border);border-radius:14px;padding:14px;}.metric-link{display:block;color:var(--text);text-decoration:none;transition:transform .08s ease,border-color .08s ease,background .08s ease;}.metric-link:hover{background:var(--surface3);border-color:var(--primary);transform:translateY(-1px);}";
  html += ".metric-label{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;}";
  html += ".metric-value{font-size:22px;font-weight:800;margin-top:4px;}";
  html += "input,button,select,textarea{font-size:16px;padding:10px 12px;margin:4px;border-radius:12px;border:1px solid var(--border);}";
  html += "input,select,textarea{background:var(--surface2);color:var(--text);outline:none;}";
  html += "input:focus,select:focus,textarea:focus{border-color:var(--primary);box-shadow:0 0 0 3px rgba(91,140,255,.18);}";
  html += "textarea{width:100%;max-width:980px;height:360px;font-family:monospace;display:block;}";
  html += "button,.button{background:var(--primary);color:white;font-weight:700;cursor:pointer;border-color:transparent;text-decoration:none;display:inline-block;padding:10px 12px;margin:4px;border-radius:12px;white-space:nowrap;}button:hover,.button:hover{background:var(--primary2);}";
  html += "table{border-collapse:separate;border-spacing:0;margin-top:14px;width:100%;max-width:none;background:var(--surface);border:1px solid var(--border);border-radius:14px;overflow:hidden;box-shadow:0 8px 22px rgba(0,0,0,.18);}";
  html += "td,th{border-bottom:1px solid var(--border);padding:10px 12px;text-align:left;vertical-align:middle;}tr:last-child td{border-bottom:0;}th{background:var(--surface3);color:#dfe7f5;font-size:13px;text-transform:uppercase;letter-spacing:.04em;}";
  html += "code,pre{color:#a7f3d0;}pre{background:#0b0f14;border:1px solid var(--border);border-radius:12px;padding:14px;overflow:auto;}";
  html += "a{color:#93bbff;}";
  html += ".warn{color:var(--warn);}.ok{color:var(--ok);}.danger{color:var(--danger);}";
  html += ".apply{background:var(--primary);color:white;}.off{background:#722020;color:white;}.off:hover{background:#9a2929;}";
  html += ".mask{width:88px;font-family:monospace;}.table-scroll{width:100%;overflow-x:auto;}.control-table{width:100%;min-width:0;table-layout:auto;}.control-table th,.control-table td{padding:6px 7px;font-size:13px;}.control-table th{font-size:11px;letter-spacing:.025em;}.control-table td:nth-child(1){width:62px;white-space:nowrap;}.control-table td:nth-child(2){width:92px;white-space:nowrap;}.control-table td:nth-child(3){min-width:390px;}.control-table td:nth-child(4){width:135px;white-space:nowrap;}.control-table td:nth-child(5){width:220px;white-space:nowrap;}.control-table td:nth-child(6){width:205px;white-space:nowrap;}.control-table input,.control-table button{font-size:13px;padding:6px 8px;margin:1px;border-radius:9px;}.manual-mask-form{display:flex;gap:2px;align-items:center;flex-wrap:nowrap;white-space:nowrap;}";
  html += ".small{color:var(--muted);font-size:14px;line-height:1.45;}";
  html += ".bits{display:flex;flex-wrap:nowrap;gap:2px;min-width:386px;white-space:nowrap;}";
  html += ".bit{display:inline-block;flex:0 0 23px;width:23px;height:22px;line-height:22px;text-align:center;border:1px solid var(--border);border-radius:6px;font-size:10px;font-family:monospace;}";
  html += ".bit:hover{transform:translateY(-1px);box-shadow:0 0 0 2px rgba(91,140,255,.25);border-color:var(--primary);}";
  html += ".bit:active{transform:translateY(0);filter:brightness(1.25);}";
  html += ".bit.on{background:var(--ok);color:#06130c;border-color:#73f0aa;box-shadow:0 0 10px rgba(53,208,127,.55);font-weight:800;}";
  html += ".bit.off{background:#111720;color:#667085;border-color:#2b3442;}";
  html += ".res{font-family:monospace;color:#a7f3d0;white-space:nowrap;font-weight:700;}";
  html += ".notice{border-left:4px solid var(--primary);padding:12px 14px;background:rgba(91,140,255,.10);border-radius:12px;margin:14px 0;}";
  html += ".chip{display:inline-block;padding:4px 9px;border-radius:999px;border:1px solid var(--border);background:var(--surface2);font-size:12px;font-weight:700;}";
  html += ".res-danger{color:#ff7b7b;}.res-warn{color:#ffd166;}.res-ok{color:#35d07f;}.res-high{color:#93bbff;}.res-open{color:#a9b3c3;}.res-error{color:#ff5d5d;}";
  html += ".target{width:128px;font-family:monospace;}.inline-form{display:inline-flex;gap:2px;align-items:center;flex-wrap:nowrap;white-space:nowrap;}.factory-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:12px;margin-top:14px;}.factory-grid .button{width:100%;text-align:center;margin:0;padding:13px 14px;}";
  html += "@media(max-width:720px){.topbar-inner{align-items:flex-start}.tabs{width:100%;}.tab{flex:1;text-align:center;}table{font-size:13px;}td,th{padding:8px;}h1{font-size:24px;}}";
  html += "</style>";

  html += "</head><body>";
  html += "<header class='topbar'><div class='topbar-inner'>";
  html += "<div class='brand'><span class='dot'></span><span>E-Resistor</span><span class='serial'>SN ";
  html += deviceSerialNumber;
  html += "</span><span class='serial'>FW ";
  html += FIRMWARE_VERSION;
  html += "</span><span class='serial'>Build ";
  html += FIRMWARE_BUILD_DATE;
  html += "</span></div>";
  html += "<nav class='tabs'>";
  html += "<a class='tab' href='/'>Control</a>";
  html += "<a class='tab' href='/settings'>Calibration</a>";
  html += "<a class='tab' href='/profiles'>Profiles</a>";
  html += "<a class='tab' href='/safety'>Safety</a>";
  html += "<a class='tab' href='/network'>Ethernet</a>";
  html += "<a class='tab' href='/runtime'>Runtime</a>";
  html += "<a class='tab' href='/scpi'>SCPI</a>";
  html += "<a class='tab' href='/files'>Files</a>";
  html += "<a class='tab' href='/firmware'>Firmware</a>";
  html += "<a class='tab' href='/log'>Log</a>";
  html += "<a class='tab' href='/backup'>Backup</a>";
  html += "<a class='tab' href='/watchdog'>Watchdog</a>";
  html += "<a class='tab' href='/live'>Live State</a>";
  html += "</nav>";
  html += "</div></header>";
  html += "<main class='page'>";
}

/**
 * @brief Append Common Page Footer.
 * @param html HTML string that receives generated markup.
 */
void appendCommonPageFooter(String& html) {
  html += "</main></body></html>";
}


