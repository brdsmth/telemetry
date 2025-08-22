// ModemGNSS.cpp
#include "ModemGNSS.h"

ModemGNSS::ModemGNSS(HardwareSerial& serial) : modem(serial) {}

void ModemGNSS::start() {
  Serial.println("[GNSS] === GNSS Initialization Start ===");
  
  modem.setTimeout(2000);
  
  // Check current GNSS power status
  Serial.println("[GNSS] Checking power status...");
  modem.println("AT+CGNSSPWR?");
  delay(500);
  String resp = modem.readString();
  Serial.println("[GNSS] Power status response: " + resp);

  if (resp.indexOf("+CGNSSPWR: 1") != -1) {
      Serial.println("[GNSS] Already powered on, continuing with configuration...");
  } else {
      Serial.println("[GNSS] Powering ON...");
      modem.println("AT+CGNSSPWR=1");
      delay(1000);
      if (modem.find("OK")) {
          Serial.println("[GNSS] Power ON successful");
      } else {
          Serial.println("[GNSS] ❌ Failed to power on GNSS!");
          return;
      }
  }

  // Try alternative constellation configuration commands
  Serial.println("[GNSS] Configuring constellations...");
  
  // Try the newer format first
  modem.println("AT+CGNSSMODE=1,1,1,1");  // GPS, GLONASS, BeiDou, Galileo
  delay(500);
  if (modem.find("OK")) {
      Serial.println("[GNSS] Constellation configuration successful (4-param)");
  } else {
      // Try older format
      Serial.println("[GNSS] Trying alternative constellation config...");
      modem.println("AT+CGNSSMODE=1");  // GPS only for compatibility
      delay(500);
      if (modem.find("OK")) {
          Serial.println("[GNSS] GPS-only configuration successful");
      } else {
          Serial.println("[GNSS] ⚠️ All constellation configs failed, using defaults");
      }
  }

  // Enable GNSS data output (Waveshare specific)
  Serial.println("[GNSS] Enabling GNSS data output...");
  modem.println("AT+CGNSSTST=1");  // Enable GNSS test mode for Waveshare board
  delay(500);
  if (modem.find("OK")) {
      Serial.println("[GNSS] GNSS data output enabled");
  } else {
      Serial.println("[GNSS] ⚠️ GNSS data output enable failed, continuing...");
  }

  // Skip antenna command if it's not supported
  Serial.println("[GNSS] Skipping antenna check (command not supported by this module)");

  Serial.println("[GNSS] === GNSS Initialization Complete ===");
}

void ModemGNSS::stop() {
  Serial.println("[GNSS] Stopping GNSS...");
  
  // Try multiple commands to stop GNSS output
  Serial.println("[GNSS] Sending AT+CGNSSTST=0...");
  modem.println("AT+CGNSSTST=0");
  delay(1000);
  while (modem.available()) modem.read();
  
  Serial.println("[GNSS] Sending AT+CGNSSPWR=0...");
  modem.println("AT+CGNSSPWR=0");
  delay(1000);
  while (modem.available()) modem.read();
  
  // Try alternative command to disable NMEA output
  Serial.println("[GNSS] Trying AT+CGNSSPORTSWITCH=0,0...");
  modem.println("AT+CGNSSPORTSWITCH=0,0");
  delay(1000);
  while (modem.available()) modem.read();
  
  // Try to reset GNSS completely
  Serial.println("[GNSS] Sending AT+CGNSRST (GNSS reset)...");
  modem.println("AT+CGNSRST");
  delay(2000); // Longer delay for reset
  
  // Aggressive final clearing
  Serial.println("[GNSS] Final aggressive buffer clear...");
  unsigned long clearStart = millis();
  int clearedBytes = 0;
  while (millis() - clearStart < 5000) { // 5 second clear window
    while (modem.available()) {
      modem.read();
      clearedBytes++;
    }
    delay(100);
  }
  
  Serial.printf("[GNSS] Stop sequence complete. Cleared %d bytes.\n", clearedBytes);
}

