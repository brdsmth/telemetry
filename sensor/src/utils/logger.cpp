#include <Arduino.h>
#include "logger.h"

void logln(String msg) {
    Serial.println(msg);
}

void logkv(const char* key, const char* value) {
    Serial.print(key);
    Serial.print(":\t\t");
    Serial.println(value);
}

void logkv(const char* key, int value) {
    Serial.print(key);
    Serial.print(":\t\t");
    Serial.println(value);
}