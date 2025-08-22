// === System includes ===
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <esp_now.h>
#include <NimBLEDevice.h>
#include <SPIFFS.h>
#include <time.h>
#include <ArduinoJson.h>
extern "C" {
  #include "esp_wifi.h"
}

// === Project includes ===
#include "logger.h"
#include "espnow_utils.h"
#include "wifi_manager.h"


// === Configuration (loaded from SPIFFS config.json) ===
bool WIFI_ENABLED = true;
bool WIFI_HTTP_ENABLED = true;
String WIFI_HTTP_URL = "http://192.168.1.19:8000/";
bool BLE_ENABLED = true;
bool WEB_SERVER_ENABLED = true;
bool DATA_LOGGING_ENABLED = true;
bool SYSTEM_STATUS_ENABLED = true;
unsigned long sensorInterval = 10000; // milliseconds

// === Static Configuration ===
#define CONFIG_FILE_PATH      "/config.json"
#define LOG_FILE_PATH         "/sensor_data.csv"
#define MAX_LOG_ENTRIES       1000
#define WEB_SERVER_PORT       80
#define NTP_SERVER            "pool.ntp.org"
#define GMT_OFFSET_SEC        0
#define DAYLIGHT_OFFSET_SEC   0

// === Sensor Configuration ===
#define POWER_PIN     23  // Power supply for breakout board
#define SENSOR_1_PIN  32  // Analog input from voltage divider 1 (ADC1_CH4)
#define SENSOR_2_PIN  33  // Analog input from voltage divider 2 (ADC1_CH5)
#define SENSOR_3_PIN  34  // Analog input from voltage divider 3 (ADC1_CH6)

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define RECEIVE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // For sending data TO mobile (READ/NOTIFY)
#define TRANSFER_CHARACTERISTIC_UUID "beb5483f-36e1-4688-b7f5-ea07361b26a9" // For receiving data FROM mobile (WRITE)
NimBLECharacteristic* bleDataRead = nullptr;   // Characteristic for mobile to read from
NimBLECharacteristic* bleDataWrite = nullptr;  // Characteristic for mobile to write to
bool bleConnected = false;

// === Web Server ===
WebServer server(WEB_SERVER_PORT);

class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        bleConnected = true;
        logln("-----> BLE: Device connected");
    }
    void onDisconnect(NimBLEServer* pServer) {
        bleConnected = false;
        logln("-----> BLE: Device disconnected");
    }
};

class CharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            logln("-----> BLE: Received data: " + String(value.c_str()));
        }
    }
};

