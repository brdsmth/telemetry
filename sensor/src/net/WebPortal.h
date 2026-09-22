// WebPortal.h
//
// Small HTTP UI for diagnostics and for inspecting and clearing the on-device
// CSV log.
//
//   /          live diagnostics page
//   /status    diagnostics as JSON
//   /view      CSV log in the browser
//   /download  CSV log as a file
//   /clear     wipe the CSV log
#pragma once

class DataLog;
struct Diagnostics;

namespace web_portal {

void begin(DataLog& log, const Diagnostics& diag);

// Must be called frequently from loop() to keep the server responsive.
void handle();

}  // namespace web_portal
