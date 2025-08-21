#include "i2c_scanner.h"

void runI2CScanner(TwoWire &wire, int sda, int scl) {
    wire.begin(sda, scl);
    Serial.println("I2C scanner starting...");

    byte error, address;
    int nDevices = 0;

    for (address = 1; address < 127; address++) {
        wire.beginTransmission(address);
        error = wire.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            Serial.println();
            nDevices++;
        }
    }

    if (nDevices == 0) {
        Serial.println("No I2C devices found\n");
    } else {
        Serial.println("Scan complete\n");
    }
}
