#include "DataLog.h"

#include <SPIFFS.h>

#include "logger.h"

DataLog::DataLog(const char* path, const char* header)
: path_(path), header_(header) {}

bool DataLog::begin() {
    logln("-----> Initializing SPIFFS...");

    if (!SPIFFS.begin(true)) {
        logln("-----> ERROR: SPIFFS mount failed!");
        return false;
    }

    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    logln("-----> SPIFFS mounted successfully");
    logln("-----> Total: " + String(totalBytes) + " bytes");
    logln("-----> Used: " + String(usedBytes) + " bytes");
    logln("-----> Free: " + String(totalBytes - usedBytes) + " bytes");

    if (SPIFFS.exists(path_)) {
        if (headerMatches()) {
            logln("-----> Log file already exists");
            return true;
        }
        logln("-----> Log file has a different header, recreating it");
        SPIFFS.remove(path_);
    }

    logln("-----> Creating new log file...");
    if (!writeHeader()) {
        logln("-----> ERROR: Could not create log file!");
        return false;
    }
    logln("-----> Log file created with header");
    return true;
}

bool DataLog::append(const String& line) {
    File file = SPIFFS.open(path_, FILE_APPEND);
    if (!file) {
        logln("-----> ERROR: Could not open log file for writing!");
        return false;
    }
    file.println(line);
    file.close();
    logln("-----> Data logged: " + line);
    return true;
}

bool DataLog::clear() {
    if (SPIFFS.exists(path_)) {
        SPIFFS.remove(path_);
    }
    return writeHeader();
}

bool DataLog::exists() const {
    return SPIFFS.exists(path_);
}

size_t DataLog::size() const {
    File file = SPIFFS.open(path_, FILE_READ);
    if (!file) return 0;
    size_t s = file.size();
    file.close();
    return s;
}

File DataLog::openForRead() const {
    return SPIFFS.open(path_, FILE_READ);
}

bool DataLog::writeHeader() {
    File file = SPIFFS.open(path_, FILE_WRITE);
    if (!file) return false;
    file.println(header_);
    file.close();
    return true;
}

bool DataLog::headerMatches() const {
    File file = SPIFFS.open(path_, FILE_READ);
    if (!file) return false;
    String first = file.readStringUntil('\n');
    file.close();
    first.trim();
    return first == header_;
}
