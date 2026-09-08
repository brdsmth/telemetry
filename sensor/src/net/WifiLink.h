// WifiLink.h
#pragma once

// Scans for ssid, then connects. Blocks up to ~30 s. Returns true on success.
bool connectToWiFi(const char* ssid, const char* password);
