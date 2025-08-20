// ModemHttp.h
#pragma once
#include <Arduino.h>
#include "../config.h"

class ModemHttp {
public:
  ModemHttp(HardwareSerial& serial);
  void begin(uint32_t baud, int rxPin, int txPin);
  bool post(const char* url, const String& body);

private:
  HardwareSerial& modem;
};
