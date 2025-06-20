#include <Arduino.h>
#include "esp_heap_caps.h"
#include "memory.h"
#include "logger.h"

void printMemoryStats() {
    logkv("Free heap", esp_get_free_heap_size());
    logkv("Internal heap", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    logkv("SPI RAM heap", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}