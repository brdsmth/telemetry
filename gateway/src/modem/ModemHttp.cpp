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

  // 1. Terminate any previous session
  modem.println("AT+HTTPTERM");
  delay(500);
  while (modem.available()) modem.read(); // clear buffer

  // 2. Init HTTP service
  modem.println("AT+HTTPINIT");
  delay(500);

  // 3. Enable HTTPS
  modem.println("AT+HTTPSSL=1");
  delay(500);

  // 4. Set URL
  cmd = "AT+HTTPPARA=\"URL\",\"" + String(url) + "\"";
  modem.println(cmd);
  delay(500);

  // 5. Set headers
  modem.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(500);
  modem.println("AT+HTTPPARA=\"USERDATA\",\"x-source: telemetry.node\"");
  delay(200);
  modem.println("AT+HTTPPARA=\"USERDATA\",\"x-detail-type: telemetry\"");
  delay(200);

  // 6. Tell modem how many bytes we’ll send
  cmd = "AT+HTTPDATA=" + String(body.length()) + ",10000";
  modem.println(cmd);
  delay(2000);

  // 7. Wait for DOWNLOAD prompt
  long start = millis();
  while (millis() - start < 5000) {
    if (modem.find("DOWNLOAD")) break;
  }

  // 8. Send JSON body
  modem.print(body);
  delay(1000);

  // 9. POST
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
