#include "config.h"
#include "system/Battery.h"
#include "modem/ModemHttp.h"
#include "telemetry/Telemetry.h"
#include "i2c_scanner.h"

ModemHttp http(MODEM);

Battery battery;  // Uses Wire on GPIO2 (SDA), GPIO3 (SCL) per Waveshare schematic

void setup() {
  Serial.begin(115200);
  http.begin(MODEM_BAUD, RX_PIN, TX_PIN);

  runI2CScanner();
  
  if (!battery.begin()) {
    Serial.println("❌ Battery gauge not detected!");
  } else {
    Serial.println("✅ Battery gauge initialized.");
  }

  Serial.println("Boot complete...");
}

void loop() {
  bool present = battery.isBatteryPresent();
  float voltage = present ? battery.readVoltage()   : 0.0f;
  float percent = present ? battery.readPercentage() : 0.0f;

  if (voltage > 0 && percent >= 0) {
    Serial.printf("Battery Voltage: %.2f V | Level: %.2f%%\n", voltage, percent); 
  } else {
    Serial.println("Battery reading unavailable.");
  }

  Telemetry t = {"SIM7670G", "test", "2024-01-20T12:00:00Z", present, voltage, percent};
  String json = t.toJson();

  Serial.println("Sending POST to " + String(SERVER_URL));
  if (http.post(SERVER_URL, json)) {
    Serial.println("✅ POST success!");
  } else {
    Serial.println("❌ POST failed!");
  }

  delay(10000);
}