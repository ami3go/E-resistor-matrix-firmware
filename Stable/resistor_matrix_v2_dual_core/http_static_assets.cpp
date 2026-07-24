/**
 * @file http_static_assets.cpp
 * @brief Gate 5 flash-backed, cacheable CSS and JavaScript resources.
 */
#include "http_api.h"

static const char kApplicationCss[] PROGMEM = R"G5CSS(:root{--bg:#0f1117;--surface:#171c24;--surface2:#202733;--surface3:#273041;--primary:#5b8cff;--primary2:#7ca6ff;--danger:#ff5d5d;--ok:#35d07f;--warn:#ffd166;--text:#eef3fb;--muted:#a9b3c3;--border:#303a49;--shadow:0 12px 30px rgba(0,0,0,.28);--radius:16px;}
*{box-sizing:border-box;}
body{margin:0;font-family:Arial,Helvetica,sans-serif;background:linear-gradient(135deg,#0f1117,#141b28 55%,#10141c);color:var(--text);}
.topbar{position:sticky;top:0;z-index:10;background:rgba(15,17,23,.92);backdrop-filter:blur(10px);border-bottom:1px solid var(--border);box-shadow:0 4px 18px rgba(0,0,0,.22);}
.topbar-inner{max-width:1600px;margin:0 auto;padding:14px 18px;display:flex;gap:16px;align-items:center;justify-content:space-between;flex-wrap:wrap;}
.brand{font-size:18px;font-weight:800;letter-spacing:.2px;display:flex;gap:10px;align-items:center;flex-wrap:wrap;}
.serial{font-size:12px;font-weight:700;color:var(--muted);border:1px solid var(--border);background:var(--surface2);padding:4px 8px;border-radius:999px;font-family:monospace;}
.dot{width:12px;height:12px;border-radius:50%;background:var(--ok);box-shadow:0 0 14px var(--ok);display:inline-block;}
.tabs{display:flex;gap:8px;flex-wrap:wrap;}
.tab{color:var(--text);text-decoration:none;padding:9px 13px;border-radius:999px;border:1px solid var(--border);background:var(--surface);font-size:14px;font-weight:600;}
.tab:hover{background:var(--surface3);border-color:var(--primary);}
.page{max-width:1600px;margin:0 auto;padding:22px 18px 40px;}
h1{font-size:30px;margin:10px 0 8px;}h2{font-size:21px;margin:26px 0 12px;}h3{font-size:17px;margin:20px 0 10px;}
.card{background:linear-gradient(180deg,var(--surface),#141922);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin:16px 0;box-shadow:var(--shadow);}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;}
.metric{background:var(--surface2);border:1px solid var(--border);border-radius:14px;padding:14px;}.metric-link{display:block;color:var(--text);text-decoration:none;transition:transform .08s ease,border-color .08s ease,background .08s ease;}.metric-link:hover{background:var(--surface3);border-color:var(--primary);transform:translateY(-1px);}
.metric-label{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;}
.metric-value{font-size:22px;font-weight:800;margin-top:4px;}
input,button,select,textarea{font-size:16px;padding:10px 12px;margin:4px;border-radius:12px;border:1px solid var(--border);}
input,select,textarea{background:var(--surface2);color:var(--text);outline:none;}
input:focus,select:focus,textarea:focus{border-color:var(--primary);box-shadow:0 0 0 3px rgba(91,140,255,.18);}
textarea{width:100%;max-width:980px;height:360px;font-family:monospace;display:block;}
button,.button{background:var(--primary);color:white;font-weight:700;cursor:pointer;border-color:transparent;text-decoration:none;display:inline-block;padding:10px 12px;margin:4px;border-radius:12px;white-space:nowrap;}button:hover,.button:hover{background:var(--primary2);}
table{border-collapse:separate;border-spacing:0;margin-top:14px;width:100%;max-width:none;background:var(--surface);border:1px solid var(--border);border-radius:14px;overflow:hidden;box-shadow:0 8px 22px rgba(0,0,0,.18);}
td,th{border-bottom:1px solid var(--border);padding:10px 12px;text-align:left;vertical-align:middle;}tr:last-child td{border-bottom:0;}th{background:var(--surface3);color:#dfe7f5;font-size:13px;text-transform:uppercase;letter-spacing:.04em;}
code,pre{color:#a7f3d0;}pre{background:#0b0f14;border:1px solid var(--border);border-radius:12px;padding:14px;overflow:auto;}
a{color:#93bbff;}
.warn{color:var(--warn);}.ok{color:var(--ok);}.danger{color:var(--danger);}
.apply{background:var(--primary);color:white;}.off{background:#722020;color:white;}.off:hover{background:#9a2929;}
.mask{width:88px;font-family:monospace;}.table-scroll{width:100%;overflow-x:auto;}.control-table{width:100%;min-width:0;table-layout:auto;}.control-table th,.control-table td{padding:6px 7px;font-size:13px;}.control-table th{font-size:11px;letter-spacing:.025em;}.control-table td:nth-child(1){width:62px;white-space:nowrap;}.control-table td:nth-child(2){width:92px;white-space:nowrap;}.control-table td:nth-child(3){min-width:390px;}.control-table td:nth-child(4){width:135px;white-space:nowrap;}.control-table td:nth-child(5){width:220px;white-space:nowrap;}.control-table td:nth-child(6){width:205px;white-space:nowrap;}.control-table input,.control-table button{font-size:13px;padding:6px 8px;margin:1px;border-radius:9px;}.manual-mask-form{display:flex;gap:2px;align-items:center;flex-wrap:nowrap;white-space:nowrap;}
.small{color:var(--muted);font-size:14px;line-height:1.45;}
.resistor-matrix-table{min-width:1050px;}.resistor-matrix-table th,.resistor-matrix-table td{text-align:center;white-space:nowrap;}.resistor-matrix-table th:first-child,.resistor-matrix-table td:first-child{text-align:left;}.resistor-cell code{display:block;color:#dfe7f5;}.resistor-cell span{display:block;color:#a7f3d0;font-family:monospace;margin-top:3px;}
.safety-table{min-width:1180px;}.safety-table th:not(:first-child),.safety-table td:not(:first-child){text-align:center;}.safety-table input{width:126px;max-width:100%;font-family:monospace;padding:7px 8px;margin:0;}
.file-table{min-width:920px;}
.scpi-command-table{min-width:760px;}.scpi-command-table th:first-child,.scpi-command-table td:first-child{width:42%;white-space:nowrap;}.scpi-command-table td:nth-child(2){text-align:left;}
.bits{display:flex;flex-wrap:nowrap;gap:4px;min-width:444px;white-space:nowrap;align-items:center;}
.bits form{display:inline-flex;margin:0;padding:0;align-items:center;}
.bit{appearance:none;-webkit-appearance:none;display:inline-flex;align-items:center;justify-content:center;flex:0 0 24px;width:24px;height:24px;min-width:24px;min-height:24px;padding:0;margin:0;border:1px solid var(--border);border-radius:50%;font-size:11px;font-family:monospace;font-weight:700;line-height:1;text-align:center;vertical-align:middle;}
.bit:hover{transform:translateY(-1px);box-shadow:0 0 0 2px rgba(91,140,255,.25);border-color:var(--primary);}
.bit:active{transform:translateY(0);filter:brightness(1.25);}
.bit.on{background:var(--ok);color:#06130c;border-color:#73f0aa;box-shadow:0 0 10px rgba(53,208,127,.55);font-weight:800;}
.bit.off{background:#111720;color:#667085;border-color:#2b3442;}
.res{font-family:monospace;color:#a7f3d0;white-space:nowrap;font-weight:700;}
.notice{border-left:4px solid var(--primary);padding:12px 14px;background:rgba(91,140,255,.10);border-radius:12px;margin:14px 0;}
.chip{display:inline-block;padding:4px 9px;border-radius:999px;border:1px solid var(--border);background:var(--surface2);font-size:12px;font-weight:700;}
.res-danger{color:#ff7b7b;}.res-warn{color:#ffd166;}.res-ok{color:#35d07f;}.res-high{color:#93bbff;}.res-open{color:#a9b3c3;}.res-error{color:#ff5d5d;}
.target{width:128px;font-family:monospace;}.inline-form{display:inline-flex;gap:2px;align-items:center;flex-wrap:nowrap;white-space:nowrap;}.factory-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:12px;margin-top:14px;}.factory-grid .button{width:100%;text-align:center;margin:0;padding:13px 14px;}
@media(max-width:720px){.topbar-inner{align-items:flex-start}.tabs{width:100%;}.tab{flex:1;text-align:center;}table{font-size:13px;}td,th{padding:8px;}h1{font-size:24px;}})G5CSS";
static const char kApplicationJs[] PROGMEM = R"G5JS((function(){
  'use strict';
  function bindSimpleFileLoader(){
    var input=document.getElementById('fileInput');
    var target=document.getElementById('configText');
    if(!input||!target){return;}
    input.addEventListener('change',function(evt){
      var file=evt.target.files&&evt.target.files[0];
      if(!file){return;}
      var reader=new FileReader();
      reader.onload=function(e){target.value=e.target.result;};
      reader.readAsText(file);
    });
  }
  function bindCalibrationBundleLoader(){
    var input=document.getElementById('calibrationImportFile');
    var target=document.getElementById('calibrationBundleText');
    var button=document.getElementById('calibrationImportButton');
    var form=document.getElementById('calibrationImportForm');
    if(!input||!target||!button||!form){return;}
    var maxBytes=parseInt(form.getAttribute('data-max-bytes')||'16384',10);
    input.addEventListener('change',function(){
      button.disabled=true;target.value='';
      var file=input.files&&input.files[0];
      if(!file){return;}
      if(file.size>maxBytes){window.alert('Calibration file is too large.');input.value='';return;}
      var reader=new FileReader();
      reader.onload=function(e){target.value=String(e.target.result||'');button.disabled=(target.value.length===0);};
      reader.onerror=function(){window.alert('Could not read calibration file.');input.value='';};
      reader.readAsText(file);
    });
    form.addEventListener('submit',function(event){
      if(!target.value||!window.confirm('Restore all eight calibration tables from this file? Existing channel calibration files will be replaced.')){
        event.preventDefault();
      }
    });
  }
  function bindConfirmForms(){
    var forms=document.querySelectorAll('form[data-confirm]');
    for(var i=0;i<forms.length;i++){
      forms[i].addEventListener('submit',function(event){
        var message=this.getAttribute('data-confirm')||'Continue?';
        if(!window.confirm(message)){event.preventDefault();}
      });
    }
  }
  function startLiveState(){
    var target=document.getElementById('liveState');
    if(!target){return;}
    function update(){fetch('/state',{cache:'no-store'}).then(function(r){return r.text();}).then(function(t){target.textContent=t;}).catch(function(){});}
    update();
    window.setInterval(update,2000);
  }
  window.addEventListener('load',function(){bindSimpleFileLoader();bindCalibrationBundleLoader();bindConfirmForms();startLiveState();});
})();
)G5JS";

static void sendFlashAsset(const char* contentType, const char* cacheControl,
                           const char* payload, size_t payloadLength) {
  noteHttpRequest();
  server.sendHeader("Cache-Control", cacheControl);
  server.sendHeader("ETag", FIRMWARE_VERSION);
  server.send_P(200, contentType, payload, payloadLength);
}

void handleStaticCss() {
  sendFlashAsset("text/css; charset=utf-8", "public, max-age=86400, immutable",
                 kApplicationCss, sizeof(kApplicationCss) - 1U);
}

void handleStaticJs() {
  sendFlashAsset("application/javascript; charset=utf-8", "public, max-age=86400, immutable",
                 kApplicationJs, sizeof(kApplicationJs) - 1U);
}