bool ModemGNSS::pollFix(float& lat, float& lon) {
  modem.setTimeout(3000);
  
  // Use Waveshare-specific command
  modem.println("AT+CGPSINFO");
  delay(500);

  String resp;
  unsigned long start = millis();
  while (millis() - start < 2000) {  // Wait up to 2 seconds for response
    while (modem.available()) {
      resp += (char)modem.read();
    }
    if (resp.indexOf("OK") != -1 || resp.indexOf("ERROR") != -1) break;
    delay(10);
  }

  Serial.println("[GNSS] Raw response: " + resp);

  // Check for Waveshare CGPSINFO response first
  if (resp.indexOf("+CGPSINFO:") != -1) {
    if (parseCGPSINFO(resp, lat, lon)) {
      Serial.printf("[GNSS] ✅ Fix acquired: lat=%.6f, lon=%.6f\n", lat, lon);
      return true;
    } else {
      Serial.println("[GNSS] CGPSINFO received but no valid fix data");
    }
  } 
  // Check for standard CGNSSINFO response
  else if (resp.indexOf("+CGNSSINFO:") != -1) {
    if (parseCGNSSINFO(resp, lat, lon)) {
      Serial.printf("[GNSS] ✅ Fix acquired: lat=%.6f, lon=%.6f\n", lat, lon);
      return true;
    } else {
      Serial.println("[GNSS] CGNSSINFO received but no valid fix data");
    }
  }
  else {
    Serial.println("[GNSS] No recognized GNSS response format");
  }
  
  return false;
}

bool ModemGNSS::parseCGNSSINFO(const String& resp, float& lat, float& lon) {
  Serial.println("[GNSS] Parsing CGNSSINFO response...");
  
  int start = resp.indexOf("+CGNSSINFO:");
  if (start == -1) {
    Serial.println("[GNSS] No +CGNSSINFO: found in response");
    return false;
  }

  // Find the colon after CGNSSINFO
  int colonPos = resp.indexOf(":", start);
  if (colonPos == -1) {
    Serial.println("[GNSS] No colon found after +CGNSSINFO");
    return false;
  }

  String data = resp.substring(colonPos + 1);
  data.trim();
  Serial.println("[GNSS] Data portion: '" + data + "'");

  // Split by commas
  String fields[15];  // Increased field count for safety
  int fieldIndex = 0;
  int lastComma = -1;

  for (int i = 0; i <= data.length(); i++) {
    if (i == data.length() || data[i] == ',') {
      if (fieldIndex < 15) {
        fields[fieldIndex] = data.substring(lastComma + 1, i);
        fields[fieldIndex].trim();
        Serial.println("[GNSS] Field[" + String(fieldIndex) + "]: '" + fields[fieldIndex] + "'");
        fieldIndex++;
      }
      lastComma = i;
    }
  }

  // Check if we have enough fields for the actual CGNSSINFO format
  // Format: +CGNSSINFO: status,satellites,,,, lat,N/S,lon,E/W,date,time,alt,speed,course,...
  if (fieldIndex < 9) {
    Serial.println("[GNSS] Insufficient fields in CGNSSINFO response (need at least 9, got " + String(fieldIndex) + ")");
    return false;
  }

  // Check for coordinate data in fields 5,6,7,8 (0-indexed)
  if (fields[5].length() == 0 || fields[7].length() == 0) {
    Serial.println("[GNSS] Empty coordinate fields - no fix available");
    return false;
  }

  // Parse coordinates from the correct field positions
  float rawLat = fields[5].toFloat();  // Field 5: latitude
  char ns = (fields[6].length() > 0) ? fields[6].charAt(0) : 'N';  // Field 6: N/S
  float rawLon = fields[7].toFloat();  // Field 7: longitude  
  char ew = (fields[8].length() > 0) ? fields[8].charAt(0) : 'E';  // Field 8: E/W

  Serial.printf("[GNSS] Raw coordinates: lat=%.1f%c, lon=%.1f%c\n", rawLat, ns, rawLon, ew);

  // Validate coordinate ranges (basic sanity check for decimal degrees)
  if (rawLat == 0.0 || rawLon == 0.0) {
    Serial.printf("[GNSS] Zero coordinate values: lat=%.6f, lon=%.6f\n", rawLat, rawLon);
    return false;
  }

  // Convert from DDMM.MMMM format to decimal degrees
  // DDMM.MMMM = degrees + (minutes/60)
  lat = floor(rawLat / 100.0) + fmod(rawLat, 100.0) / 60.0;
  lon = floor(rawLon / 100.0) + fmod(rawLon, 100.0) / 60.0;
  
  // Apply hemisphere indicators
  if (ns == 'S') lat = -lat;
  if (ew == 'W') lon = -lon;

  Serial.printf("[GNSS] Converted coordinates: lat=%.6f, lon=%.6f\n", lat, lon);

  // Final sanity check on converted coordinates
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    Serial.printf("[GNSS] Converted coordinates out of valid range\n");
    return false;
  }

  return true;
}

