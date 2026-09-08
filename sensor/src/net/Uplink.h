// Uplink.h
#pragma once

// POSTs a JSON body to url. Returns true on a 2xx response. Blocks up to 3 s.
bool postJson(const char* url, const char* payload);
