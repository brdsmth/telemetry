#include "WebPortal.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "logger.h"
#include "storage/DataLog.h"

namespace web_portal {

static const int kPort = 80;
static WebServer server(kPort);
static DataLog* dataLog = nullptr;

static void handleRoot() {
    logln("-----> Web: Root page requested");

    String html = "<html><head><title>ESP32 Sensor Data</title></head><body>";
    html += "<h1>ESP32 Sensor Data Logger</h1>";
    html += "<p><strong>Status:</strong> Running</p>";
    html += "<p><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</p>";
    html += "<p><strong>Free Heap:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
    if (dataLog->exists()) {
        html += "<p><strong>Log file size:</strong> " + String(dataLog->size()) + " bytes</p>";
    }
    html += "<h2>Actions:</h2>";
    html += "<p><a href='/download'>Download CSV File</a></p>";
    html += "<p><a href='/view'>View CSV in Browser</a></p>";
    html += "<p><a href='/clear' onclick='return confirm(\"Are you sure?\")'>Clear Log File</a></p>";
    html += "</body></html>";

    server.send(200, "text/html", html);
    logln("-----> Web: Root page sent");
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

void begin(DataLog& log) {
    dataLog = &log;

    server.on("/", handleRoot);
    server.on("/test", handleTest);
    server.on("/download", handleDownload);
    server.on("/view", handleView);
    server.on("/clear", handleClear);
    server.onNotFound(handleNotFound);

    server.begin();
    logln("-----> Web server started on port " + String(kPort));
    logln("-----> Access at: http://" + WiFi.localIP().toString());
    logln("-----> Test endpoint: http://" + WiFi.localIP().toString() + "/test");
}

void handle() {
    server.handleClient();
}

}  // namespace web_portal
