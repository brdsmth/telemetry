// ModemGNSS.h
#pragma once
#include <Arduino.h>

class ModemGNSS {
public:
  ModemGNSS(HardwareSerial& serial);

  void start();   // power on GNSS (non-blocking)
  void stop();    // power off
  bool pollFix(float& lat, float& lon); // call in loop, returns true if fix found
  
  // Diagnostic functions
  void printStatus();     // Print comprehensive GNSS status
  void printSatellites(); // Print satellite information
  bool checkModemComms(); // Verify modem communication

private:
  HardwareSerial& modem;
  bool parseCGNSSINFO(const String& resp, float& lat, float& lon);
  bool parseCGPSINFO(const String& resp, float& lat, float& lon); // Waveshare format
};