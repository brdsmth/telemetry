#include <Arduino.h>
#include <ArduinoJson.h>

#define MODEM Serial1   // SIM7670 connected to Serial1
static const uint32_t MODEM_BAUD = 115200;
static const int RXPin = 17;
static const int TXPin = 18;

// Replace with your API Gateway endpoint
const char* serverUrl = "https://api.autostrux.com/ingest";

// Build JSON body with ArduinoJson
String buildJson() {
  JsonDocument doc;   // auto-sizing, replaces StaticJsonDocument
  doc["device"] = "SIM7670G";
  doc["status"] = "test";
  doc["timestamp"] = "2024-01-20T12:00:00Z";

  String json;
  serializeJson(doc, json);
  return json;
}

bool sendPost(const char* url, const String& body) {
  String cmd;
  String resp;

  // 1. Terminate any previous session
  MODEM.println("AT+HTTPTERM");
  delay(500);
  // This clears the buffer on the modem
  while (MODEM.available()) MODEM.read(); // clear buffer

  // 2. Init HTTP service
  MODEM.println("AT+HTTPINIT");
  delay(500);

  // 3. Enable HTTPS
  MODEM.println("AT+HTTPSSL=1");
  delay(500);

  // 4. Set URL
  cmd = "AT+HTTPPARA=\"URL\",\"" + String(url) + "\"";
  MODEM.println(cmd);
  delay(500);

  // 5. Set Content-Type
  MODEM.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(500);
  MODEM.println("AT+HTTPPARA=\"USERDATA\",\"x-source: telemetry.node\"");
  delay(200);
  MODEM.println("AT+HTTPPARA=\"USERDATA\",\"x-detail-type: telemetry\"");
  delay(200);

  // 6. Tell modem how many bytes we’ll send
  cmd = "AT+HTTPDATA=" + String(body.length()) + ",10000";
  MODEM.println(cmd);
  delay(2000);

  // 7. Wait for DOWNLOAD prompt
  long start = millis();
  while (millis() - start < 5000) {
    if (MODEM.find("DOWNLOAD")) break;
  }

  // 8. Send JSON body
  MODEM.print(body);
  delay(1000);

  // 9. POST
  MODEM.println("AT+HTTPACTION=1");
  MODEM.setTimeout(10000);
  if (MODEM.find("+HTTPACTION:")) {
    int method = MODEM.parseInt();   // should be 1 for POST
    int status = MODEM.parseInt();   // HTTP code
    int datalen = MODEM.parseInt();  // response length
    Serial.printf("HTTP %d, %d bytes\n", status, datalen);
  }

  // 10. Read response
  MODEM.println("AT+HTTPREAD");
  MODEM.setTimeout(10000);
  if (MODEM.find("+HTTPREAD:")) {
    int len = MODEM.parseInt();
    Serial.printf("Expecting %d bytes\n", len);
    for (int i = 0; i < len; i++) {
      while (!MODEM.available());  // wait for each byte
      resp += (char)MODEM.read();
    }
  }

  Serial.println("HTTPREAD Response: ");
  Serial.println(resp);

  // crude check
  return resp.indexOf("ERROR") == -1;
}

void setup() {
  Serial.begin(115200);
  // MODEM.begin(115200);
  MODEM.begin(MODEM_BAUD, SERIAL_8N1, RXPin, TXPin);

  delay(3000);
  Serial.println("Boot complete...");
}

void loop() {
  // Nothing, one-shot test
  String json = buildJson();
  Serial.println("JSON to send: " + json);
  Serial.println("Sending POST request to " + String(serverUrl));

  if (sendPost(serverUrl, json)) {
    Serial.println("✅ POST success!");
  } else {
    Serial.println("❌ POST failed!");
  }

  Serial.println("Sleeping for 10 seconds...");
  delay(10000);
}
