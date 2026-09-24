#include "DataLog.h"

#include <LittleFS.h>

#include "logger.h"

DataLog::DataLog(const char* path, const char* header)
: path_(path), header_(header) {}

bool DataLog::begin() {
    logln("-----> Initializing LittleFS...");

    if (!LittleFS.begin(true)) {
        logln("-----> ERROR: LittleFS mount failed!");
        return false;
    }

    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    logln("-----> LittleFS mounted successfully");
    logln("-----> Total: " + String(totalBytes) + " bytes");
    logln("-----> Used: " + String(usedBytes) + " bytes");
    logln("-----> Free: " + String(totalBytes - usedBytes) + " bytes");

    if (LittleFS.exists(path_)) {
        if (headerMatches()) {
            logln("-----> Log file already exists");
            return true;
        }
        logln("-----> Log file has a different header, recreating it");
        LittleFS.remove(path_);
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
    File file = LittleFS.open(path_, FILE_APPEND);
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
    if (LittleFS.exists(path_)) {
        LittleFS.remove(path_);
    }
    return writeHeader();
}

bool DataLog::exists() const {
    return LittleFS.exists(path_);
}

size_t DataLog::size() const {
    File file = LittleFS.open(path_, FILE_READ);
    if (!file) return 0;
    size_t s = file.size();
    file.close();
    return s;
}

File DataLog::openForRead() const {
    return LittleFS.open(path_, FILE_READ);
}

bool DataLog::writeHeader() {
    File file = LittleFS.open(path_, FILE_WRITE);
    if (!file) return false;
    file.println(header_);
    file.close();
    return true;
}

bool DataLog::headerMatches() const {
    File file = LittleFS.open(path_, FILE_READ);
    if (!file) return false;
    String first = file.readStringUntil('\n');
    file.close();
    first.trim();
    return first == header_;
}
