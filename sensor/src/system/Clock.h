// Clock.h
//
// Wall-clock time via NTP, with a millis() fallback before the first sync.
#pragma once
#include <Arduino.h>
#include <time.h>

namespace clock_sync {

// Blocks up to ~10 s waiting for NTP. Requires WiFi to be connected.
void syncNTP();

// True once NTP has produced a plausible date.
bool isSynced();

// Unix seconds, or 0 if the clock has not synced.
time_t unixTime();

// "YYYY-MM-DD HH:MM:SS" when synced, otherwise seconds since boot.
String timestamp();

}  // namespace clock_sync
