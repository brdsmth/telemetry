#include "config.h"
#include "modem/ModemHttp.h"
#include "telemetry/Telemetry.h"

ModemHttp http(MODEM);

void setup() {
  Serial.begin(115200);
  http.begin(MODEM_BAUD, RX_PIN, TX_PIN);
  Serial.println("Boot complete...");
}

void loop() {
  Telemetry t = {"SIM7670G", "test", "2024-01-20T12:00:00Z"};
  String json = t.toJson();

  Serial.println("Sending POST to " + String(SERVER_URL));
  if (http.post(SERVER_URL, json)) {
    Serial.println("✅ POST success!");
  } else {
    Serial.println("❌ POST failed!");
  }

  delay(10000);
}