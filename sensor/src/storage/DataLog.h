// DataLog.h
//
// Append-only CSV log on SPIFFS.
#pragma once
#include <Arduino.h>
#include <FS.h>

class DataLog {
public:
    DataLog(const char* path, const char* header);

    // Mounts SPIFFS (formatting on first use) and creates the file with its
    // header if it does not exist yet. An existing file whose first line does
    // not match the header (older firmware, different columns) is recreated.
    bool begin();

    bool append(const String& line);

    // Deletes the file and recreates it with just the header.
    bool clear();

    bool exists() const;
    size_t size() const;
    File openForRead() const;

private:
    bool writeHeader();
    bool headerMatches() const;

    const char* path_;
    const char* header_;
};