bool ModemGNSS::parseCGPSINFO(const String& resp, float& lat, float& lon) {
  Serial.println("[GNSS] Parsing CGPSINFO response (Waveshare format)...");
  
  int start = resp.indexOf("+CGPSINFO:");
  if (start == -1) {
    Serial.println("[GNSS] No +CGPSINFO: found in response");
    return false;
  }

  // Find the colon after CGPSINFO
  int colonPos = resp.indexOf(":", start);
  if (colonPos == -1) {
    Serial.println("[GNSS] No colon found after +CGPSINFO");
    return false;
  }

  String data = resp.substring(colonPos + 1);
  data.trim();
  Serial.println("[GNSS] Data portion: '" + data + "'");

  // CGPSINFO format: +CGPSINFO: lat,N/S,lon,E/W,date,UTC time,alt,speed,course
  String fields[10];
  int fieldIndex = 0;
  int lastComma = -1;

  for (int i = 0; i <= data.length(); i++) {
    if (i == data.length() || data[i] == ',') {
      if (fieldIndex < 10) {
        fields[fieldIndex] = data.substring(lastComma + 1, i);
        fields[fieldIndex].trim();
        Serial.println("[GNSS] Field[" + String(fieldIndex) + "]: '" + fields[fieldIndex] + "'");
        fieldIndex++;
      }
      lastComma = i;
    }
  }

  // Check if we have enough fields and if coordinates are present
  if (fieldIndex < 4) {
    Serial.println("[GNSS] Insufficient fields in CGPSINFO response (need at least 4, got " + String(fieldIndex) + ")");
    return false;
  }

  // Check for empty coordinate fields (indicates no fix)
  if (fields[0].length() == 0 || fields[2].length() == 0) {
    Serial.println("[GNSS] Empty coordinate fields - no fix available");
    return false;
  }

  // Parse coordinates from CGPSINFO format: lat,N/S,lon,E/W,date,time,...
  float rawLat = fields[0].toFloat();  // Field 0: latitude
  char ns = (fields[1].length() > 0) ? fields[1].charAt(0) : 'N';  // Field 1: N/S
  float rawLon = fields[2].toFloat();  // Field 2: longitude
  char ew = (fields[3].length() > 0) ? fields[3].charAt(0) : 'E';  // Field 3: E/W

  Serial.printf("[GNSS] Raw coordinates: lat=%.6f%c, lon=%.6f%c\n", rawLat, ns, rawLon, ew);

  // Validate coordinate ranges
  if (rawLat == 0.0 || rawLon == 0.0) {
    Serial.println("[GNSS] Zero coordinates - no fix available");
    return false;
  }

  // Convert from DDMM.MMMM format to decimal degrees
  // DDMM.MMMM = degrees + (minutes/60)
  lat = floor(rawLat / 100.0) + fmod(rawLat, 100.0) / 60.0;
  lon = floor(rawLon / 100.0) + fmod(rawLon, 100.0) / 60.0;
  
  // Apply hemisphere indicators
  if (ns == 'S') lat = -lat;
  if (ew == 'W') lon = -lon;

  Serial.printf("[GNSS] Converted coordinates: lat=%.6f, lon=%.6f\n", lat, lon);

  // Final sanity check
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    Serial.printf("[GNSS] Coordinates out of valid range\n");
    return false;
  }

  return true;
}

