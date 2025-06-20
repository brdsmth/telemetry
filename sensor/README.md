# ESP32 Watermark

A simple ESP32 project that prints "Hello World" to the serial monitor.

## Prerequisites
- PlatformIO CLI (`pip install platformio`)
- ESP32 development board
- USB cable

## Commands

```bash
make build     # Compile the project
make upload    # Upload to ESP32
make monitor   # Open serial monitor
make clean     # Clean build files
make all       # Build, upload and monitor
```

## Serial Configuration
- Baud Rate: 115200 


## Developer Documentation

### ESP32 Memory Types

The ESP32 has several types of memory that serve different purposes:

#### Volatile Memory
- **RAM (SRAM)**: ~520KB
  - Main runtime memory
  - Cleared on reset or power cycle
  - Used for program execution and temporary data storage

#### Non-Volatile Memory
- **Flash Memory**: Up to 4MB
  - Stores the program code
  - Persists after power off
  - Cleared during firmware flashing
  
- **NVS (Non-volatile Storage)**
  - Dedicated portion of flash memory
  - Persists through resets AND firmware updates
  - Perfect for storing configuration data, calibration values, etc.
  - Accessible using the Preferences library
  - Must be explicitly cleared (doesn't clear with normal firmware updates)

#### Memory Behavior
- Reset button: Clears RAM but preserves Flash and NVS
- Firmware update: Clears program Flash but preserves NVS
- Full flash erase: Clears all memory types 

### ESP32 Memory Access

#### RAM (SRAM)
- Standard variable declarations use RAM automatically
```cpp
// Regular variables use RAM
int normalVar = 42;
String ramString = "stored in RAM";
```

#### Flash Memory
1. **PROGMEM** - For storing constants in flash:
```cpp
#include <pgmspace.h>
// Store string in flash instead of RAM
const char flashString[] PROGMEM = "stored in flash";
```

2. **SPIFFS (SPI Flash File System)**
```cpp
#include <SPIFFS.h>

void setupSPIFFS() {
    if(!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    
    // Write to file
    File file = SPIFFS.open("/data.txt", "w");
    file.println("Hello SPIFFS!");
    file.close();
    
    // Read from file
    file = SPIFFS.open("/data.txt", "r");
    String content = file.readString();
    file.close();
}
```

#### NVS (Non-volatile Storage)
- Best for storing configuration and small persistent data
```cpp
#include <Preferences.h>

Preferences preferences;

void nvs_example() {
    // Open namespace "my-app" in RW mode (false = RW, true = RO)
    preferences.begin("my-app", false);
    
    // Store values
    preferences.putInt("counter", 42);
    preferences.putFloat("temperature", 23.5);
    preferences.putString("device_name", "ESP32_1");
    
    // Read values (second parameter is default value if key not found)
    int counter = preferences.getInt("counter", 0);
    float temp = preferences.getFloat("temperature", 0.0);
    String name = preferences.getString("device_name", "unknown");
    
    // Clear specific key
    preferences.remove("counter");
    
    // Clear all keys in "my-app" namespace
    preferences.clear();
    
    // Close namespace
    preferences.end();
}
```

### Memory Management Best Practices
1. Use PROGMEM for large constant data (strings, arrays)
2. Use SPIFFS for larger files and data sets
3. Use NVS for configuration and small persistent data
4. Free dynamic memory when no longer needed
```cpp
// Example of proper dynamic memory management
uint8_t* buffer = (uint8_t*)malloc(1024);
// ... use buffer ...
free(buffer);
```

5. Check available memory during runtime
```cpp
void checkMemory() {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
}
``` 