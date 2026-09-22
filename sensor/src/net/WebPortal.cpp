#include "WebPortal.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "logger.h"
#include "storage/DataLog.h"
#include "system/Diagnostics.h"

namespace web_portal {

static const int kPort = 80;
// Reachable as http://esp32-sensor.local on networks with mDNS (macOS, iOS,
// most Linux; Windows needs Bonjour).
static const char* kMdnsName = "esp32-sensor";
static WebServer server(kPort);
static DataLog* dataLog = nullptr;
static const Diagnostics* diag = nullptr;

// Static page; everything live comes from /status via fetch().
static const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Soil Sensor</title>
<style>
body{font:15px/1.4 -apple-system,system-ui,sans-serif;margin:0;padding:16px;background:#111;color:#ddd;max-width:720px}
h1{font-size:20px;margin:0 0 4px}h2{font-size:14px;text-transform:uppercase;letter-spacing:.05em;color:#8a8;margin:20px 0 6px}
table{border-collapse:collapse;width:100%}td{padding:4px 8px;border-bottom:1px solid #222;vertical-align:top}
td:first-child{color:#999;width:42%}
.ok{color:#6c6}.bad{color:#e66}.warn{color:#ec6}.mono{font-family:ui-monospace,monospace;font-size:13px;word-break:break-all}
#err{color:#e66;margin:8px 0}a{color:#8cf}.actions a{margin-right:16px}
</style></head><body>
<h1>ESP32 Soil Sensor</h1>
<div id="sub" class="mono"></div><div id="err"></div>
<h2>Sensor</h2><table id="sensor"></table>
<h2>Upload</h2><table id="upload"></table>
<h2>WiFi</h2><table id="wifi"></table>
<h2>Board</h2><table id="board"></table>
<h2>Config</h2><table id="config"></table>
<h2>Log</h2><p class="actions"><a href="/view">View CSV</a><a href="/download">Download CSV</a>
<a href="/clear" onclick="return confirm('Clear the log file?')">Clear log</a></p>
<script>
function rows(id,list){document.getElementById(id).innerHTML=list.map(([k,v,c])=>
  '<tr><td>'+k+'</td><td class="'+(c||'')+'">'+v+'</td></tr>').join('');}
function ago(s){return s==null?'':' ('+s+'s ago)';}
async function refresh(){
  try{
    const d=await (await fetch('/status',{cache:'no-store'})).json();
    document.getElementById('err').textContent='';
    document.getElementById('sub').textContent=d.firmware+' · up '+d.uptime_s+'s · '+d.clock.time;
    const r=d.reading;
    rows('sensor',r?[
      ['Last reading',r.timestamp+ago(r.age_s)],
      ['Millivolts',r.millivolts],
      ['Resistance',r.resistance_ohms<0?'open circuit':r.resistance_ohms+' Ω'],
      ['Quality',r.quality,r.quality=='ok'?'ok':r.quality=='open'?'bad':'warn'],
      ['ADC raw',r.adc_raw]]:[['Last reading','none yet','warn']]);
    const u=d.upload,c=d.config||{};
    rows('upload',[
      ['Enabled',c.http_enabled?'yes':'no'],
      ['Target',c.ingest_url||'','mono'],
      ['Last result',u.last_code+' '+u.last_result+ago(u.age_s),u.last_code==0?'':(u.last_code>=200&&u.last_code<300)?'ok':'bad'],
      ['Ok / failed / skipped',u.ok+' / '+u.failed+' / '+u.skipped]]);
    const w=d.wifi;
    rows('wifi',w.connected?[
      ['Connected','yes','ok'],['SSID',w.ssid],['IP',w.ip,'mono'],['Hostname',w.hostname],
      ['RSSI',w.rssi+' dBm',w.rssi>-70?'ok':w.rssi>-80?'warn':'bad'],
      ['Clock',d.clock.synced?'synced':'not synced',d.clock.synced?'ok':'warn']]
      :[['Connected','no','bad']]);
    rows('board',[
      ['Free heap',d.free_heap+' bytes'],['CPU',d.cpu_mhz+' MHz'],
      ['BLE',d.ble.enabled?(d.ble.connected?'connected':'advertising'):'disabled']]);
    rows('config',[
      ['Mode',c.mode],['Interval',(c.interval_ms/1000)+' s'],['Node',c.node_id],['Depth',c.depth_cm+' cm'],
      ['Divider',c.supply_millivolts+' mV / '+c.series_resistor_ohms+' Ω'],['ADC samples',c.adc_samples],
      ['Flash logging',c.data_logging_enabled?'on':'off']]);
  }catch(e){document.getElementById('err').textContent='Board not responding: '+e;}
}
refresh();setInterval(refresh,5000);
</script></body></html>)HTML";

static void handleRoot() {
    server.send_P(200, "text/html", kIndexHtml);
}

static void handleStatus() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", diag->toJson());
}

static void handleDownload() {
    logln("-----> Web: Download requested");

    if (!dataLog->exists()) {
        logln("-----> Web: File not found");
        server.send(404, "text/plain", "Log file not found");
        return;
    }

    File file = dataLog->openForRead();
    if (!file) {
        logln("-----> Web: Error opening file");
        server.send(500, "text/plain", "Error opening file");
        return;
    }

    logln("-----> Web: Streaming file (size: " + String(file.size()) + " bytes)");
    server.sendHeader("Content-Disposition", "attachment; filename=sensor_data.csv");
    server.streamFile(file, "text/csv");
    file.close();
    logln("-----> Web: Download complete");
}

static void handleView() {
    if (!dataLog->exists()) {
        server.send(404, "text/plain", "Log file not found");
        return;
    }

    File file = dataLog->openForRead();
    if (!file) {
        server.send(500, "text/plain", "Error opening file");
        return;
    }

    // Stream in chunks so a large log does not have to fit in heap.
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent("<html><head><title>Sensor Data</title></head><body><h1>Sensor Data Log</h1><pre>");

    char buf[256];
    while (file.available()) {
        size_t n = file.readBytes(buf, sizeof(buf));
        server.sendContent(buf, n);
    }
    file.close();

    server.sendContent("</pre><p><a href='/'>Back to Home</a></p></body></html>");
    server.sendContent("");
}

static void handleClear() {
    dataLog->clear();
    logln("-----> Log file cleared via HTTP");

    String html = "<html><head><title>Log Cleared</title></head><body>";
    html += "<h1>Log File Cleared</h1>";
    html += "<p>The log file has been cleared successfully.</p>";
    html += "<p><a href='/'>Back to Home</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

static void handleTest() {
    logln("-----> Web: Test endpoint hit");
    server.send(200, "text/plain", "ESP32 Web Server is working!");
}

static void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

void begin(DataLog& log, const Diagnostics& diagnostics) {
    dataLog = &log;
    diag = &diagnostics;

    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/test", handleTest);
    server.on("/download", handleDownload);
    server.on("/view", handleView);
    server.on("/clear", handleClear);
    server.onNotFound(handleNotFound);

    server.begin();
    logln("-----> Web server started on port " + String(kPort));
    logln("-----> Access at: http://" + WiFi.localIP().toString());

    if (MDNS.begin(kMdnsName)) {
        MDNS.addService("http", "tcp", kPort);
        logln("-----> Access at: http://" + String(kMdnsName) + ".local");
    } else {
        logln("-----> WARNING: mDNS failed to start");
    }
    logln("-----> Test endpoint: http://" + WiFi.localIP().toString() + "/test");
}

void handle() {
    server.handleClient();
}

}  // namespace web_portal