void setupBLE() {
    NimBLEDevice::init("ESP32");
    
    // Set MTU to maximum (512 bytes) for larger packets
    NimBLEDevice::setMTU(512);
    
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
    // Characteristic for mobile to READ from (sensor sends data TO mobile)
    bleDataRead = pService->createCharacteristic(
        RECEIVE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    
    // Set initial value to avoid garbage data
    String initialValue = "{\"test\":\"ESP32 Ready\",\"count\":0}";
    bleDataRead->setValue((uint8_t*)initialValue.c_str(), initialValue.length());
    
    // Characteristic for mobile to WRITE to (mobile sends data TO sensor)
    bleDataWrite = pService->createCharacteristic(
        TRANSFER_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    bleDataWrite->setCallbacks(new CharacteristicCallbacks());
    
    pService->start();
    NimBLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
    NimBLEDevice::startAdvertising();
    logln("-----> BLE: Advertising as 'ESP32' with READ and WRITE characteristics");
}

void updateBLE(const String& data) {
    if (bleDataRead) {
        logln("-----> BLE: Setting value (length: " + String(data.length()) + "): " + data);
        
        // Set value with explicit length to avoid truncation
        bleDataRead->setValue((uint8_t*)data.c_str(), data.length());
        
        if (bleConnected) {
            bleDataRead->notify();
            logln("-----> BLE: Notification sent to connected device");
        } else {
            logln("-----> BLE: Value updated but no device connected");
        }
    } else {
        logln("-----> BLE: ERROR - bleDataRead is null!");
    }
}

// === Load Configuration from SPIFFS ===
bool loadConfig() {
    logln("-----> Loading configuration from SPIFFS...");
    
    if (!SPIFFS.exists(CONFIG_FILE_PATH)) {
        logln("-----> WARNING: config.json not found, using defaults");
        return false;
    }
    
    File file = SPIFFS.open(CONFIG_FILE_PATH, FILE_READ);
    if (!file) {
        logln("-----> ERROR: Could not open config.json");
        return false;
    }
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        logln("-----> ERROR: Failed to parse config.json: " + String(error.c_str()));
        return false;
    }
    
    // Load configuration values
    WIFI_ENABLED = doc["wifi_enabled"] | true;
    WIFI_HTTP_ENABLED = doc["wifi_http_enabled"] | true;
    WIFI_HTTP_URL = doc["wifi_http_url"] | "http://192.168.1.19:8000/";
    BLE_ENABLED = doc["ble_enabled"] | true;
    WEB_SERVER_ENABLED = doc["web_server_enabled"] | true;
    DATA_LOGGING_ENABLED = doc["data_logging_enabled"] | true;
    SYSTEM_STATUS_ENABLED = doc["system_status_enabled"] | true;
    sensorInterval = doc["sensor_interval_ms"] | 10000;
    
    logln("-----> Configuration loaded successfully");
    return true;
}

// === NTP Time Sync Function ===
void syncNTPTime() {
    logln("-----> Syncing time with NTP server...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    // Wait for time to be set
    int retry = 0;
    const int maxRetries = 10;
    
    while (time(nullptr) < 100000 && retry < maxRetries) {
        delay(1000);
        Serial.print(".");
        retry++;
    }
    
    if (retry < maxRetries) {
        logln("\n-----> Time synced successfully!");
        time_t now = time(nullptr);
        logln("-----> Current time: " + String(ctime(&now)));
    } else {
        logln("\n-----> WARNING: Time sync failed, will use millis() fallback");
    }
}

// === Web Server Handlers ===
void handleRoot() {
    logln("-----> Web: Root page requested");
    
    String html = "<html><head><title>ESP32 Sensor Data</title></head><body>";
    html += "<h1>ESP32 Sensor Data Logger</h1>";
    html += "<p><strong>Status:</strong> Running</p>";
    html += "<p><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</p>";
    html += "<p><strong>Free Heap:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
    
    // Get file info
    if (SPIFFS.exists(LOG_FILE_PATH)) {
        File file = SPIFFS.open(LOG_FILE_PATH, FILE_READ);
        if (file) {
            size_t fileSize = file.size();
            file.close();
            html += "<p><strong>Log file size:</strong> " + String(fileSize) + " bytes</p>";
        }
    }
    
    html += "<h2>Actions:</h2>";
    html += "<p><a href='/download'>Download CSV File</a></p>";
    html += "<p><a href='/view'>View CSV in Browser</a></p>";
    html += "<p><a href='/clear' onclick='return confirm(\"Are you sure?\")'>Clear Log File</a></p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
    logln("-----> Web: Root page sent");
}

void handleDownload() {
    logln("-----> Web: Download requested");
    
    if (!SPIFFS.exists(LOG_FILE_PATH)) {
        logln("-----> Web: File not found");
        server.send(404, "text/plain", "Log file not found");
        return;
    }
    
    File file = SPIFFS.open(LOG_FILE_PATH, FILE_READ);
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

void handleView() {
    if (!SPIFFS.exists(LOG_FILE_PATH)) {
        server.send(404, "text/plain", "Log file not found");
        return;
    }
    
    File file = SPIFFS.open(LOG_FILE_PATH, FILE_READ);
    if (!file) {
        server.send(500, "text/plain", "Error opening file");
        return;
    }
    
    String content = "<html><head><title>Sensor Data</title></head><body>";
    content += "<h1>Sensor Data Log</h1>";
    content += "<pre>";
    
    while (file.available()) {
        content += (char)file.read();
    }
    
    content += "</pre>";
    content += "<p><a href='/'>Back to Home</a></p>";
    content += "</body></html>";
    
    file.close();
    server.send(200, "text/html", content);
}

void handleClear() {
    if (SPIFFS.exists(LOG_FILE_PATH)) {
        SPIFFS.remove(LOG_FILE_PATH);
    }
    
    // Recreate with header
    File file = SPIFFS.open(LOG_FILE_PATH, FILE_WRITE);
    if (file) {
        file.println("timestamp,sensor_1,sensor_2,sensor_3");
        file.close();
    }
    
    logln("-----> Log file cleared via HTTP");
    
    String html = "<html><head><title>Log Cleared</title></head><body>";
    html += "<h1>Log File Cleared</h1>";
    html += "<p>The log file has been cleared successfully.</p>";
    html += "<p><a href='/'>Back to Home</a></p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleTest() {
    logln("-----> Web: Test endpoint hit");
    server.send(200, "text/plain", "ESP32 Web Server is working!");
}

void handleNotFound() {
    // Handle 404 and favicon requests without logging
    server.send(404, "text/plain", "Not found");
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/test", handleTest);
    server.on("/download", handleDownload);
    server.on("/view", handleView);
    server.on("/clear", handleClear);
    server.onNotFound(handleNotFound);  // Handle favicon.ico and other requests
    
    server.begin();
    logln("-----> Web server started on port " + String(WEB_SERVER_PORT));
    logln("-----> Access at: http://" + WiFi.localIP().toString());
    logln("-----> Test endpoint: http://" + WiFi.localIP().toString() + "/test");
}

// === SPIFFS Initialization ===
bool initSPIFFS() {
    logln("-----> Initializing SPIFFS...");
    
    if (!SPIFFS.begin(true)) {
        logln("-----> ERROR: SPIFFS mount failed!");
        return false;
    }
    
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    logln("-----> SPIFFS mounted successfully");
    logln("-----> Total: " + String(totalBytes) + " bytes");
    logln("-----> Used: " + String(usedBytes) + " bytes");
    logln("-----> Free: " + String(totalBytes - usedBytes) + " bytes");
    
    // Create CSV file with header if it doesn't exist
    if (!SPIFFS.exists(LOG_FILE_PATH)) {
        logln("-----> Creating new log file...");
        File file = SPIFFS.open(LOG_FILE_PATH, FILE_WRITE);
        if (file) {
            file.println("timestamp,sensor_1,sensor_2,sensor_3");
            file.close();
            logln("-----> Log file created with header");
        } else {
            logln("-----> ERROR: Could not create log file!");
            return false;
        }
    } else {
        logln("-----> Log file already exists");
    }
    
    return true;
}

// === Get Formatted Timestamp ===
String getTimestamp() {
    time_t now = time(nullptr);
    
    // Check if time is valid (more than Jan 1, 2020)
    if (now > 1577836800) {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        char buffer[30];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(buffer);
    } else {
        // Fallback to millis if NTP hasn't synced
        return String(millis() / 1000);
    }
}

// === Log Data to SPIFFS ===
void logDataToSPIFFS(float sensor1, float sensor2, float sensor3) {
    if (!DATA_LOGGING_ENABLED) return;
    
    String timestamp = getTimestamp();
    String csvLine = timestamp + "," + 
                     String(sensor1, 3) + "," + 
                     String(sensor2, 3) + "," + 
                     String(sensor3, 3);
    
    // Append to file
    File file = SPIFFS.open(LOG_FILE_PATH, FILE_APPEND);
    if (file) {
        file.println(csvLine);
        file.close();
        logln("-----> Data logged: " + csvLine);
    } else {
        logln("-----> ERROR: Could not open log file for writing!");
    }
}

// === WiFi Connection Function ===
bool connectToWiFi(const char* ssid, const char* password) {
    logln("\n-----> Connecting to WiFi...");
    logln("-----> SSID: " + String(ssid));
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    logln("-----> Waiting for connection...");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
     
    if (WiFi.status() == WL_CONNECTED) {
        logln("-----> WiFi CONNECTED!");
        logln("-----> IP: " + WiFi.localIP().toString());
        logln("-----> RSSI: " + String(WiFi.RSSI()) + "dBm");
        return true;
    } else {
        logln("-----> WiFi FAILED! Status: " + String(WiFi.status()));
        return false;
    }
}

// === HTTP Request Function ===
bool sendHTTPRequest(const char* url, const char* payload = nullptr) {
    if (WiFi.status() != WL_CONNECTED) {
        logln("-----> ERROR: WiFi not connected");
        return false;
    }
    
    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000); // 3 second timeout to avoid long blocking
    
    // Set headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-Sensor/1.0");
    
    int httpResponseCode;
    
    if (payload != nullptr) {
        // POST request with payload
        logln("-----> Sending POST to: " + String(url));
        logln("-----> Payload: " + String(payload));
        httpResponseCode = http.POST(payload);
    } else {
        // GET request
        logln("-----> Sending GET to: " + String(url));
        httpResponseCode = http.GET();
    }
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        logln("-----> HTTP Response Code: " + String(httpResponseCode));
        logln("-----> Response: " + response);
        
        http.end();
        return (httpResponseCode >= 200 && httpResponseCode < 300);
    } else {
        logln("-----> HTTP Request failed, error: " + String(httpResponseCode));
        http.end();
        return false;
    }
}

// === Sensor Reading Function ===
String readSensors(float& voltage1, float& voltage2, float& voltage3) {
    int sensor1 = analogRead(SENSOR_1_PIN);
    int sensor2 = analogRead(SENSOR_2_PIN);
    int sensor3 = analogRead(SENSOR_3_PIN);
    
    // Convert to voltage (ESP32 ADC is 12-bit: 0-4095 maps to 0-3.3V by default)
    voltage1 = sensor1 * (3.3 / 4095.0);
    voltage2 = sensor2 * (3.3 / 4095.0);
    voltage3 = sensor3 * (3.3 / 4095.0);
    
    logln("-----> Sensor 1 (32): " + String(voltage1, 2) + "V (raw: " + String(sensor1) + ")");
    logln("-----> Sensor 2 (33): " + String(voltage2, 2) + "V (raw: " + String(sensor2) + ")");
    logln("-----> Sensor 3 (34): " + String(voltage3, 2) + "V (raw: " + String(sensor3) + ")");
    
    // Create JSON payload using ArduinoJson
    StaticJsonDocument<256> doc;  // 256 bytes should be plenty for our small JSON
    doc["timestamp"] = getTimestamp();
    doc["sensor_1"] = serialized(String(voltage1, 2));
    doc["sensor_2"] = serialized(String(voltage2, 2));
    doc["sensor_3"] = serialized(String(voltage3, 2));

    String json;
    serializeJson(doc, json);
    
    logln("-----> JSON payload: " + json);
    logln("-----> JSON length: " + String(json.length()));
    
    return json;
}

// === System Status Function ===
void printSystemStatus() {
    logln("-----> System Status Check...");
    
    // Free heap memory
    uint32_t freeHeap = ESP.getFreeHeap();
    logln("-----> Free heap: " + String(freeHeap) + " bytes");
    
    // WiFi signal strength
    if (WiFi.status() == WL_CONNECTED) {
      int rssi = WiFi.RSSI();
      logln("-----> WiFi RSSI: " + String(rssi) + "dBm");
    }
    
    // Uptime
    unsigned long uptime = millis();
    logln("-----> Uptime: " + String(uptime / 1000) + " seconds");
    
    // CPU frequency
    uint32_t cpuFreq = ESP.getCpuFreqMHz();
    logln("-----> CPU Frequency: " + String(cpuFreq) + " MHz");
}

void printConfig() {
    logln("\n=== CONFIGURATION ===");
    logln("-----> WiFi Enabled: " + String(WIFI_ENABLED ? "YES" : "NO"));
    logln("-----> WiFi HTTP Enabled: " + String(WIFI_HTTP_ENABLED ? "YES" : "NO"));
    logln("-----> WiFi HTTP URL: " + WIFI_HTTP_URL);
    logln("-----> BLE Enabled: " + String(BLE_ENABLED ? "YES" : "NO"));
    logln("-----> Web Server Enabled: " + String(WEB_SERVER_ENABLED ? "YES" : "NO"));
    logln("-----> Data Logging Enabled: " + String(DATA_LOGGING_ENABLED ? "YES" : "NO"));
    logln("-----> System Status Enabled: " + String(SYSTEM_STATUS_ENABLED ? "YES" : "NO"));
    logln("-----> Sensor Interval: " + String(sensorInterval) + "ms");
    logln("=== END CONFIGURATION ===\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize sensor power pin
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH);
    logln("-----> Power pin " + String(POWER_PIN) + " set to HIGH");
    
    // Initialize analog input pins
    pinMode(SENSOR_1_PIN, INPUT);
    pinMode(SENSOR_2_PIN, INPUT);
    pinMode(SENSOR_3_PIN, INPUT);
    logln("-----> Sensor pins initialized");
    
    // Initialize SPIFFS
    initSPIFFS();
    
    // Load configuration from SPIFFS
    loadConfig();
    
    // Print configuration
    printConfig();
    
    // if (BLE_ENABLED) setupBLE();
    
    // Connect to WiFi
    if (WIFI_ENABLED) {
        if (connectToWiFi("SpectrumSetup-8A38", "mainbelt780")) {
            // Sync time via NTP after WiFi connects
            syncNTPTime();
            
            // Start web server
            if (WEB_SERVER_ENABLED) {
                setupWebServer();
            }
        }
    }
    
    logln("\n === SETUP COMPLETE ===");
}

// === Main loop ===
// 
// ARCHITECTURE NOTE: Web Server Responsiveness
// 
// The web server requires frequent calls to server.handleClient() to remain responsive.
// Since we have blocking operations (HTTP POST, SPIFFS writes, sensor reads) that can take
// 100ms-1000ms each, we intersperse server.handleClient() calls throughout the loop.
// 
// Without this pattern:
//   - Browser requests timeout (browsers expect responses within seconds)
//   - Favicon.ico requests fail, causing broken page loads
//   - User interface becomes unusable during sensor reading cycles
// 
// With this pattern:
//   - Web server can respond to requests at any time, even during sensor readings
//   - Each server.handleClient() call processes one pending HTTP request (if any)
//   - Total overhead is <1ms per call, negligible compared to blocking operations
// 
// The multiple calls ensure the web interface remains usable while maintaining
// the sensor reading schedule (configurable via config.json).
//
unsigned long lastSensorRead = 0;

void loop() {
    // Handle web server requests in the main loop (always responsive)
    if (WEB_SERVER_ENABLED && WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }
    
    // Read sensors at configured interval
    unsigned long currentMillis = millis();
    if (currentMillis - lastSensorRead >= sensorInterval) {
        lastSensorRead = currentMillis;
        
        logln("\n === INNER LOOP ===");

        // Read sensor data
        float voltage1, voltage2, voltage3;
        String jsonPayload = readSensors(voltage1, voltage2, voltage3);

        // Log data to SPIFFS
        if (DATA_LOGGING_ENABLED) {
            logDataToSPIFFS(voltage1, voltage2, voltage3);
        }

        // Send via HTTP (blocking operation, can take up to 3 seconds with timeout)
        if (WIFI_HTTP_ENABLED && WiFi.status() == WL_CONNECTED) {
            sendHTTPRequest(WIFI_HTTP_URL.c_str(), jsonPayload.c_str());
        }

        // Send via BLE
        if (BLE_ENABLED) {
            updateBLE(jsonPayload);
        }

        if (SYSTEM_STATUS_ENABLED) {
            printSystemStatus();
        }

        int pins[] = {32,33,34,35,36,39};

        for (int i = 0; i < 6; i++) {
            Serial.printf("GPIO %d = %d\n", pins[i], analogRead(pins[i])); // added: scan ADC pins
        }
        Serial.println("----");

        logln("\n === LOOP COMPLETE ===");
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}
