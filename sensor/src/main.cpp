#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(5000);
    WiFi.mode(WIFI_STA);
    Serial.println(WiFi.macAddress());
}

void loop() {
    Serial.println("Hello World!");
    delay(5000);
} 