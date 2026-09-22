// Uplink.h
#pragma once

// POSTs a JSON body to url. Blocks up to 3 s. Returns the HTTP status code,
// or a negative HTTPClient error code if no response was received.
int postJson(const char* url, const char* payload);
