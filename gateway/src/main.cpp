#include "config.h"
#include "system/Battery.h"
#include "modem/ModemHttp.h"
#include "modem/ModemGNSS.h"
#include "telemetry/Telemetry.h"
#include "i2c_scanner.h"

ModemHttp http(MODEM);
ModemGNSS gnss(MODEM);

Battery battery;  // Uses Wire on GPIO2 (SDA), GPIO3 (SCL) per Waveshare schematic

// --- GNSS state tracking ---
int GNSS_TIMEOUT_MS = 10 * 60 * 1000; // 10 minutes
float lastLat = 0.0f;
float lastLon = 0.0f;
bool hasFix   = false;

void setup() {
  Serial.begin(115200);
  delay(2000);  // Give serial time to initialize
  
  Serial.println("\n=== TELEMETRY GATEWAY STARTING ===");
  
  // Initialize modem communication
  http.begin(MODEM_BAUD, RX_PIN, TX_PIN);
  delay(1000);
  
  // Run hardware diagnostics
  Serial.println("\n--- Hardware Initialization ---");
  runI2CScanner();
  
  if (!battery.begin()) {
    Serial.println("❌ Battery gauge not detected!");
  } else {
    Serial.println("✅ Battery gauge initialized.");
  }

  Serial.println("\n--- Board Info ---");
  Serial.println("🏗️  Board: Waveshare ESP32-S3-SIM7670G-4G");
  Serial.println("📍 GNSS: Will be activated after HTTP operations");
  Serial.println("📶 For best results: Place antenna outdoors with clear sky view");

  Serial.println("\n=== BOOT COMPLETE ===");
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

  Telemetry t = {
    "SIM7670G",
    "test",
    "2024-01-20T12:00:00Z",
    present,
    voltage,
    percent,
    hasFix ? lastLat : 0.0f,
    hasFix ? lastLon : 0.0f
  };

  String json = t.toJson();

  // === HTTP OPERATION (completely separate from GNSS) ===
  Serial.println("\n--- HTTP POST Operation ---");
  Serial.println("📤 Sending POST to " + String(SERVER_URL));
  
  // Ensure completely clean start for HTTP
  while (MODEM.available()) {
    MODEM.read();
  }
  delay(500);
  
  if (http.post(SERVER_URL, json)) {
    Serial.println("✅ POST success!");
  } else {
    Serial.println("❌ POST failed!");
  }
  
  // Wait for HTTP to completely finish and clear any residual data
  Serial.println("🧹 Clearing HTTP session...");
  delay(2000);
  while (MODEM.available()) {
    MODEM.read();
  }
  
  // === GNSS OPERATION (continuous for fast fixes) ===
  Serial.println("\n--- GNSS Fix Acquisition ---");
  
  unsigned long start = millis();
  int attempts = 0;
  const int maxAttempts = 8; // Reduced since we should get faster fixes

  // Only start GNSS on first cycle or if we lost the fix
  if (!hasFix) {
    Serial.println("🛰️ Starting GNSS (first time or after fix loss)...");
    gnss.start();
    delay(3000); // Give GNSS time to start
  } else {
    Serial.println("🛰️ GNSS running continuously - expecting fast fix...");
  }

  if (!hasFix) {
    Serial.println("[GNSS] ⏳ Cold start - may take 1-15 minutes for first fix");
  } else {
    Serial.println("[GNSS] ⚡ Warm start - should get fix in ~30 seconds");
  }
  Serial.println("[GNSS] 💡 For best results, ensure:");
  Serial.println("    - Clear sky view (no buildings/trees overhead)");
  Serial.println("    - Stationary position during acquisition");
  Serial.println("    - Good antenna connection");

  while (!hasFix && attempts < maxAttempts) {
    attempts++;
    Serial.printf("[GNSS] 🛰️  Attempt %d/%d (%.1f minutes elapsed)...\n", 
                  attempts, maxAttempts, (millis() - start) / 60000.0);
    
    if (gnss.pollFix(lastLat, lastLon)) {
      hasFix = true;
      Serial.printf("[GNSS] 🎯 First fix acquired! lat=%.6f, lon=%.6f\n", lastLat, lastLon);
      Serial.printf("[GNSS] 🕒 Fix time: %.1f minutes\n", (millis() - start) / 60000.0);
      break;
    }

    // Longer delay for satellite acquisition
    delay(10000); // wait 10 seconds between polls
  }

  gnss.stop();

  if (!hasFix) {
    Serial.printf("[GNSS] ❌ No fix after %d attempts (%.1f minutes)\n", 
                  attempts, (millis() - start) / 60000.0);
    Serial.println("[GNSS] 🔧 Waveshare Board Troubleshooting:");
    Serial.println("  1. 📡 ANTENNA: Connect external GNSS antenna to GNSS SMA connector");
    Serial.println("  2. 🌍 LOCATION: Move to open area (parking lot, field, rooftop)");
    Serial.println("  3. ⏰ TIME: Allow 15+ minutes for cold start acquisition");
    Serial.println("  4. 🔌 POWER: Ensure board has adequate power (USB or battery)");
    Serial.println("  5. 🛰️ TIMING: Try different times of day");
    Serial.println("  6. 📶 INTERFERENCE: Move away from WiFi routers, computers");
    Serial.println("  7. 🔧 HARDWARE: Check SMA connector and antenna cable");
    Serial.println("  8. 📋 FIRMWARE: Verify SIM7670G module firmware version");
  } else {
    Serial.printf("[GNSS] ✅ Using coordinates: lat=%.6f, lon=%.6f\n", lastLat, lastLon);
  }

  // --- Wait before next cycle ---
  Serial.println("\n=== Cycle Complete ===");
  if (hasFix) {
    Serial.println("⏱️ GNSS has fix - waiting 60 seconds before next cycle...");
    delay(60000); // 1 minute cycle when we have a fix
  } else {
    Serial.println("⏱️ No GNSS fix yet - waiting 30 seconds before retry...");
    delay(30000); // 30 second retry when no fix
  }
}