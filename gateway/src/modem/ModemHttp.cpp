// ModemHttp.cpp
#include "ModemHttp.h"

ModemHttp::ModemHttp(HardwareSerial& serial) : modem(serial) {}

void ModemHttp::begin(uint32_t baud, int rxPin, int txPin) {
  modem.begin(baud, SERIAL_8N1, rxPin, txPin);
  delay(3000); // allow modem to boot
}

bool ModemHttp::post(const char* url, const String& body) {
  String cmd;
  String resp;
  int datalen = 0;

  // Clear any data that might be in the buffer
  while (modem.available()) {
    modem.read();
  }

  // 1. Terminate any previous session (multiple attempts)
  Serial.println("[HTTP] Terminating any previous session...");
  modem.println("AT+HTTPTERM");
  delay(500);
  while (modem.available()) modem.read();
  
  // Try terminating again to be sure
  modem.println("AT+HTTPTERM");
  delay(500);
  while (modem.available()) modem.read();

  // 2. Init HTTP service
  Serial.println("[HTTP] Initializing HTTP service...");
  modem.println("AT+HTTPINIT");
  delay(1000); // Longer delay for HTTP init
  
  // Wait for response with timeout
  unsigned long start = millis();
  String initResp = "";
  while (millis() - start < 5000) { // Longer timeout
    while (modem.available()) {
      char c = modem.read();
      initResp += c;
      // Look for OK or ERROR, ignore GNSS data
      if (initResp.indexOf("OK") != -1 || initResp.indexOf("ERROR") != -1) break;
      // If response gets too long, truncate (likely GNSS data)
      if (initResp.length() > 100) {
        initResp = initResp.substring(initResp.length() - 50); // Keep last 50 chars
      }
    }
    if (initResp.indexOf("OK") != -1 || initResp.indexOf("ERROR") != -1) break;
    delay(10);
  }
  Serial.println("[HTTP] HTTPINIT response: " + initResp.substring(max(0, (int)initResp.length() - 100)));
  
  if (initResp.indexOf("ERROR") != -1) {
    Serial.println("[HTTP] ⚠️ HTTP init failed, trying to continue anyway...");
    // Don't return false immediately - the service might already be running
    Serial.println("[HTTP] HTTP service might already be active");
  } else if (initResp.indexOf("OK") != -1) {
    Serial.println("[HTTP] ✅ HTTP initialization successful");
  } else {
    Serial.println("[HTTP] ⚠️ Unclear HTTP init response, continuing...");
  }

  // 3. Enable HTTPS
  Serial.println("[HTTP] Enabling HTTPS...");
  modem.println("AT+HTTPSSL=1");
  delay(500);
  while (modem.available()) modem.read(); // clear any residual data

  // 4. Set URL
  Serial.println("[HTTP] Setting URL...");
  cmd = "AT+HTTPPARA=\"URL\",\"" + String(url) + "\"";
  modem.println(cmd);
  delay(500);
  while (modem.available()) modem.read(); // clear GNSS data

  // 5. Set headers
  Serial.println("[HTTP] Setting headers...");
  modem.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(500);
  modem.println("AT+HTTPPARA=\"USERDATA\",\"x-source: telemetry.node\"");
  delay(200);
  modem.println("AT+HTTPPARA=\"USERDATA\",\"x-detail-type: telemetry\"");
  delay(200);

  // 6. Tell modem how many bytes we'll send
  Serial.println("[HTTP] Setting data length...");
  cmd = "AT+HTTPDATA=" + String(body.length()) + ",10000";
  modem.println(cmd);
  delay(2000);

  // 7. Wait for DOWNLOAD prompt
  Serial.println("[HTTP] Waiting for DOWNLOAD prompt...");
  unsigned long downloadStart = millis();
  while (millis() - downloadStart < 5000) {
    if (modem.find("DOWNLOAD")) {
      Serial.println("[HTTP] ✅ DOWNLOAD prompt received");
      break;
    }
  }
  
  if (millis() - downloadStart >= 5000) {
    Serial.println("[HTTP] ❌ DOWNLOAD prompt timeout");
    return false;
  }

  // 8. Send JSON body
  Serial.println("[HTTP] Sending JSON data...");
  modem.print(body);
  delay(1000);

  // 9. POST
  Serial.println("[HTTP] Executing POST...");
  modem.println("AT+HTTPACTION=1");
  modem.setTimeout(10000);
  if (modem.find("+HTTPACTION:")) {
    int method = modem.parseInt();   // should be 1 for POST
    int status = modem.parseInt();   // HTTP code
    datalen = modem.parseInt();    // response length
    Serial.printf("HTTP %d, %d bytes\n", status, datalen);
  }

  // 10. Read response
  cmd = "AT+HTTPREAD=0," + String(datalen);
  modem.println(cmd);
  modem.setTimeout(10000);

  delay(200); // let modem respond
  while (modem.available()) {
    char c = modem.read();
    Serial.write(c);  // raw dump to Serial
  }
  Serial.println();

  return resp.indexOf("ERROR") == -1;
}
