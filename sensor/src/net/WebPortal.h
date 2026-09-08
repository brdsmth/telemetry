// WebPortal.h
//
// Small HTTP UI for inspecting and clearing the on-device CSV log.
#pragma once

class DataLog;

namespace web_portal {

void begin(DataLog& log);

// Must be called frequently from loop() to keep the server responsive.
void handle();

}  // namespace web_portal