bool ModemGNSS::checkModemComms() {
  Serial.println("[GNSS] Testing modem communication...");
  
  // Clear any pending data first
  while (modem.available()) {
    modem.read();
  }
  
  modem.setTimeout(3000);
  modem.println("AT");
  delay(500);
  
  String resp = "";
  unsigned long start = millis();
  while (millis() - start < 2000) {
    while (modem.available()) {
      char c = modem.read();
      resp += c;
      if (resp.indexOf("OK") != -1) {
        Serial.println("[GNSS] ✅ Modem communication OK");
        return true;
      }
      if (resp.length() > 200) break; // Prevent memory overflow
    }
    delay(10);
  }
  
  Serial.println("[GNSS] AT response (first 100 chars): " + resp.substring(0, min(100, (int)resp.length())));
  
  // If we see NMEA data, the modem is responsive even if AT didn't work perfectly
  if (resp.indexOf("$GP") != -1 || resp.indexOf("GNSS") != -1) {
    Serial.println("[GNSS] ✅ Modem responsive (GNSS data detected)");
    return true;
  }
  
  Serial.println("[GNSS] ⚠️ Modem communication unclear, continuing anyway...");
  return true; // Don't block on this test
}

void ModemGNSS::printStatus() {
  Serial.println("[GNSS] === COMPREHENSIVE GNSS STATUS ===");
  
  modem.setTimeout(3000);
  
  // Check basic AT communication first
  if (!checkModemComms()) {
    Serial.println("[GNSS] ❌ Cannot communicate with modem - check wiring/power");
    return;
  }
  
  // Power status
  modem.println("AT+CGNSSPWR?");
  delay(500);
  String powerResp = modem.readString();
  Serial.println("[GNSS] Power status: " + powerResp);
  
  // Mode/constellation status
  modem.println("AT+CGNSSMODE?");
  delay(500);
  String modeResp = modem.readString();
  Serial.println("[GNSS] Mode/constellation: " + modeResp);
  
  // Skip antenna status (command not supported by SIM7670G)
  Serial.println("[GNSS] Antenna status: N/A (command not supported)");
  
  // Signal strength/satellite info (try Waveshare command first)
  modem.println("AT+CGPSINFO");
  delay(500);
  String infoResp = modem.readString();
  Serial.println("[GNSS] CGPSINFO response: " + infoResp);
  
  // Also try standard command
  modem.println("AT+CGNSSINFO");
  delay(500);
  String gnssResp = modem.readString();
  Serial.println("[GNSS] CGNSSINFO response: " + gnssResp);
  
  // Check for NMEA output
  modem.println("AT+CGNSSPORTSWITCH?");
  delay(500);
  String portResp = modem.readString();
  Serial.println("[GNSS] Port switch: " + portResp);
  
  Serial.println("[GNSS] === STATUS CHECK COMPLETE ===");
}

void ModemGNSS::printSatellites() {
  Serial.println("[GNSS] === SATELLITE INFORMATION ===");
  
  modem.setTimeout(3000);
  
  // Get current GNSS information (this shows satellite count in some fields)
  modem.println("AT+CGNSSINFO");
  delay(1000);
  String infoResp = modem.readString();
  Serial.println("[GNSS] GNSS info: " + infoResp);
  
  // Check if NMEA sentences are available
  Serial.println("[GNSS] Checking for NMEA data...");
  delay(1000);
  String nmeaData = "";
  unsigned long start = millis();
  while (millis() - start < 2000) {
    while (modem.available()) {
      char c = modem.read();
      nmeaData += c;
      if (nmeaData.length() > 500) break; // Limit data size
    }
    if (nmeaData.length() > 50) break;
    delay(100);
  }
  
  if (nmeaData.length() > 0) {
    Serial.println("[GNSS] NMEA data received: " + nmeaData.substring(0, min(200, (int)nmeaData.length())));
  } else {
    Serial.println("[GNSS] No NMEA data available");
  }
  
  Serial.println("[GNSS] === SATELLITE INFO COMPLETE ===");
}