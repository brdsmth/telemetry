#include "Clock.h"

#include "logger.h"

namespace clock_sync {

static const char* kNtpServer        = "pool.ntp.org";
static const long  kGmtOffsetSec     = 0;
static const int   kDaylightOffsetSec = 0;
static const time_t kEarliestValid   = 1577836800;  // 2020-01-01

void syncNTP() {
    logln("-----> Syncing time with NTP server...");
    configTime(kGmtOffsetSec, kDaylightOffsetSec, kNtpServer);

    int retry = 0;
    const int maxRetries = 10;
    while (time(nullptr) < 100000 && retry < maxRetries) {
        delay(1000);
        Serial.print(".");
        retry++;
    }

    if (retry < maxRetries) {
        logln("\n-----> Time synced successfully!");
        time_t now = time(nullptr);
        logln("-----> Current time: " + String(ctime(&now)));
    } else {
        logln("\n-----> WARNING: Time sync failed, will use millis() fallback");
    }
}

bool isSynced() {
    return time(nullptr) > kEarliestValid;
}

time_t unixTime() {
    return isSynced() ? time(nullptr) : 0;
}

String timestamp() {
    if (!isSynced()) return String(millis() / 1000);

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

}  // namespace clock_sync
